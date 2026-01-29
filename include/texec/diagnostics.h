#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct texec_executor_submit_info;
struct texec_task;

typedef void (*texec_executor_on_submit_fn_t)(void* user, const struct texec_executor_submit_info* submit_info);
typedef void (*texec_executor_on_task_begin_fn_t)(void* user, const struct texec_task* task, const void* trace_context);
typedef void (*texec_executor_on_task_end_fn_t)(void* user, const struct texec_task* task, const void* trace_context, int task_result);

typedef struct texec_executor_diagnostics {
  void* user;
  texec_executor_on_submit_fn_t on_submit;
  texec_executor_on_task_begin_fn_t on_task_begin;
  texec_executor_on_task_end_fn_t on_task_end;
} texec_executor_diagnostics_t;

#ifdef __cplusplus
}
#endif
