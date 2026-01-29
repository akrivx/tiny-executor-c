#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*texec_task_run_t)(void* ctx);
typedef void (*texec_task_on_complete_fn_t)(void* ctx);

typedef struct texec_task {
  texec_task_run_t run;
  void* ctx;
  texec_task_on_complete_fn_t on_complete; // optional; called after run, on the executing thread
} texec_task_t;

#ifdef __cplusplus
}
#endif
