#pragma once

#include "texec/base.h"
#include "texec/task_handle.h"

texec_task_handle_t* texec_task_handle_create(const texec_allocator_t* alloc);
void texec_task_handle_complete(texec_task_handle_t* h, int result);
