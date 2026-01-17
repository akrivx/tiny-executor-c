#pragma once

#include <stddef.h>

#include "texec/base.h"
#include "texec/task.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum texec_executor_kind {
  TEXEC_EXECUTOR_KIND_INLINE = 1,
  TEXEC_EXECUTOR_KIND_THREAD_POOL
} texec_executor_kind_t;

typedef struct texec_executor_create_info {
  texec_structure_header_t header;
  texec_executor_kind_t kind;
} texec_executor_create_info_t;

// --- Create Extensions ---

typedef enum texec_executor_backpressure_policy {
  TEXEC_EXECUTOR_BACKPRESSURE_REJECT = 0,
  TEXEC_EXECUTOR_BACKPRESSURE_BLOCK,
  TEXEC_EXECUTOR_BACKPRESSURE_CALLER_RUNS
} texec_executor_backpressure_policy_t;

typedef struct texec_executor_create_thread_pool_info {
  texec_structure_header_t header;
  size_t thread_count;
  size_t queue_capacity;
  texec_executor_backpressure_policy_t backpressure_policy;
} texec_executor_create_thread_pool_info_t;

typedef struct texec_executor_create_allocator_info {
  texec_structure_header_t header;
  const texec_allocator_t* allocator;
} texec_executor_create_allocator_info_t;

struct texec_executor_submit_info;
typedef void (*texec_executor_on_submit_fn_t)(void* user, const struct texec_executor_submit_info* submit_info);
typedef void (*texec_executor_on_task_begin_fn_t)(void* user, const texec_task_t* task, const void* trace_context);
typedef void (*texec_executor_on_task_end_fn_t)(void* user, const texec_task_t* task, const void* trace_context, int task_result);

typedef struct texec_executor_diagnostics {
  void* user;
  texec_executor_on_submit_fn_t on_submit;
  texec_executor_on_task_begin_fn_t on_task_begin;
  texec_executor_on_task_end_fn_t on_task_end;
} texec_executor_diagnostics_t;

typedef struct texec_executor_create_diagnostics_info {
  texec_structure_header_t header;
  const texec_executor_diagnostics_t* diag;
} texec_executor_create_diagnostics_info_t;

#ifdef __cplusplus
}
#endif
