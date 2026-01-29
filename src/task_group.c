#include "texec/task_group.h"

#include <assert.h>
#include <threads.h>
#include <string.h>

#include "internal/allocator.h"

static const size_t TASK_GROUP_DEFAULT_CAPACITY = 8;
static const float TASK_GROUP_EXPANSION_FACTOR = 1.5f;

struct texec_task_group {
  mtx_t mtx;
  const texec_allocator_t* alloc;
  texec_task_handle_t** handles;
  size_t count;
  size_t capacity;
  bool closed;
};

static inline texec_task_handle_t** alloc_task_handles(const texec_allocator_t* alloc, size_t n) {
  return texec_allocate(alloc, n * sizeof(texec_task_handle_t*), _Alignof(texec_task_handle_t*));
}

static inline void free_task_handles(const texec_allocator_t* alloc, texec_task_handle_t** handles, size_t n) {
  texec_free(alloc, handles, n * sizeof(*handles), _Alignof(texec_task_handle_t*));
}

static inline bool task_group_ensure_capacity(texec_task_group_t* g, size_t min_capacity) {
  if (g->capacity >= min_capacity) return true;

  size_t new_cap = g->capacity;
  while (new_cap < min_capacity) {
    size_t next = (size_t)(new_cap * TASK_GROUP_EXPANSION_FACTOR);
    if (next < new_cap) return false; // overflow
    new_cap = next;
  }

  texec_task_handle_t** new_buf = alloc_task_handles(g->alloc, new_cap);
  if (!new_buf) return false;

  if (g->handles && g->count) {
    memcpy(new_buf, g->handles, g->count * sizeof(*g->handles));
  }

  if (g->handles) {
    free_task_handles(g->alloc, g->handles, g->capacity);
  }

  g->handles = new_buf;
  g->capacity = new_cap;
  return true;
}

static inline texec_status_t task_group_init(texec_task_group_t* g, size_t capacity, const texec_allocator_t* alloc) {
  assert(capacity != 0);

  if (mtx_init(&g->mtx, mtx_plain) != thrd_success) return TEXEC_STATUS_INTERNAL_ERROR;

  texec_task_handle_t** handles = alloc_task_handles(alloc, capacity);
  if (!handles) {
    mtx_destroy(&g->mtx);
    return TEXEC_STATUS_OUT_OF_MEMORY;
  }

  g->alloc = alloc;
  g->handles = handles;
  g->count = 0;
  g->capacity = capacity;
  g->closed = false;
  return TEXEC_STATUS_OK;
}

static inline texec_status_t task_group_unlock_return(texec_task_group_t* g, texec_status_t st) {
  mtx_unlock(&g->mtx);
  return st;
}

texec_status_t texec_task_group_create(const texec_task_group_create_info_t* info, const texec_allocator_t* alloc, texec_task_group_t** out_group) {
  if (!out_group) return TEXEC_STATUS_INVALID_ARGUMENT;
  *out_group = NULL;

  if (!info || info->header.type != TEXEC_STRUCT_TYPE_TASK_GROUP_CREATE_INFO) {
    return TEXEC_STATUS_INVALID_ARGUMENT;
  }

  if (!alloc) {
    alloc = texec_get_default_allocator();
  }

  texec_task_group_t* g = texec_allocate(alloc, sizeof(*g), _Alignof(texec_task_group_t));
  if (!g) return TEXEC_STATUS_OUT_OF_MEMORY;

  const size_t capacity = (info->capacity ? info->capacity : TASK_GROUP_DEFAULT_CAPACITY);
  texec_status_t st = task_group_init(g, capacity, alloc);
  if (st != TEXEC_STATUS_OK) {
    texec_free(alloc, g, sizeof(*g), _Alignof(texec_task_group_t));
  } else {
    *out_group = g;
  }
  return st;
}

void texec_task_group_destroy(texec_task_group_t* g) {
  if (!g) return;

  mtx_lock(&g->mtx);
  for (size_t i = 0; i < g->count; ++i) {
    texec_task_handle_release(g->handles[i]);
    g->handles[i] = NULL;
  }
  g->count = 0;
  mtx_unlock(&g->mtx);
  mtx_destroy(&g->mtx);

  if (g->handles) {
    free_task_handles(g->alloc, g->handles, g->capacity);
  }
  texec_free(g->alloc, g, sizeof(*g), _Alignof(texec_task_group_t));
}

texec_status_t texec_task_group_add(texec_task_group_t* g, texec_task_handle_t* h) {
  if (!g || !h) return TEXEC_STATUS_INVALID_ARGUMENT;

  mtx_lock(&g->mtx);

  if (g->closed) return task_group_unlock_return(g, TEXEC_STATUS_CLOSED);

  if (!task_group_ensure_capacity(g, g->count + 1)) {
    return task_group_unlock_return(g, TEXEC_STATUS_OUT_OF_MEMORY);
  }
  
  texec_status_t st = texec_task_handle_retain(h);
  if (st != TEXEC_STATUS_OK) return task_group_unlock_return(g, st);

  g->handles[g->count++] = h;

  mtx_unlock(&g->mtx);
  return TEXEC_STATUS_OK;
}

texec_status_t texec_task_group_wait(texec_task_group_t* g) {
  if (!g) return TEXEC_STATUS_INVALID_ARGUMENT;

  mtx_lock(&g->mtx);

  g->closed = true;

  texec_task_handle_t** handles = g->handles;
  const size_t count = g->count;
  const size_t capacity = g->capacity;

  g->handles = NULL;
  g->count = 0;
  g->capacity = 0;

  mtx_unlock(&g->mtx);

  for (size_t i = 0; i < count; ++i) {
    texec_task_handle_wait(handles[i]);
    texec_task_handle_release(handles[i]);
  }

  if (handles) {
    free_task_handles(g->alloc, handles, capacity);
  }

  return TEXEC_STATUS_OK;
}
