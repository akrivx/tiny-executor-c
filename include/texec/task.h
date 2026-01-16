#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*texec_task_fn_t)(void* ctx);
typedef void (*texec_task_cleanup_fn_t)(void* ctx);

typedef struct texec_task {
  texec_task_fn_t fn;
  void* ctx;
  texec_task_cleanup_fn_t cleanup; // optional; called after fn, on the executing thread
} texec_task_t;

#ifdef __cplusplus
}
#endif
