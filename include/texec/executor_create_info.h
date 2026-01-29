#pragma once

#include <stddef.h>

#include "texec/base.h"
#include "texec/diagnostics.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum texec_executor_kind {
  TEXEC_EXECUTOR_KIND_INLINE = 1,
  TEXEC_EXECUTOR_KIND_THREAD_POOL
} texec_executor_kind_t;

typedef struct texec_executor_create_info {
  texec_structure_header_t header;
  texec_executor_kind_t kind;
} texec_executor_create_info_t;

// --- Create Extensions ---

typedef struct texec_executor_create_thread_pool_info {
  texec_structure_header_t header;
  size_t thread_count;
  size_t queue_capacity;
  texec_backpressure_policy_t backpressure;
} texec_executor_create_thread_pool_info_t;

typedef struct texec_executor_create_diagnostics_info {
  texec_structure_header_t header;
  const texec_diagnostics_t* diag;
} texec_executor_create_diagnostics_info_t;

#ifdef __cplusplus
}
#endif
