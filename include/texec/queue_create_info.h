#pragma once

#include <stddef.h>
#include <stdint.h>

#include "texec/base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct texec_queue_create_info {
  texec_structure_header_t header;
  size_t capacity;
} texec_queue_create_info_t;

// --- Queue Create Extensions ---

#ifdef __cplusplus
}
#endif
