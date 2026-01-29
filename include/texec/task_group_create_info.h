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

#ifdef __cplusplus
}
#endif
