#pragma once

#include "texec/executor.h"
#include "texec/task.h"
#include "texec/task_handle.h"

#include "internal/allocator.h"
#include "internal/work_item.h"

typedef enum texec_executor_state {
  TEXEC_EXECUTOR_STATE_RUNNING,
  TEXEC_EXECUTOR_STATE_CLOSING,
  TEXEC_EXECUTOR_STATE_CLOSED,
} texec_executor_state_t;

typedef texec_status_t (*texec_executor_submit_fn_t)(texec_executor_t* ex,  const texec_executor_submit_info_t* info, texec_task_handle_t** out_handle);
typedef texec_status_t (*texec_executor_submit_many_fn_t)(texec_executor_t* ex, const texec_executor_submit_info_t* infos, size_t count, texec_task_group_t** out_group);
typedef void (*texec_executor_close_fn_t)(texec_executor_t* ex);
typedef void (*texec_executor_join_fn_t)(texec_executor_t* ex);
typedef texec_status_t (*texec_executor_destroy_fn_t)(texec_executor_t* ex);
typedef texec_status_t (*texec_executor_query_fn_t)(const texec_executor_t* ex, texec_executor_capability_t cap, void* out_value);

typedef struct texec_executor_vtable {
  texec_executor_submit_fn_t submit;
  texec_executor_submit_many_fn_t submit_many;
  texec_executor_close_fn_t close;
  texec_executor_join_fn_t join;
  texec_executor_destroy_fn_t destroy;
  texec_executor_query_fn_t query;
} texec_executor_vtable_t;

struct texec_executor {
  const texec_executor_vtable_t* vtbl;
  const texec_allocator_t* alloc;
  const texec_executor_diagnostics_t* diag;
  texec_executor_kind_t kind;
  texec_executor_state_t state;
};

typedef struct texec_thread_pool_executor_config {
  const texec_allocator_t* alloc;
  const texec_executor_diagnostics_t* diag;
  size_t thread_count;
  size_t queue_capacity;
  texec_executor_backpressure_policy_t backpressure_policy;
} texec_thread_pool_executor_config_t;

texec_status_t texec_executor_create_thread_pool(const texec_thread_pool_executor_config_t* cfg, texec_executor_t** out_ex);

static inline const void* texec_executor_submit_get_trace_context(const texec_executor_submit_info_t* info) {
  const texec_executor_submit_trace_context_info_t* ti = texec_structure_find(info->header.next, TEXEC_STRUCTURE_TYPE_EXECUTOR_SUBMIT_TRACE_CONTEXT);
  return ti ? ti->trace_context : ti;
}

static inline void texec_executor_diagnostics_on_submit(const texec_executor_diagnostics_t* diag, const struct texec_executor_submit_info* submit_info) {
  if (diag) diag->on_submit(diag->user, submit_info);
}

static inline void texec_executor_diagnostics_on_task_begin(const texec_executor_diagnostics_t* diag, const texec_task_t* task, const void* trace_context) {
  if (diag) diag->on_task_begin(diag->user, task, trace_context);
}

static inline void texec_executor_diagnostics_on_task_end(const texec_executor_diagnostics_t* diag, const texec_task_t* task, const void* trace_context, int task_result) {
  if (diag) diag->on_task_end(diag->user, task, trace_context, task_result);
}

static inline void texec_task_cleanup(const texec_task_t* t) {
  if (t->cleanup) t->cleanup(t->ctx);
}

static inline void texec_executor_consume_work_item(const texec_executor_t* ex, texec_work_item_t* wi) {
  texec_executor_diagnostics_on_task_begin(ex->diag, &wi->task, wi->trace_context);
  const int result = wi->task.fn(wi->task.ctx);
  texec_executor_diagnostics_on_task_end(ex->diag, &wi->task, wi->trace_context, result);
  texec_task_cleanup(&wi->task);
  texec_task_handle_complete(wi->handle, result);
  texec_work_item_destroy(ex, wi);
}
