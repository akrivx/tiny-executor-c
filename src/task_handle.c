#include "texec/task_handle.h"

#include <stdatomic.h>
#include <stddef.h>
#include <string.h>
#include <threads.h>

#include "internal/allocator.h"

struct texec_task_handle {
  mtx_t mtx;
  cnd_t cv;
  const texec_allocator_t* alloc;
  atomic_uint refcount;
  int result;
  bool done;
};

static inline bool task_handle_init(texec_task_handle_t* h, const texec_allocator_t* alloc) {
  if (!h) return false;

  if (mtx_init(&h->mtx, mtx_plain) != thrd_success) {
    return false;
  }

  if (cnd_init(&h->cv) != thrd_success) {
    mtx_destroy(&h->mtx);
    return false;
  }

  atomic_init(&h->refcount, 1);
  h->alloc = alloc;
  h->result = 0;
  h->done = false;
  return true;
}

static inline void task_handle_free(texec_task_handle_t* h) {
  texec_free(h->alloc, h, sizeof(*h), _Alignof(texec_task_handle_t));
}

static inline texec_status_t task_handle_unlock_return(texec_task_handle_t* h, texec_status_t st) {
  mtx_unlock(&h->mtx);
  return st;
}

texec_task_handle_t* texec_task_handle_create(const texec_allocator_t* alloc) {
  texec_task_handle_t* h = texec_allocate(alloc, sizeof(*h), _Alignof(texec_task_handle_t));
  if (!task_handle_init(h, alloc)) {
    task_handle_free(h);
    return NULL;
  }
  return h;
}

void texec_task_handle_destroy(texec_task_handle_t* h) {
  if (!h) return;
  cnd_destroy(&h->cv);
  mtx_destroy(&h->mtx);
  task_handle_free(h);
}

void texec_task_handle_complete(texec_task_handle_t* h, int result) {
  if (!h) return;

  mtx_lock(&h->mtx);
  if (!h->done) {
    h->result = result;
    h->done = true;
    cnd_broadcast(&h->cv);
  }
  mtx_unlock(&h->mtx);
}

texec_status_t texec_task_handle_retain(texec_task_handle_t* h) {
  if (!h) return TEXEC_STATUS_INVALID_ARGUMENT;

  unsigned int count = atomic_load_explicit(&h->refcount, memory_order_relaxed);

  for (;;) {
    if (count == 0u) return TEXEC_STATUS_INVALID_ARGUMENT; // use-after-free bug in caller

    // Try to change refcount from `count` to `count + 1`.
    if (atomic_compare_exchange_weak_explicit(
      &h->refcount,
      &count,     // expected
      count + 1u, // desired
      memory_order_relaxed,  // on success
      memory_order_relaxed)) // on failure
    {
      break;
    }

    // CAS failed.
    // `count` now contains the *current* value of refcount.
    // Loop around and retry.
  }

  return TEXEC_STATUS_OK;
}

void texec_task_handle_release(texec_task_handle_t* h) {
  if (!h) return;

  if (atomic_fetch_sub_explicit(&h->refcount, 1, memory_order_release) == 1u) {
    atomic_thread_fence(memory_order_acquire);
    texec_task_handle_destroy(h);
  }
}

void texec_task_handle_wait(texec_task_handle_t* h) {
  if (!h) return;

  mtx_lock(&h->mtx);
  while (!h->done) {
    cnd_wait(&h->cv, &h->mtx);
  }
  mtx_unlock(&h->mtx);
}

bool texec_task_handle_is_done(texec_task_handle_t* h) {
  if (!h) return false;

  mtx_lock(&h->mtx);
  const bool done = h->done;
  mtx_unlock(&h->mtx);
  return done;
}

texec_status_t texec_task_handle_result(texec_task_handle_t* h, int* out_result) {
  if (!h || !out_result) return TEXEC_STATUS_INVALID_ARGUMENT;
  mtx_lock(&h->mtx);
  if (!h->done) return task_handle_unlock_return(h, TEXEC_STATUS_NOT_READY);
  *out_result = h->result;
  mtx_unlock(&h->mtx);
  return TEXEC_STATUS_OK;
}
