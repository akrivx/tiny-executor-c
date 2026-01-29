#pragma once

#include <stddef.h>

#include "texec/base.h"
#include "texec/executor_create_info.h"
#include "texec/executor_submit_info.h"
#include "texec/task_handle.h"
#include "texec/task_group.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct texec_executor texec_executor_t;

texec_status_t texec_executor_create(const texec_executor_create_info_t* info, const texec_allocator_t* allocator, texec_executor_t** out_executor);
texec_status_t texec_executor_destroy(texec_executor_t* ex);
texec_status_t texec_executor_submit(texec_executor_t* ex, const texec_submit_info_t* info, texec_task_handle_t** out_handle);
texec_status_t texec_executor_submit_many(texec_executor_t* ex, const texec_submit_info_t* infos, size_t count, texec_task_group_t** out_group);
void texec_executor_close(texec_executor_t* ex);
void texec_executor_join(texec_executor_t* ex);

typedef enum texec_executor_capability {
  TEXEC_EXECUTOR_CAPABILITY_WORKER_COUNT = 1,  // out: size_t
  TEXEC_EXECUTOR_CAPABILITY_SUPPORTS_PRIORITY, // out: bool
  TEXEC_EXECUTOR_CAPABILITY_SUPPORTS_DEADLINE, // out: bool
  TEXEC_EXECUTOR_CAPABILITY_SUPPORTS_TRACING   // out: bool
} texec_executor_capability_t;

texec_status_t texec_executor_query(const texec_executor_t* ex, texec_executor_capability_t cap, void* out_value);

#ifdef __cplusplus
}
#endif
