#include "internal/ringbuf.h"
#include "internal/allocator.h"

#include <assert.h>

static inline bool ringbuf_init_cnds(texec_ringbuf_t* rb) {
  if (cnd_init(&rb->not_empty) != thrd_success) return false;

  if (cnd_init(&rb->not_full) != thrd_success) {
    cnd_destroy(&rb->not_empty);
    return false;
  }

  return true;
}

static inline bool ringbuf_init_sync_prims(texec_ringbuf_t* rb) {
  if (mtx_init(&rb->mtx, mtx_plain) != thrd_success) return false;

  if (!ringbuf_init_cnds(rb)) {
    mtx_destroy(&rb->mtx);
    return false;
  }

  return true;
}

static inline bool ringbuf_is_full(const texec_ringbuf_t* rb) {
  return rb->count == rb->capacity;
}

static inline bool ringbuf_is_empty(const texec_ringbuf_t* rb) {
  return rb->count == 0;
}

static inline texec_status_t ringbuf_unlock_return(texec_ringbuf_t* rb, texec_status_t st) {
  mtx_unlock(&rb->mtx);
  return st;
}

static inline size_t ringbuf_decr_wrap(const texec_ringbuf_t* rb, size_t i) {
  return i == 0 ? (rb->capacity - 1) : (i - 1);
}

static inline size_t ringbuf_incr_wrap(const texec_ringbuf_t* rb, size_t i) {
  return (i + 1) % rb->capacity;
}

static inline void ringbuf_push_front(texec_ringbuf_t* rb, uintptr_t item) {
  rb->head = ringbuf_decr_wrap(rb, rb->head);
  rb->buf[rb->head] = item;
  rb->count++;
}

static inline void ringbuf_push_back(texec_ringbuf_t* rb, uintptr_t item) {
  rb->buf[rb->tail] = item;
  rb->tail = ringbuf_incr_wrap(rb, rb->tail);
  rb->count++;
}

static inline uintptr_t ringbuf_pop_front(texec_ringbuf_t* rb) {
  assert(rb->count > 0);
  uintptr_t item = rb->buf[rb->head];
  rb->head = ringbuf_incr_wrap(rb, rb->head);
  rb->count--;
  return item;
}

static inline uintptr_t ringbuf_pop_back(texec_ringbuf_t* rb) {
  assert(rb->count > 0);
  rb->tail = ringbuf_decr_wrap(rb, rb->tail);
  uintptr_t item = rb->buf[rb->tail];
  rb->count--;
  return item;
}

static inline void ringbuf_push_end(texec_ringbuf_t* rb, texec_ringbuf_end_t end, uintptr_t item) {
  if (end == TEXEC_RINGBUF_FRONT) {
    ringbuf_push_front(rb, item);
  } else {
    ringbuf_push_back(rb, item);
  }
}

static inline uintptr_t ringbuf_pop_end(texec_ringbuf_t* rb, texec_ringbuf_end_t end) {
  if (end == TEXEC_RINGBUF_FRONT) {
    return ringbuf_pop_front(rb);
  } else {
    return ringbuf_pop_back(rb);
  }
}

static inline texec_status_t ringbuf_push_impl(texec_ringbuf_t* rb, texec_ringbuf_end_t end, uintptr_t item, bool wait_not_full) {
  if (!rb) return TEXEC_STATUS_INVALID_ARGUMENT;

  mtx_lock(&rb->mtx);

  while (!rb->closed && ringbuf_is_full(rb)) {
    if (!wait_not_full) {
      return ringbuf_unlock_return(rb, TEXEC_STATUS_REJECTED);
    }
    cnd_wait(&rb->not_full, &rb->mtx);
  }

  if (rb->closed) {
    return ringbuf_unlock_return(rb, TEXEC_STATUS_CLOSED);
  }

  ringbuf_push_end(rb, end, item);

  cnd_signal(&rb->not_empty);
  mtx_unlock(&rb->mtx);
  return TEXEC_STATUS_OK;
}

static inline texec_status_t ringbuf_pop_impl(texec_ringbuf_t* rb, texec_ringbuf_end_t end, uintptr_t* out_item, bool wait_not_empty) {
  if (!rb || !out_item) return TEXEC_STATUS_INVALID_ARGUMENT;

  mtx_lock(&rb->mtx);

  while (!rb->closed && ringbuf_is_empty(rb)) {
    if (!wait_not_empty) {
      return ringbuf_unlock_return(rb, TEXEC_STATUS_REJECTED);
    }
    cnd_wait(&rb->not_empty, &rb->mtx);
  }

  // Drain semantics: if closed but still has items, allow pops until empty.
  if (rb->closed && ringbuf_is_empty(rb)) {
    return ringbuf_unlock_return(rb, TEXEC_STATUS_CLOSED);
  }

  *out_item = ringbuf_pop_end(rb, end);

  cnd_signal(&rb->not_full);
  mtx_unlock(&rb->mtx);
  return TEXEC_STATUS_OK;
}

texec_status_t texec_ringbuf_init(texec_ringbuf_t* rb, size_t capacity, const texec_allocator_t* alloc) {
  if (!rb || capacity == 0) return TEXEC_STATUS_INVALID_ARGUMENT;
  if (!alloc) alloc = texec_get_default_allocator();

  uintptr_t* buf = texec_allocate(alloc, capacity * sizeof(uintptr_t), _Alignof(uintptr_t));
  if (!buf) return TEXEC_STATUS_OUT_OF_MEMORY;

  if (!ringbuf_init_sync_prims(rb)) {
    texec_free(alloc, buf, capacity * sizeof(uintptr_t), _Alignof(uintptr_t));
    return TEXEC_STATUS_INTERNAL_ERROR;
  }

  rb->alloc = alloc;
  rb->buf = buf;
  rb->capacity = capacity;
  rb->count = 0;
  rb->head = 0;
  rb->tail = 0;
  rb->closed = false;

  return TEXEC_STATUS_OK;
}

texec_status_t texec_ringbuf_destroy(texec_ringbuf_t* rb) {
  if (!rb) return TEXEC_STATUS_INVALID_ARGUMENT;

  mtx_lock(&rb->mtx);
  const bool closed = rb->closed;
  mtx_unlock(&rb->mtx);
  if (!closed) return TEXEC_STATUS_BUSY;

  cnd_destroy(&rb->not_full);
  cnd_destroy(&rb->not_empty);
  mtx_destroy(&rb->mtx);

  texec_free(rb->alloc, rb->buf, rb->capacity * sizeof(uintptr_t), _Alignof(uintptr_t));

  rb->buf = NULL;
  rb->capacity = 0;
  rb->count = 0;
  rb->head = rb->tail = 0;
  rb->closed = true;
  rb->alloc = NULL;

  return TEXEC_STATUS_OK;
}

void texec_ringbuf_close(texec_ringbuf_t* rb) {
  if (!rb) return;

  mtx_lock(&rb->mtx);
  if (!rb->closed) {
    // Wake everyone so they can observe rb->closed and exit.
    cnd_broadcast(&rb->not_empty);
    cnd_broadcast(&rb->not_full);
    rb->closed = true;
  }
  mtx_unlock(&rb->mtx);
}

texec_status_t texec_ringbuf_try_push(texec_ringbuf_t* rb, texec_ringbuf_end_t end, uintptr_t item) {
  return ringbuf_push_impl(rb, end, item, false);
}

texec_status_t texec_ringbuf_try_pop(texec_ringbuf_t* rb, texec_ringbuf_end_t end, uintptr_t* out_item) {
  return ringbuf_pop_impl(rb, end, out_item, false);
}

texec_status_t texec_ringbuf_push(texec_ringbuf_t* rb, texec_ringbuf_end_t end, uintptr_t item) {
  return ringbuf_push_impl(rb, end, item, true);
}

texec_status_t texec_ringbuf_pop(texec_ringbuf_t* rb, texec_ringbuf_end_t end, uintptr_t* out_item) {
  return ringbuf_pop_impl(rb, end, out_item, true);
}
