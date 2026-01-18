#pragma once

#include "texec/executor.h"
#include "texec/task.h"
#include "texec/task_handle.h"

#include "internal/allocator.h"

typedef struct texec__executor_impl {
  const texec_allocator_t* alloc;
  const texec_executor_diagnostics_t* diag;
  texec_executor_kind_t kind;
} texec__executor_impl_t;

typedef texec_status_t (*texec__executor_submit_fn_t)(texec__executor_impl_t* ex,  const texec_executor_submit_info_t* info, texec_task_handle_t** out_handle);
typedef texec_status_t (*texec__executor_submit_many_fn_t)(texec__executor_impl_t* ex, const texec_executor_submit_info_t* infos, size_t count, texec_task_group_t** out_group);
typedef void (*texec__executor_shutdown_fn_t)(texec__executor_impl_t* ex);
typedef void (*texec__executor_await_termination_fn_t)(texec__executor_impl_t* ex);
typedef void (*texec__executor_destroy_fn_t)(texec__executor_impl_t* ex);
typedef texec_status_t (*texec__executor_query_fn_t)(const texec__executor_impl_t* ex, texec_executor_capability_t cap, void* out_value);

typedef struct texec__executor_vtable {
  texec__executor_submit_fn_t submit;
  texec__executor_submit_many_fn_t submit_many;
  texec__executor_shutdown_fn_t shutdown;
  texec__executor_await_termination_fn_t await_termination;
  texec__executor_destroy_fn_t destroy;
  texec__executor_query_fn_t query;
} texec__executor_vtable_t;

struct texec_executor {
  const texec__executor_vtable_t* vtbl;
  texec__executor_impl_t* impl;
};

typedef struct texec__executor_work_item {
  texec_task_t task;
  texec_task_handle_t* handle;
  const void* trace_context;
} texec__executor_work_item_t;

typedef struct texec__thread_pool_executor_config {
  const texec_allocator_t* alloc;
  const texec_executor_diagnostics_t* diag;
  size_t thread_count;
  size_t queue_capacity;
  texec_executor_backpressure_policy_t backpressure_policy;
} texec__thread_pool_executor_config_t;

texec_status_t texec__executor_create_thread_pool(const texec__thread_pool_executor_config_t* cfg, texec_executor_t* ex);

static inline const void* texec__executor_submit_get_trace_context(const texec_executor_submit_info_t* info) {
  const texec_executor_submit_trace_context_info_t* ti = texec_structure_find(info->header.next, TEXEC_STRUCTURE_TYPE_EXECUTOR_SUBMIT_TRACE_CONTEXT);
  return ti ? ti->trace_context : ti;
}

static inline void texec__task_cleanup(const texec_task_t* t) {
  if (t->cleanup) {
    t->cleanup(t->ctx);
  }
}

static inline void texec__executor_diagnostics_on_submit(const texec_executor_diagnostics_t* diag, const struct texec_executor_submit_info* submit_info) {
  if (diag) diag->on_submit(diag->user, submit_info);
}

static inline void texec__executor_diagnostics_on_task_begin(const texec_executor_diagnostics_t* diag, const texec_task_t* task, const void* trace_context) {
  if (diag) diag->on_task_begin(diag->user, task, trace_context);
}

static inline void texec__executor_diagnostics_on_task_end(const texec_executor_diagnostics_t* diag, const texec_task_t* task, const void* trace_context, int task_result) {
  if (diag) diag->on_task_end(diag->user, task, trace_context, task_result);
}

static inline texec__executor_work_item_t* texec__executor_impl_alloc_work_item(const texec__executor_impl_t* ex) {
  return texec_allocate(ex->alloc, sizeof(texec__executor_work_item_t), _Alignof(texec__executor_work_item_t));
}

static inline void texec__executor_impl_free_work_item(const texec__executor_impl_t* ex, texec__executor_work_item_t* wi) {
  texec_free(ex->alloc, wi, sizeof(*wi), _Alignof(texec__executor_work_item_t));
}

static inline void texec__executor_impl_destroy_work_item(const texec__executor_impl_t* ex, texec__executor_work_item_t* wi) {
  texec_task_handle_release(wi->handle);
  texec__executor_impl_free_work_item(ex, wi);
}

static inline void texec__executor_impl_consume_work_item(const texec__executor_impl_t* ex, texec__executor_work_item_t* wi) {
  texec__executor_diagnostics_on_task_begin(ex->diag, &wi->task, wi->trace_context);
  const int result = wi->task.fn(wi->task.ctx);
  texec__executor_diagnostics_on_task_end(ex->diag, &wi->task, wi->trace_context, result);
  texec__task_cleanup(&wi->task);
  texec_task_handle_complete(wi->handle, result);
  texec__executor_impl_destroy_work_item(ex, wi);
}
