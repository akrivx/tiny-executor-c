#pragma once

#include <stdbool.h>
#include "texec/base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct texec_task_handle texec_task_handle_t;

texec_status_t texec_task_handle_retain(texec_task_handle_t* h);
void texec_task_handle_release(texec_task_handle_t* h);

void texec_task_handle_wait(texec_task_handle_t* h);
bool texec_task_handle_is_done(const texec_task_handle_t* h);

texec_status_t texec_task_handle_result(const texec_task_handle_t* h, int* out_result);

#ifdef __cplusplus
}
#endif
