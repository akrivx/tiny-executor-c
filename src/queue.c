#include "texec/queue.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "internal/allocator.h"

struct texec_queue {
  mtx_t mtx;
  cnd_t not_empty;
  cnd_t not_full;
  const texec_allocator_t* alloc;
  uintptr_t* buf;
  size_t head;
  size_t tail;
  size_t count;
  size_t capacity;
  bool closed;
};

static inline const texec_queue_create_allocator_info_t* find_queue_alloc_info(const texec_queue_create_info_t* info) {
  return texec_structure_find(info->header.next, TEXEC_STRUCTURE_TYPE_QUEUE_CREATE_ALLOCATOR_INFO);
}

static inline bool queue_init_cnds(texec_queue_t* q) {
  if (cnd_init(&q->not_empty) != thrd_success) return false;

  if (cnd_init(&q->not_full) != thrd_success) {
    cnd_destroy(&q->not_empty);
    return false;
  }

  return true;
}

static inline bool queue_init_sync_prims(texec_queue_t* q) {
  if (mtx_init(&q->mtx, mtx_plain) != thrd_success) return false;

  if (!queue_init_cnds(q)) {
    mtx_destroy(&q->mtx);
    return false;
  }

  return true;
}

static inline texec_status_t queue_init(texec_queue_t* q, size_t capacity, const texec_allocator_t* alloc) {
  uintptr_t* qbuf = texec_allocate(alloc, capacity * sizeof(uintptr_t), _Alignof(uintptr_t));
  if (!qbuf) return TEXEC_STATUS_OUT_OF_MEMORY;

  if (!queue_init_sync_prims(q)) {
    texec_free(alloc, qbuf, capacity * sizeof(uintptr_t), _Alignof(uintptr_t));
    return TEXEC_STATUS_INTERNAL_ERROR;
  }

  q->alloc = alloc;
  q->buf = qbuf;
  q->head = 0;
  q->tail = 0;
  q->count = 0;
  q->capacity = capacity;
  q->closed = false;

  return TEXEC_STATUS_OK;
}

static inline texec_status_t queue_unlock_return(texec_queue_t* q, texec_status_t st) {
  mtx_unlock(&q->mtx);
  return st;
}

static inline void queue_push_item(texec_queue_t* q, uintptr_t item) {
  q->buf[q->tail] = item;
  q->tail = (q->tail + 1) % q->capacity;
  q->count++;
}

static inline uintptr_t queue_pop_item(texec_queue_t* q) {
  uintptr_t item = q->buf[q->head];
  q->head = (q->head + 1) % q->capacity;
  q->count--;
  return item;
}

static inline bool queue_is_full(const texec_queue_t* q) {
  return q->count == q->capacity;
}

static inline bool queue_is_empty(const texec_queue_t* q) {
  return q->count == 0;
}

static inline texec_status_t queue_push_impl(texec_queue_t* q, uintptr_t item, bool wait_not_full) {
  if (!q) return TEXEC_STATUS_INVALID_ARGUMENT;

  mtx_lock(&q->mtx);

  while (!q->closed && queue_is_full(q)) {
    if (!wait_not_full) {
      return queue_unlock_return(q, TEXEC_STATUS_REJECTED);
    }
    cnd_wait(&q->not_full, &q->mtx);
  }

  if (q->closed) {
    return queue_unlock_return(q, TEXEC_STATUS_CLOSED);
  }

  queue_push_item(q, item);

  cnd_signal(&q->not_empty);
  mtx_unlock(&q->mtx);
  return TEXEC_STATUS_OK;
}

static inline texec_status_t queue_pop_impl(texec_queue_t* q, uintptr_t* out_item, bool wait_not_empty) {
  if (!q || !out_item) return TEXEC_STATUS_INVALID_ARGUMENT;

  mtx_lock(&q->mtx);

  while (!q->closed && queue_is_empty(q)) {
    if (!wait_not_empty) {
      return queue_unlock_return(q, TEXEC_STATUS_REJECTED);
    }
    cnd_wait(&q->not_empty, &q->mtx);
  }

  if (q->closed && queue_is_empty(q)) {
    return queue_unlock_return(q, TEXEC_STATUS_CLOSED);
  }

  *out_item = queue_pop_item(q);

  cnd_signal(&q->not_full);
  mtx_unlock(&q->mtx);
  return TEXEC_STATUS_OK;
}

texec_status_t texec_queue_create(const texec_queue_create_info_t* info, texec_queue_t** out_q) {
  if (!out_q) return TEXEC_STATUS_INVALID_ARGUMENT;

  *out_q = NULL;

  if (!info || info->header.type != TEXEC_STRUCTURE_TYPE_QUEUE_CREATE_INFO || info->capacity == 0) {
    return TEXEC_STATUS_INVALID_ARGUMENT;
  }

  const texec_queue_create_allocator_info_t* alloc_info = find_queue_alloc_info(info);
  if (alloc_info && !alloc_info->allocator) return TEXEC_STATUS_INVALID_ARGUMENT;
  const texec_allocator_t* alloc = alloc_info ? alloc_info->allocator : texec_get_default_allocator();

  texec_queue_t* q = texec_allocate(alloc, sizeof(*q), _Alignof(texec_queue_t));
  if (!q) return TEXEC_STATUS_OUT_OF_MEMORY;

  texec_status_t st = queue_init(q, info->capacity, alloc);
  if (st != TEXEC_STATUS_OK) {
    texec_free(alloc, q, sizeof(*q), _Alignof(texec_queue_t));
  } else {
    *out_q = q;
  }
  return st;
}

texec_status_t texec_queue_destroy(texec_queue_t* q) {
  if (!q) return TEXEC_STATUS_INVALID_ARGUMENT;

  mtx_lock(&q->mtx);
  const bool closed = q->closed;
  mtx_unlock(&q->mtx);

  if (!closed) return TEXEC_STATUS_BUSY;

  cnd_destroy(&q->not_full);
  cnd_destroy(&q->not_empty);
  mtx_destroy(&q->mtx);

  texec_free(q->alloc, q->buf, q->capacity * sizeof(uintptr_t), _Alignof(uintptr_t));
  texec_free(q->alloc, q, sizeof(*q), _Alignof(texec_queue_t));
}

void texec_queue_close(texec_queue_t* q) {
  if (!q) return;
  mtx_lock(&q->mtx);
  if (!q->closed) {
    cnd_broadcast(&q->not_empty);
    cnd_broadcast(&q->not_full);
  }
  q->closed = true;
  mtx_unlock(&q->mtx);
}

texec_status_t texec_queue_try_push(texec_queue_t* q, uintptr_t item) {
  return queue_push_impl(q, item, false);
}

texec_status_t texec_queue_try_pop(texec_queue_t* q, uintptr_t* out_item) {
  return queue_pop_impl(q, out_item, false);
}

texec_status_t texec_queue_push(texec_queue_t* q, uintptr_t item) {
  return queue_push_impl(q, item, true);
}

texec_status_t texec_queue_pop(texec_queue_t* q, uintptr_t* out_item) {
  return queue_pop_impl(q, out_item, true);
}
