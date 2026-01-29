#include "internal/executor.h"

#include <assert.h>
#include <stddef.h>
#include <threads.h>

#include "texec/queue.h"
#include "texec/task_group.h"
#include "internal/task_handle.h"

typedef struct thread_pool_executor {
  texec_executor_t base;
  mtx_t mtx;
  texec_queue_t* q;
  thrd_t* threads;
  size_t thread_count;
  texec_backpressure_policy_t backpressure;
} thread_pool_executor_t;

static inline bool tp_is_thread_pool(const texec_executor_t* ex) {
  return ex && ex->kind == TEXEC_EXECUTOR_KIND_THREAD_POOL;
}

static inline thread_pool_executor_t* tp_from_base(texec_executor_t* ex) {
  if (!tp_is_thread_pool(ex)) {
    return NULL;
  }
  return (thread_pool_executor_t*)ex;
}

static inline const thread_pool_executor_t* tp_from_const_base(const texec_executor_t* ex) {
  if (!tp_is_thread_pool(ex)) {
    return NULL;
  }
  return (const thread_pool_executor_t*)ex;
}

static texec_executor_state_t tp_get_state(thread_pool_executor_t* ex) {
  mtx_lock(&ex->mtx);
  const texec_executor_state_t state = ex->base.state;
  mtx_unlock(&ex->mtx);
  return state;
}

static void tp_free(thread_pool_executor_t* ex) {
  texec_free(ex->base.alloc, ex, sizeof(*ex), _Alignof(thread_pool_executor_t));
}

static texec_status_t tp_destroy_unchecked(thread_pool_executor_t* ex) {  
  if (ex->q) {
    texec_status_t st = texec_queue_destroy(ex->q);
    if (st != TEXEC_STATUS_OK) return st;
  }

  if (ex->threads) {
    texec_free(ex->base.alloc, ex->threads, ex->thread_count * sizeof(thrd_t), _Alignof(thrd_t));
  }
  
  mtx_destroy(&ex->mtx);
  
  tp_free(ex);

  return TEXEC_STATUS_OK;
}

static int tp_worker_main(void* arg) {
  thread_pool_executor_t* ex = (thread_pool_executor_t*)arg;

  for (;;) {
    texec_work_item_t* wi = NULL;
    texec_status_t st = texec_queue_pop_ptr(ex->q, &wi);

    if (st == TEXEC_STATUS_OK) {
      texec_executor_consume_work_item(&ex->base, wi);
      continue;
    }

    if (st == TEXEC_STATUS_CLOSED) {
      break;
    }

    // Defensive: exit on unexpected code
    break;
  }

  return 0;
}

static texec_status_t tp_start_workers(thread_pool_executor_t* ex) {
  for (size_t i = 0; i < ex->thread_count; ++i) {
    if (thrd_create(&ex->threads[i], &tp_worker_main, ex) != thrd_success) {
      // Best effort: shut down already started threads
      texec_queue_close(ex->q);
      for (size_t j = 0; j < i; ++j) {
        thrd_join(ex->threads[j], NULL);
      }
      return TEXEC_STATUS_INTERNAL_ERROR;
    }
  }
  return TEXEC_STATUS_OK;
}

static texec_status_t tp_submit_with_handle(thread_pool_executor_t* ex,
                                            texec_task_t task,
                                            const void* trace_context,
                                            texec_backpressure_policy_t backpressure,
                                            texec_task_handle_t* h) {
  if (!ex || !h) return TEXEC_STATUS_INVALID_ARGUMENT;

  if (tp_get_state(ex) != TEXEC_EXECUTOR_STATE_RUNNING) return TEXEC_STATUS_CLOSED;

  texec_work_item_t* wi = texec_work_item_allocate(ex->base.alloc);
  if (!wi) return TEXEC_STATUS_OUT_OF_MEMORY;

  wi->task = task;
  wi->handle = h;
  wi->trace_context = trace_context;

  texec_status_t st = TEXEC_STATUS_INTERNAL_ERROR;

  switch (backpressure) {
  case TEXEC_BACKPRESSURE_REJECT:
    st = texec_queue_try_push_ptr(ex->q, wi);
    break;
  
  case TEXEC_BACKPRESSURE_BLOCK:
    st = texec_queue_push_ptr(ex->q, wi);
    break;

  case TEXEC_BACKPRESSURE_CALLER_RUNS:
    st = texec_queue_try_push_ptr(ex->q, wi);
    if (st == TEXEC_STATUS_REJECTED) {
      texec_executor_consume_work_item(&ex->base, wi);
      st = TEXEC_STATUS_OK;
    }
    break;
  
  default:
    assert(false);
    break;
  }

  if (st != TEXEC_STATUS_OK) {
    texec_work_item_destroy(wi, ex->base.alloc);
  }

  return st;
}

static texec_executor_state_t tp_close(thread_pool_executor_t* ex) {
  mtx_lock(&ex->mtx);
  const texec_executor_state_t original_state = ex->base.state;
  if (original_state == TEXEC_EXECUTOR_STATE_RUNNING) {
    ex->base.state = TEXEC_EXECUTOR_STATE_CLOSING;
    texec_queue_close(ex->q);
  }
  mtx_unlock(&ex->mtx);
  return original_state;
}

static void tp_join(thread_pool_executor_t* ex) {
  if (tp_close(ex) == TEXEC_EXECUTOR_STATE_CLOSED) return;

  for (size_t i = 0; i < ex->thread_count; ++i) {
    thrd_join(ex->threads[i], NULL);
  }

  mtx_lock(&ex->mtx);
  ex->base.state = TEXEC_EXECUTOR_STATE_CLOSED;
  mtx_unlock(&ex->mtx);
}

static texec_status_t tp_vtbl_submit(texec_executor_t* ex,  const texec_executor_submit_info_t* info, texec_task_handle_t** out_handle) {
  if (!out_handle) return TEXEC_STATUS_INVALID_ARGUMENT;
  *out_handle = NULL;
  
  thread_pool_executor_t* tp_ex = tp_from_base(ex);
  if (!tp_ex) return TEXEC_STATUS_INVALID_ARGUMENT;

  if (!info || info->header.type != TEXEC_STRUCTURE_TYPE_EXECUTOR_SUBMIT_INFO) {
    return TEXEC_STATUS_INVALID_ARGUMENT;
  }

  if (!info->task.fn) return TEXEC_STATUS_INVALID_ARGUMENT;

  const texec_executor_submit_backpressure_info_t* bpi = texec_structure_find(info->header.next, TEXEC_STRUCTURE_TYPE_EXECUTOR_SUBMIT_BACKPRESSURE);
  const texec_backpressure_policy_t backpressure = (bpi ? bpi->backpressure : tp_ex->backpressure);

  const texec_executor_submit_trace_context_info_t* tci = texec_structure_find(info->header.next, TEXEC_STRUCTURE_TYPE_EXECUTOR_SUBMIT_TRACE_CONTEXT);
  const void* trace_context = tci ? tci->trace_context : NULL;

  texec_task_handle_t* h = texec_task_handle_create(tp_ex->base.alloc);
  if (!h) return TEXEC_STATUS_OUT_OF_MEMORY;

  if (texec_task_handle_retain(h) != TEXEC_STATUS_OK) {
    texec_task_handle_destroy(h);
    return TEXEC_STATUS_INTERNAL_ERROR;
  }

  texec_status_t st = tp_submit_with_handle(tp_ex, info->task, trace_context, backpressure, h);
  if (st != TEXEC_STATUS_OK) {
    texec_task_handle_release(h);
    return st;
  }

  *out_handle = h;
  return st;
}

static texec_status_t tp_vtbl_submit_many(texec_executor_t* ex, const texec_executor_submit_info_t* infos, size_t count, texec_task_group_t** out_group) {
  if (!out_group) return TEXEC_STATUS_INVALID_ARGUMENT;
  *out_group = NULL;

  if (!tp_is_thread_pool(ex)) return TEXEC_STATUS_INVALID_ARGUMENT;

  const texec_task_group_create_info_t gi = {
    .header = {.type = TEXEC_STRUCTURE_TYPE_TASK_GROUP_CREATE_INFO, .next = NULL},
    .capacity = count,
  };

  texec_task_group_t* g = NULL;
  texec_status_t st = texec_task_group_create(&gi, ex->alloc, &g);
  if (st != TEXEC_STATUS_OK) return st;

  for (size_t i = 0; i < count; ++i) {
    texec_task_handle_t* h = NULL;
    st = tp_vtbl_submit(ex, &infos[i], &h);
    if (st != TEXEC_STATUS_OK) break;
    st = texec_task_group_add(g, h);
    texec_task_handle_release(h);
    if (st != TEXEC_STATUS_OK) break;
  }

  if (st != TEXEC_STATUS_OK) {
    texec_task_group_destroy(g);
  } else {
    *out_group = g;
  }
  return st;
}

static void tp_vtbl_close(texec_executor_t* ex) {
  thread_pool_executor_t* tp_ex = tp_from_base(ex);
  if (!tp_ex) return;
  tp_close(tp_ex);
}

static void tp_vtbl_join(texec_executor_t* ex) {
  thread_pool_executor_t* tp_ex = tp_from_base(ex);
  if (!tp_ex) return;
  tp_join(tp_ex);
}

static texec_status_t tp_vtbl_destroy(texec_executor_t* ex) {
  thread_pool_executor_t* tp_ex = tp_from_base(ex);
  if (!tp_ex) return TEXEC_STATUS_INVALID_ARGUMENT;
  if (tp_get_state(tp_ex) != TEXEC_EXECUTOR_STATE_CLOSED) return TEXEC_STATUS_BUSY;
  return tp_destroy_unchecked(tp_ex);
}

static texec_status_t tp_vtbl_query(const texec_executor_t* ex, texec_executor_capability_t cap, void* out_value) {
  if (!out_value) return TEXEC_STATUS_INVALID_ARGUMENT;

  const thread_pool_executor_t* tp_ex = tp_from_const_base(ex);
  if (!ex) return TEXEC_STATUS_INVALID_ARGUMENT;

  switch (cap){
  case TEXEC_EXECUTOR_CAPABILITY_WORKER_COUNT:
    *(size_t*)out_value = tp_ex->thread_count;
    return TEXEC_STATUS_OK;
    
  case TEXEC_EXECUTOR_CAPABILITY_SUPPORTS_PRIORITY:
    *(bool*)out_value = false;
    return TEXEC_STATUS_OK;
  
  case TEXEC_EXECUTOR_CAPABILITY_SUPPORTS_DEADLINE:
    *(bool*)out_value = false;
    return TEXEC_STATUS_OK;
  
  case TEXEC_EXECUTOR_CAPABILITY_SUPPORTS_TRACING:
    *(bool*)out_value = true;
    return TEXEC_STATUS_OK;
  
  default:
    break;
  }

  return TEXEC_STATUS_INVALID_ARGUMENT;
}

texec_status_t texec_executor_create_thread_pool(const texec_thread_pool_executor_config_t* cfg, texec_executor_t** out_ex) {
  if (!out_ex) return TEXEC_STATUS_INVALID_ARGUMENT;
  *out_ex = NULL;

  if (!cfg) return TEXEC_STATUS_INVALID_ARGUMENT;

  thread_pool_executor_t* tp_ex = texec_allocate(cfg->alloc, sizeof(*tp_ex), _Alignof(thread_pool_executor_t));
  if (!tp_ex) return TEXEC_STATUS_OUT_OF_MEMORY;

  static const texec_executor_vtable_t vtbl_instance = {
    .submit = tp_vtbl_submit,
    .submit_many = tp_vtbl_submit_many,
    .close = tp_vtbl_close,
    .join = tp_vtbl_join,
    .destroy = tp_vtbl_destroy,
    .query = tp_vtbl_query,
  };
  
  tp_ex->base.vtbl = &vtbl_instance;
  tp_ex->base.alloc = cfg->alloc;
  tp_ex->base.diag = cfg->diag;
  tp_ex->base.kind = TEXEC_EXECUTOR_KIND_THREAD_POOL;
  tp_ex->base.state = TEXEC_EXECUTOR_STATE_RUNNING;
  tp_ex->q = NULL;
  tp_ex->threads = NULL;
  tp_ex->thread_count = 0;
  tp_ex->backpressure = cfg->backpressure;

  if (mtx_init(&tp_ex->mtx, mtx_plain) != thrd_success) {
    tp_free(tp_ex);
    return TEXEC_STATUS_INTERNAL_ERROR;
  }

  thrd_t* threads = texec_allocate(tp_ex->base.alloc, cfg->thread_count * sizeof(thrd_t), _Alignof(thrd_t));
  if (!threads) {
    tp_destroy_unchecked(tp_ex);
    return TEXEC_STATUS_OUT_OF_MEMORY;
  }
  tp_ex->threads = threads;
  tp_ex->thread_count = cfg->thread_count;

  const texec_queue_create_info_t qi = {
    .header = {.type = TEXEC_STRUCTURE_TYPE_QUEUE_CREATE_INFO, .next = NULL},
    .capacity = cfg->queue_capacity,
  };
  texec_queue_t* q = NULL;
  texec_status_t st = texec_queue_create(&qi, tp_ex->base.alloc, &q);
  if (st != TEXEC_STATUS_OK) {
    tp_destroy_unchecked(tp_ex);
    return st;
  }
  tp_ex->q = q;

  st = tp_start_workers(tp_ex);
  if (st != TEXEC_STATUS_OK) {
    tp_destroy_unchecked(tp_ex);
    return st;
  }

  *out_ex = (texec_executor_t*)tp_ex;
  return TEXEC_STATUS_OK;
}
