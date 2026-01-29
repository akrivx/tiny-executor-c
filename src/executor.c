#include "texec/executor.h"

#include <stddef.h>

#include "internal/allocator.h"
#include "internal/executor.h"

static const size_t TP_EXECUTOR_DEFAULT_THREAD_COUNT = 1;
static const size_t TP_EXECUTOR_DEFAULT_QUEUE_CAPACITY = 1024;

static inline const texec_executor_create_thread_pool_info_t*
find_executor_thread_pool_create_info(const texec_executor_create_info_t* info) {
  return texec_structure_find(info->header.next, TEXEC_STRUCT_TYPE_EXECUTOR_CREATE_THREAD_POOL_INFO);
}

static inline const texec_executor_create_diagnostics_info_t*
find_executor_diag_info(const texec_executor_create_info_t* info) {
  return texec_structure_find(info->header.next, TEXEC_STRUCT_TYPE_EXECUTOR_CREATE_DIAGNOSTICS_INFO);
}

static inline texec_status_t executor_create_thread_pool(const texec_allocator_t* alloc,
                                                         const texec_diagnostics_t* diag,
                                                         const texec_executor_create_info_t* info,
                                                         texec_executor_t** out_ex) {
  const texec_executor_create_thread_pool_info_t* tp_info = find_executor_thread_pool_create_info(info);
  if (!tp_info) return TEXEC_STATUS_INVALID_ARGUMENT;

  const texec_thread_pool_executor_config_t cfg = {
    .alloc = alloc,
    .diag = diag,
    .thread_count = tp_info->thread_count ? tp_info->thread_count : TP_EXECUTOR_DEFAULT_THREAD_COUNT,
    .queue_capacity = tp_info->queue_capacity ? tp_info->queue_capacity : TP_EXECUTOR_DEFAULT_QUEUE_CAPACITY,
    .backpressure = tp_info->backpressure
  };

  return texec_executor_create_thread_pool(&cfg, out_ex);
}

static inline bool executor_validate(const texec_executor_t* ex) {
  return ex
    && ex->alloc
    && ex->vtbl
    && ex->vtbl->submit
    && ex->vtbl->submit_many
    && ex->vtbl->close
    && ex->vtbl->join
    && ex->vtbl->destroy
    && ex->vtbl->query
    && ex->state == TEXEC_EXECUTOR_STATE_RUNNING;
}

texec_status_t texec_executor_create(const texec_executor_create_info_t* info, const texec_allocator_t* alloc, texec_executor_t** out_executor) {
  if (!out_executor) return TEXEC_STATUS_INVALID_ARGUMENT;
  *out_executor = NULL;

  if (!info || info->header.type != TEXEC_STRUCT_TYPE_EXECUTOR_CREATE_INFO) {
    return TEXEC_STATUS_INVALID_ARGUMENT;
  }

  const texec_executor_create_diagnostics_info_t* diag_info = find_executor_diag_info(info);  
  const texec_diagnostics_t* diag = diag_info ? diag_info->diag : NULL;
  
  if (!alloc) {
    alloc = texec_get_default_allocator();
  } 

  texec_status_t st = TEXEC_STATUS_UNSUPPORTED;
  switch (info->kind) {
  case TEXEC_EXECUTOR_KIND_THREAD_POOL:
    st = executor_create_thread_pool(alloc, diag, info, out_executor);
    break;
  default:
    break;
  }

  if (st == TEXEC_STATUS_OK) {
    if (!executor_validate(*out_executor)) {
      // Internal error so we can't really destroy the executor
      // TODO: abort?
      *out_executor = NULL;
      st = TEXEC_STATUS_INTERNAL_ERROR;
    }
  }

  return st;
}

texec_status_t texec_executor_destroy(texec_executor_t* ex) {
  if (!ex) return TEXEC_STATUS_INVALID_ARGUMENT;
  return ex->vtbl->destroy(ex);
}

texec_status_t texec_executor_submit(texec_executor_t* ex, const texec_submit_info_t* info, texec_task_handle_t** out_handle) {
  if (!ex || !out_handle) return TEXEC_STATUS_INVALID_ARGUMENT;
  return ex->vtbl->submit(ex, info, out_handle);
}

texec_status_t texec_executor_submit_many(texec_executor_t* ex, const texec_submit_info_t* infos, size_t count, texec_task_group_t** out_group) {
  if (!ex || !out_group) return TEXEC_STATUS_INVALID_ARGUMENT;
  return ex->vtbl->submit_many(ex, infos, count, out_group);
}

void texec_executor_close(texec_executor_t* ex) {
  if (!ex) return;
  ex->vtbl->close(ex);
}

void texec_executor_join(texec_executor_t* ex) {
  if (!ex) return;
  ex->vtbl->join(ex);
}

texec_status_t texec_executor_query(const texec_executor_t* ex, texec_executor_capability_t cap, void* out_value) {
  if (!ex || !out_value) return TEXEC_STATUS_INVALID_ARGUMENT;
  return ex->vtbl->query(ex, cap, out_value);
}
