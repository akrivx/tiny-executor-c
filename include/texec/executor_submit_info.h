#pragma once

#include <stdint.h>

#include "texec/base.h"
#include "texec/task.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct texec_executor_submit_info {
  texec_structure_header_t header;
  texec_task_t task;
} texec_executor_submit_info_t;

// --- Submit Extensions ---

typedef enum texec_executor_submit_priority {
  TEXEC_EXECUTOR_SUBMIT_PRIORITY_LOW = -1,
  TEXEC_EXECUTOR_SUBMIT_PRIORITY_NORMAL = 0,
  TEXEC_EXECUTOR_SUBMIT_PRIORITY_HIGH = 1
} texec_executor_submit_priority_t;

typedef struct texec_executor_submit_priority_info {
  texec_structure_header_t header;
  texec_executor_submit_priority_t priority;
} texec_executor_submit_priority_info_t;

typedef struct texec_executor_submit_deadline_info {
  texec_structure_header_t header;
  uint64_t deadline_ns;
} texec_executor_submit_deadline_info_t;

typedef struct texec_executor_submit_trace_context_info {
  texec_structure_header_t header;
  const void* trace_context;
} texec_executor_submit_trace_context_info_t;

#ifdef __cplusplus
}
#endif
