#pragma once

#include <stdbool.h>

#include "texec/base.h"
#include "texec/task_handle.h"

texec_task_handle_t* texec__task_handle_create(texec_allocator_t* alloc);
void texec__task_handle_complete(texec_task_handle_t* h, int result);
