#pragma once

#include <stdbool.h>
#include "texec/base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct texec_task_handle texec_task_handle_t;

texec_status_t texec_task_handle_retain(texec_task_handle_t* h);
void texec_task_handle_release(texec_task_handle_t* h);

texec_status_t texec_task_handle_try_result(texec_task_handle_t* h, int* out_result);
texec_status_t texec_task_handle_result(texec_task_handle_t* h, int* out_result);
bool texec_task_handle_is_done(texec_task_handle_t* h);

static inline texec_status_t texec_task_handle_wait(texec_task_handle_t* h) {
  int result;
  return texec_task_handle_result(h, &result);
}

#ifdef __cplusplus
}
#endif
