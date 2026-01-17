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

typedef enum texec_queue_full_policy {
  TEXEC_QUEUE_FULL_REJECT = 0,
  TEXEC_QUEUE_FULL_BLOCK
} texec_queue_full_policy_t;

typedef struct texec_queue_create_full_policy_info {
  texec_structure_header_t header;
  texec_queue_full_policy_t policy;
} texec_queue_create_full_policy_info_t;

typedef struct texec_queue_create_allocator_info {
  texec_structure_header_t header;
  const texec_allocator_t* allocator;
} texec_queue_create_allocator_info_t;

#ifdef __cplusplus
}
#endif
