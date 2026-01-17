#pragma once

#include <stddef.h>

#include "texec/base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct texec_task_group_create_info {
  texec_structure_header_t header;
  size_t capacity;
} texec_task_group_create_info_t;

// --- Task Group Create Extensions ---

typedef struct texec_task_group_create_allocator_info {
  texec_structure_header_t header;
  const texec_allocator_t* allocator;
} texec_task_group_create_allocator_info_t;

#ifdef __cplusplus
}
#endif
