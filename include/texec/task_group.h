#pragma once

#include <stddef.h>

#include "texec/base.h"
#include "texec/task_handle.h"
#include "texec/task_group_create_info.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct texec_task_group texec_task_group_t;

texec_status_t texec_task_group_create(const texec_task_group_create_info_t* info, texec_task_group_t** out_group);
void texec_task_group_destroy(texec_task_group_t* g);

texec_status_t texec_task_group_add(texec_task_group_t* g, texec_task_handle_t* h); // retains handle internally

texec_status_t texec_task_group_wait(texec_task_group_t* g);

#ifdef __cplusplus
}
#endif
