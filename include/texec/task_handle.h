#pragma once

#include <stdbool.h>
#include "texec/base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct texec_task_handle texec_task_handle_t;

void texec_task_handle_retain(texec_task_handle_t* h);
void texec_task_handle_release(texec_task_handle_t* h);

void texec_task_handle_wait(texec_task_handle_t* h);
bool texec_task_handle_try_wait(texec_task_handle_t* h);

bool texec_task_handle_is_done(const texec_task_handle_t* h);

#ifdef __cplusplus
}
#endif
