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

typedef struct texec_queue_create_allocator_info {
  texec_structure_header_t header;
  const texec_allocator_t* allocator;
} texec_queue_create_allocator_info_t;

#ifdef __cplusplus
}
#endif
