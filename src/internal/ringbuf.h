#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <threads.h>

#include "texec/base.h"

typedef struct texec_ringbuf {
  mtx_t mtx;
  cnd_t not_empty;
  cnd_t not_full;
  const texec_allocator_t* alloc;
  uintptr_t* buf;
  size_t capacity;
  size_t count;
  size_t head; // logical front
  size_t tail; // logical back (one past last element)
  bool closed;
} texec_ringbuf_t;

typedef enum texec_ringbuf_end {
  TEXEC_RINGBUF_FRONT,
  TEXEC_RINGBUF_BACK
} texec_ringbuf_end_t;

texec_status_t texec_ringbuf_init(texec_ringbuf_t* rb, size_t capacity, const texec_allocator_t* alloc);
texec_status_t texec_ringbuf_destroy(texec_ringbuf_t* rb);
void texec_ringbuf_close(texec_ringbuf_t* rb);

texec_status_t texec_ringbuf_try_push(texec_ringbuf_t* rb, texec_ringbuf_end_t end, uintptr_t item);
texec_status_t texec_ringbuf_try_pop(texec_ringbuf_t* rb, texec_ringbuf_end_t end, uintptr_t* out_item);

texec_status_t texec_ringbuf_push(texec_ringbuf_t* rb, texec_ringbuf_end_t end, uintptr_t item);
texec_status_t texec_ringbuf_pop(texec_ringbuf_t* rb, texec_ringbuf_end_t end, uintptr_t* out_item);
