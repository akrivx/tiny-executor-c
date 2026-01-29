#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct texec_submit_info_t;
struct texec_task;

typedef void (*texec_on_submit_fn_t)(void* user, const struct texec_submit_info_t* submit_info);
typedef void (*texec_on_task_begin_fn_t)(void* user, const struct texec_task* task, const void* trace_context);
typedef void (*texec_on_task_end_fn_t)(void* user, const struct texec_task* task, const void* trace_context, int task_result);

typedef struct texec_diagnostics {
  void* user;
  texec_on_submit_fn_t on_submit;
  texec_on_task_begin_fn_t on_task_begin;
  texec_on_task_end_fn_t on_task_end;
} texec_diagnostics_t;

#ifdef __cplusplus
}
#endif
