#include "texec/queue.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "allocator_internal.h"

struct texec_queue {
  size_t capacity;
  texec_queue_full_policy_t full_policy;
  texec_allocator_t* alloc;

  uintptr_t* buf;
  size_t head;
  size_t tail;
  size_t count;

  mtx_t mtx;
  cnd_t not_empty;
  cnd_t not_full;
  bool closed;
};

static const texec_queue_create_full_policy_info_t* find_queue_full_policy_info(const texec_queue_create_info_t* info) {
  return texec_structure_find(info->header.next, TEXEC_STRUCTURE_TYPE_QUEUE_CREATE_FULL_POLICY_INFO);
}

static const texec_queue_create_allocator_info_t* find_queue_alloc_info(const texec_queue_create_info_t* info) {
  return texec_structure_find(info->header.next, TEXEC_STRUCTURE_TYPE_QUEUE_CREATE_ALLOCATOR_INFO);
}

static bool init_queue_sync_prims(texec_queue_t* q) {
  if (mtx_init(&q->mtx, mtx_plain) == thrd_success) {
    if (cnd_init(&q->not_empty) == thrd_success) {
      if (cnd_init(&q->not_full) == thrd_success) {
        return true;
      }
      cnd_destroy(&q->not_empty);
    }
  }
  mtx_destroy(&q->mtx);
  return false;

}

static texec_status_t init_queue(texec_queue_t* q,
                                 size_t capacity,
                                 texec_queue_full_policy_t full_policy,
                                 texec_allocator_t* alloc) {
  uintptr_t* qbuf = texec__allocate(alloc, capacity * sizeof(uintptr_t), _Alignof(uintptr_t));
  if (!qbuf) return TEXEC_STATUS_OUT_OF_MEMORY;
  if (!init_queue_sync_prims(q)) {
    texec__free(alloc, qbuf, capacity * sizeof(uintptr_t), _Alignof(uintptr_t));
    return TEXEC_STATUS_INTERNAL_ERROR;
  }
  q->capacity = capacity;
  q->full_policy = full_policy;
  q->alloc = alloc;
  q->buf = qbuf;
  q->head = 0;
  q->tail = 0;
  q->count = 0;
  q->closed = false;
  return TEXEC_STATUS_OK;
}

static texec_status_t queue_unlock_return(texec_queue_t* q, texec_status_t st) {
  mtx_unlock(&q->mtx);
  return st;
}

static void queue_push_item(texec_queue_t* q, uintptr_t item) {
  q->buf[q->tail] = item;
  q->tail = (q->tail + 1) % q->capacity;
  q->count++;
}

static uintptr_t queue_pop_item(texec_queue_t* q) {
  uintptr_t item = q->buf[q->head];
  q->head = (q->head + 1) % q->capacity;
  q->count--;
  return item;
}

texec_status_t texec_queue_create(const texec_queue_create_info_t* info, texec_queue_t** out_q) {
  if (!out_q) return TEXEC_STATUS_INVALID_ARGUMENT;

  *out_q = NULL;

  if (!info || info->header.type != TEXEC_STRUCTURE_TYPE_QUEUE_CREATE_INFO || info->capacity == 0) {
    return TEXEC_STATUS_INVALID_ARGUMENT;
  }

  const texec_queue_create_full_policy_info_t* full_policy_info = find_queue_full_policy_info(info);
  const texec_queue_create_allocator_info_t* alloc_info = find_queue_alloc_info(info);

  const texec_queue_full_policy_t full_policy = full_policy_info ? full_policy_info->policy : TEXEC_QUEUE_FULL_BLOCK;
  texec_allocator_t* alloc = alloc_info ? alloc_info->allocator : texec__get_default_allocator();

  texec_queue_t* q = texec__allocate(alloc, sizeof(*q), _Alignof(texec_queue_t));
  if (!q) return TEXEC_STATUS_OUT_OF_MEMORY;

  texec_status_t st = init_queue(q, info->capacity, full_policy, alloc);
  if (st != TEXEC_STATUS_OK) {
    texec__free(alloc, q, sizeof(*q), _Alignof(texec_queue_t));
  } else {
    *out_q = q;
  }
  return st;
}

void texec_queue_destroy(texec_queue_t* q) {
  if (!q) return;

  texec_queue_close(q);

  cnd_destroy(&q->not_full);
  cnd_destroy(&q->not_empty);
  mtx_destroy(&q->mtx);

  texec__free(q->alloc, q->buf, sizeof(uintptr_t), _Alignof(uintptr_t));
  texec__free(q->alloc, q, sizeof(*q), _Alignof(texec_queue_t));
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
  if (!q) return TEXEC_STATUS_INVALID_ARGUMENT;
  mtx_lock(&q->mtx);
  if (q->closed) return queue_unlock_return(q, TEXEC_STATUS_SHUTDOWN);
  if (q->count == q->capacity) return queue_unlock_return(q, TEXEC_STATUS_NOT_READY);
  queue_push_item(q, item);
  cnd_signal(&q->not_empty);
  mtx_unlock(&q->mtx);
  return TEXEC_STATUS_OK;
}

texec_status_t texec_queue_try_pop(texec_queue_t* q, uintptr_t* out_item) {
  if (!q || !out_item) return TEXEC_STATUS_INVALID_ARGUMENT;
  mtx_lock(&q->mtx);
  if (q->count == 0) {
    return queue_unlock_return(q, q->closed ? TEXEC_STATUS_SHUTDOWN : TEXEC_STATUS_NOT_READY);
  }
  *out_item = queue_pop_item(q);
  cnd_signal(&q->not_full);
  mtx_unlock(&q->mtx);
  return TEXEC_STATUS_OK;
}

texec_status_t texec_queue_push(texec_queue_t* q, uintptr_t item) {
  if (!q) return TEXEC_STATUS_INVALID_ARGUMENT;
  mtx_lock(&q->mtx);
  if (q->closed) return queue_unlock_return(q, TEXEC_STATUS_SHUTDOWN);
  if (q->full_policy == TEXEC_QUEUE_FULL_REJECT) {
    if (q->count == q->capacity) return queue_unlock_return(q, TEXEC_STATUS_REJECTED);
  } else {
    while (!q->closed && q->count == q->capacity) cnd_wait(&q->not_full, &q->mtx);
    if (q->closed) return queue_unlock_return(q, TEXEC_STATUS_SHUTDOWN);
  }
  queue_push_item(q, item);
  cnd_signal(&q->not_empty);
  mtx_unlock(&q->mtx);
  return TEXEC_STATUS_OK;
}

texec_status_t texec_queue_pop(texec_queue_t* q, uintptr_t* out_item) {
  if (!q || !out_item) return TEXEC_STATUS_INVALID_ARGUMENT;
  mtx_lock(&q->mtx);
  while (!q->closed && q->count == 0) cnd_wait(&q->not_empty, &q->mtx);
  if (q->closed && q->count == 0) return queue_unlock_return(q, TEXEC_STATUS_SHUTDOWN);
  *out_item = queue_pop_item(q);
  cnd_signal(&q->not_full);
  mtx_unlock(&q->mtx);
  return TEXEC_STATUS_OK;
}
