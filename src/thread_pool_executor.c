#include "executor_internal.h"

#include <threads.h>

#include "texec/queue.h"
#include "texec/task_group.h"
#include "internal/task_handle.h"

typedef struct thread_pool_executor_impl {
  texec__executor_impl_t base;

  mtx_t mtx;

  texec_queue_t* q;

  thrd_t* threads;
  size_t thread_count;

  texec_executor_backpressure_policy_t backpressure;

  bool shutdown_called;
  bool terminated;
} thread_pool_executor_impl_t;

static void tp_free(thread_pool_executor_impl_t* ex) {
  texec_free(ex->base.alloc, ex, sizeof(*ex), _Alignof(thread_pool_executor_impl_t));
}

static void tp_destroy_no_wait(thread_pool_executor_impl_t* ex) {  
  if (ex->q) {
    texec_queue_destroy(ex->q);
  }

  if (ex->threads) {
    texec_free(ex->base.alloc, ex->threads, ex->thread_count * sizeof(thrd_t), _Alignof(thrd_t));
  }
  
  mtx_destroy(&ex->mtx);
  
  tp_free(ex);
}

static int tp_worker_main(void* arg) {
  thread_pool_executor_impl_t* ex = (thread_pool_executor_impl_t*)arg;

  for (;;) {
    texec__executor_work_item_t* wi = NULL;
    texec_status_t st = texec_queue_pop_ptr(ex->q, &wi);

    if (st == TEXEC_STATUS_OK) {
      texec__executor_impl_consume_work_item(&ex->base, wi);
      continue;
    }

    if (st == TEXEC_STATUS_SHUTDOWN) {
      break;
    }

    // Defensive: exit on unexpected code
    break;
  }

  return 0;
}

static texec_status_t tp_start_workers(thread_pool_executor_impl_t* ex) {
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

static texec_status_t tp_submit_with_handle(thread_pool_executor_impl_t* ex, texec_task_t task, const void* trace_context, texec_task_handle_t* h) {
  if (!ex || !h) return TEXEC_STATUS_INVALID_ARGUMENT;

  mtx_lock(&ex->mtx);
  const bool is_shutdown = ex->shutdown_called;
  mtx_unlock(&ex->mtx);
  if (is_shutdown) return TEXEC_STATUS_SHUTDOWN;

  texec_status_t st = texec_task_handle_retain(h);
  if (st != TEXEC_STATUS_OK) return st;

  texec__executor_work_item_t* wi = texec__executor_impl_alloc_work_item(ex);
  if (!wi) {
    texec_task_handle_release(h);
    return TEXEC_STATUS_OUT_OF_MEMORY;
  }

  wi->task = task;
  wi->handle = h;
  wi->trace_context = trace_context;

  if (ex->backpressure == TEXEC_EXECUTOR_BACKPRESSURE_REJECT) {
    st = texec_queue_try_push_ptr(ex->q, wi);
    if (st == TEXEC_STATUS_NOT_READY) st = TEXEC_STATUS_REJECTED;
    if (st != TEXEC_STATUS_OK) {
      texec__executor_impl_destroy_work_item(&ex->base, wi);
      return st;
    }
  } else if (ex->backpressure == TEXEC_EXECUTOR_BACKPRESSURE_BLOCK) {
    st = texec_queue_push_ptr(ex->q, wi);
    if (st != TEXEC_STATUS_OK) {
      texec__executor_impl_destroy_work_item(&ex->base, wi);
      return st;
    }
  } else { // TEXEC_EXECUTOR_BACKPRESSURE_CALLER_RUNS
    st = texec_queue_try_push_ptr(ex->q, wi);
    if (st == TEXEC_STATUS_NOT_READY) {
      texec__executor_impl_consume_work_item(&ex->base, wi);
    } else if (st != TEXEC_STATUS_OK) {
      texec__executor_impl_destroy_work_item(&ex->base, wi);
      return st;
    }
  }

  return TEXEC_STATUS_OK;
}

static texec_status_t tp_vtbl_submit(texec__executor_impl_t* ex,  const texec_executor_submit_info_t* info, texec_task_handle_t** out_handle) {
  if (!out_handle) return TEXEC_STATUS_INVALID_ARGUMENT;
  *out_handle = NULL;

  if (!ex || ex->kind != TEXEC_EXECUTOR_KIND_THREAD_POOL) return TEXEC_STATUS_INVALID_ARGUMENT;

  if (!info || info->header.type != TEXEC_STRUCTURE_TYPE_EXECUTOR_SUBMIT_INFO) {
    return TEXEC_STATUS_INVALID_ARGUMENT;
  }

  if (!info->task.fn) return TEXEC_STATUS_INVALID_ARGUMENT;

  const void* trace_context = texec__executor_submit_get_trace_context(info);

  texec_task_handle_t* h = texec_task_handle_create(ex->alloc);
  if (!h) return TEXEC_STATUS_OUT_OF_MEMORY;

  texec_status_t st = tp_submit_with_handle((thread_pool_executor_impl_t*)ex, info->task, trace_context, h);
  if (st != TEXEC_STATUS_OK) {
    texec_task_handle_release(h);
    return st;
  }

  *out_handle = h;
  return st;
}

static texec_status_t tp_vtbl_submit_many(texec__executor_impl_t* ex, const texec_executor_submit_info_t* infos, size_t count, texec_task_group_t** out_group) {
  if (!out_group) return TEXEC_STATUS_INVALID_ARGUMENT;
  *out_group = NULL;

  if (!ex || ex->kind != TEXEC_EXECUTOR_KIND_THREAD_POOL) return TEXEC_STATUS_INVALID_ARGUMENT;

  const texec_task_group_create_allocator_info_t gai = {
    .header = {.type = TEXEC_STRUCTURE_TYPE_TASK_GROUP_CREATE_ALLOCATOR_INFO, .next = NULL},
    .allocator = ex->alloc,
  };
  const texec_task_group_create_info_t gi = {
    .header = {.type = TEXEC_STRUCTURE_TYPE_TASK_GROUP_CREATE_INFO, .next = &gai},
    .capacity = count,
  };

  texec_task_group_t* g = NULL;
  texec_status_t st = texec_task_group_create(&gi, &g);
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

static void tp_vtbl_shutdown(texec__executor_impl_t* ex) {
  if (!ex || ex->kind != TEXEC_EXECUTOR_KIND_THREAD_POOL) return;

  thread_pool_executor_impl_t* tp_ex = (thread_pool_executor_impl_t*)ex;
  mtx_lock(&tp_ex->mtx);
  if (!tp_ex->shutdown_called) {
    tp_ex->shutdown_called = true;
    texec_queue_close(tp_ex->q);
  }
  mtx_unlock(&tp_ex->mtx);
}

static void tp_vtbl_await_termination(texec__executor_impl_t* ex) {
  if (!ex || ex->kind != TEXEC_EXECUTOR_KIND_THREAD_POOL) return;

  tp_vtbl_shutdown(ex);

  thread_pool_executor_impl_t* tp_ex = (thread_pool_executor_impl_t*)ex;

  mtx_lock(&tp_ex->mtx);
  const bool already_terminated = tp_ex->terminated;
  mtx_unlock(&tp_ex->mtx);
  if (already_terminated) return;

  for (size_t i = 0; i < tp_ex->thread_count; ++i) {
    thrd_join(tp_ex->threads[i], NULL);
  }

  mtx_lock(&tp_ex->mtx);
  tp_ex->terminated = true;
  mtx_unlock(&tp_ex->mtx);
}

static void tp_vtbl_destroy(texec__executor_impl_t* ex) {
  if (!ex || ex->kind != TEXEC_EXECUTOR_KIND_THREAD_POOL) return;
  tp_vtbl_await_termination(ex);
  tp_destroy_no_wait((thread_pool_executor_impl_t*)ex, true);
}

static texec_status_t tp_vtbl_query(const texec__executor_impl_t* ex, texec_executor_capability_t cap, void* out_value) {
  if (!ex || ex->kind != TEXEC_EXECUTOR_KIND_THREAD_POOL || !out_value) return TEXEC_STATUS_INVALID_ARGUMENT;

  const thread_pool_executor_impl_t* tp_ex = (const thread_pool_executor_impl_t*)ex;

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

static const texec__executor_vtable_t TP_VTBL = {
  .submit = tp_vtbl_submit,
  .submit_many = tp_vtbl_submit_many,
  .shutdown = tp_vtbl_shutdown,
  .await_termination = tp_vtbl_await_termination,
  .query = tp_vtbl_query,
};

texec_status_t texec__executor_create_thread_pool(const texec__thread_pool_executor_config_t* cfg, texec_executor_t* ex) {
  if (!cfg || !ex) return TEXEC_STATUS_INVALID_ARGUMENT;

  thread_pool_executor_impl_t* tp_ex = texec_allocate(cfg->alloc, sizeof(*tp_ex), _Alignof(thread_pool_executor_impl_t));
  if (!tp_ex) return TEXEC_STATUS_OUT_OF_MEMORY;
  
  tp_ex->base.alloc = cfg->alloc;
  tp_ex->base.diag = cfg->diag;
  tp_ex->base.kind = TEXEC_EXECUTOR_KIND_THREAD_POOL;
  tp_ex->q = NULL;
  tp_ex->threads = NULL;
  tp_ex->thread_count = 0;
  tp_ex->backpressure = cfg->backpressure_policy;
  tp_ex->shutdown_called = false;
  tp_ex->terminated = false;

  if (mtx_init(&tp_ex->mtx, mtx_plain) != thrd_success) {
    tp_free(tp_ex);
    return TEXEC_STATUS_INTERNAL_ERROR;
  }

  thrd_t* threads = texec_allocate(tp_ex->base.alloc, tp_ex->thread_count * sizeof(thrd_t), _Alignof(thrd_t));
  if (!threads) {
    tp_destroy_no_wait(tp_ex);
    return TEXEC_STATUS_OUT_OF_MEMORY;
  }
  tp_ex->threads = threads;
  tp_ex->thread_count = cfg->thread_count;

  const texec_queue_create_allocator_info_t qai = {
    .header = {.type = TEXEC_STRUCTURE_TYPE_QUEUE_CREATE_ALLOCATOR_INFO, .next = NULL},
    .allocator = tp_ex->base.alloc,
  };
  const texec_queue_create_full_policy_info_t qfpi = {
    .header = {.type = TEXEC_STRUCTURE_TYPE_QUEUE_CREATE_FULL_POLICY_INFO, .next = &qai},
    .policy = tp_ex->backpressure,
  };
  const texec_queue_create_info_t qi = {
    .header = {.type = TEXEC_STRUCTURE_TYPE_QUEUE_CREATE_INFO, .next = &qfpi},
    .capacity = cfg->queue_capacity,
  };
  texec_queue_t* q = NULL;
  texec_status_t st = texec_queue_create(&qi, &q);
  if (st != TEXEC_STATUS_OK) {
    tp_destroy_no_wait(tp_ex);
    return st;
  }
  tp_ex->q = q;

  st = tp_start_workers(tp_ex);
  if (st != TEXEC_STATUS_OK) {
    tp_destroy_no_wait(tp_ex);
    return st;
  }

  ex->vtbl = &TP_VTBL;
  ex->impl = tp_ex;
  return TEXEC_STATUS_OK;
}