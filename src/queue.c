#include "texec/queue.h"

#include <stddef.h>

#include "internal/allocator.h"
#include "internal/ringbuf.h"

struct texec_queue {
  const texec_allocator_t* alloc;
  texec_ringbuf_t rb;
};

texec_status_t texec_queue_create(const texec_queue_create_info_t* info, const texec_allocator_t* alloc, texec_queue_t** out_q) {
  if (!out_q) return TEXEC_STATUS_INVALID_ARGUMENT;

  *out_q = NULL;

  if (!info || info->header.type != TEXEC_STRUCT_TYPE_QUEUE_CREATE_INFO || info->capacity == 0) {
    return TEXEC_STATUS_INVALID_ARGUMENT;
  }

  if (!alloc) alloc = texec_get_default_allocator();

  texec_queue_t* q = texec_allocate(alloc, sizeof(*q), _Alignof(texec_queue_t));
  if (!q) return TEXEC_STATUS_OUT_OF_MEMORY;

  q->alloc = alloc;
  texec_status_t st = texec_ringbuf_init(&q->rb, info->capacity, alloc);
  if (st != TEXEC_STATUS_OK) {
    texec_free(alloc, q, sizeof(*q), _Alignof(texec_queue_t));
  } else {
    *out_q = q;
  }
  return st;
}

texec_status_t texec_queue_destroy(texec_queue_t* q) {
  if (!q) return TEXEC_STATUS_INVALID_ARGUMENT;

  texec_status_t st = texec_ringbuf_destroy(&q->rb);
  if (st != TEXEC_STATUS_OK) return st;

  texec_free(q->alloc, q, sizeof(*q), _Alignof(texec_queue_t));

  return TEXEC_STATUS_OK;
}

void texec_queue_close(texec_queue_t* q) {
  if (!q) return;
  texec_ringbuf_close(&q->rb);
}

texec_status_t texec_queue_try_push(texec_queue_t* q, uintptr_t item) {
  if (!q) return TEXEC_STATUS_INVALID_ARGUMENT;
  return texec_ringbuf_try_push(&q->rb, TEXEC_RINGBUF_BACK, item);
}

texec_status_t texec_queue_try_pop(texec_queue_t* q, uintptr_t* out_item) {
  if (!q) return TEXEC_STATUS_INVALID_ARGUMENT;
  return texec_ringbuf_try_pop(&q->rb, TEXEC_RINGBUF_FRONT, out_item);
}

texec_status_t texec_queue_push(texec_queue_t* q, uintptr_t item) {
  if (!q) return TEXEC_STATUS_INVALID_ARGUMENT;
  return texec_ringbuf_push(&q->rb, TEXEC_RINGBUF_BACK, item);
}

texec_status_t texec_queue_pop(texec_queue_t* q, uintptr_t* out_item) {
  if (!q) return TEXEC_STATUS_INVALID_ARGUMENT;
  return texec_ringbuf_pop(&q->rb, TEXEC_RINGBUF_FRONT, out_item);
}
