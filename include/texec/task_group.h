#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "texec/base.h"
#include "texec/task_handle.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct texec_task_group texec_task_group_t;

typedef struct texec_task_group_create_info {
  texec_structure_header_t header;
  size_t max_tasks_hint;
} texec_task_group_create_info_t;

texec_status_t texec_task_group_create(const texec_task_group_create_info_t* info, texec_task_group_t** out);

void task_group_retain(texec_task_group_t* g);
void task_group_release(texec_task_group_t* g);

void task_group_add(texec_task_group_t* g, texec_task_handle_t* h); // retains handle internally

void task_group_wait(texec_task_group_t* g);

#ifdef __cplusplus
}
#endif
