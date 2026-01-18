#include "texec/executor.h"

#include <stddef.h>

#include "internal/allocator.h"
#include "executor_internal.h"

static const size_t TP_EXECUTOR_DEFAULT_THREAD_COUNT = 1;
static const size_t TP_EXECUTOR_DEFAULT_QUEUE_CAPACITY = 1024;

static inline const texec_executor_create_thread_pool_info_t*
find_executor_thread_pool_info(const texec_executor_create_info_t* info) {
  return texec_structure_find(info->header.next, TEXEC_STRUCTURE_TYPE_EXECUTOR_CREATE_THREAD_POOL_INFO);
}

static inline const texec_executor_create_allocator_info_t*
find_executor_alloc_info(const texec_executor_create_info_t* info) {
  return texec_structure_find(info->header.next, TEXEC_STRUCTURE_TYPE_EXECUTOR_CREATE_ALLOCATOR_INFO);
}

static inline const texec_executor_create_diagnostics_info_t*
find_executor_diag_info(const texec_executor_create_info_t* info) {
  return texec_structure_find(info->header.next, TEXEC_STRUCTURE_TYPE_EXECUTOR_CREATE_DIAGNOSTICS_INFO);
}

static inline texec_status_t executor_create_thread_pool(const texec_allocator_t* alloc,
                                                         const texec_executor_diagnostics_t* diag,
                                                         const texec_executor_create_info_t* info,
                                                         texec_executor_t* ex) {
  const texec_executor_create_thread_pool_info_t* tp_info = find_executor_thread_pool_info(info);
  if (!tp_info) return TEXEC_STATUS_INVALID_ARGUMENT;

  const texec__thread_pool_executor_config_t cfg = {
    .alloc = alloc,
    .diag = diag,
    .thread_count = tp_info->thread_count ? tp_info->thread_count : TP_EXECUTOR_DEFAULT_THREAD_COUNT,
    .queue_capacity = tp_info->queue_capacity ? tp_info->queue_capacity : TP_EXECUTOR_DEFAULT_QUEUE_CAPACITY,
    .backpressure_policy = tp_info->backpressure_policy
  };

  return texec__executor_create_thread_pool(&cfg, ex);
}

texec_status_t texec_executor_create(const texec_executor_create_info_t* info, texec_executor_t** out_executor) {
  if (!out_executor) return TEXEC_STATUS_INVALID_ARGUMENT;
  *out_executor = NULL;

  if (!info || info->header.type != TEXEC_STRUCTURE_TYPE_EXECUTOR_CREATE_INFO) {
    return TEXEC_STATUS_INVALID_ARGUMENT;
  }

  if (info->kind != TEXEC_EXECUTOR_KIND_THREAD_POOL) {
    return TEXEC_STATUS_UNSUPPORTED;
  }

  const texec_executor_create_diagnostics_info_t* diag_info = find_executor_diag_info(info);
  const texec_executor_create_allocator_info_t* alloc_info = find_executor_alloc_info(info);
  if (alloc_info && !alloc_info->allocator) return TEXEC_STATUS_INVALID_ARGUMENT;
  
  const texec_executor_diagnostics_t* diag = diag_info ? diag_info->diag : NULL;
  const texec_allocator_t* alloc = alloc_info ? alloc_info->allocator : texec_get_default_allocator();

  texec_executor_t* ex = texec_allocate(alloc, sizeof(*ex), _Alignof(texec_executor_t));
  if (!ex) return TEXEC_STATUS_OUT_OF_MEMORY;

  ex->vtbl = NULL;
  ex->impl = NULL;

  texec_status_t st = TEXEC_STATUS_UNSUPPORTED;
  switch (info->kind)
  {
  case TEXEC_EXECUTOR_KIND_THREAD_POOL:
    st = executor_create_thread_pool(alloc, diag, info, ex);
    break;
  default:
    break;
  }

  if (!ex->impl
    || !ex->vtbl
    || !ex->vtbl->submit
    || !ex->vtbl->submit_many
    || !ex->vtbl->shutdown
    || !ex->vtbl->await_termination
    || !ex->vtbl->destroy
    || !ex->vtbl->query)
  {
    st = TEXEC_STATUS_INTERNAL_ERROR;
  }

  if (st != TEXEC_STATUS_OK) {
    texec_free(alloc, ex, sizeof(*ex), _Alignof(texec_executor_t));
  } else {
    *out_executor = ex;
  }
  return st;
}

texec_status_t texec_executor_submit(texec_executor_t* ex, const texec_executor_submit_info_t* info, texec_task_handle_t** out_handle) {
  if (!ex || !out_handle) return TEXEC_STATUS_INVALID_ARGUMENT;
  return ex->vtbl->submit(ex->impl, info, out_handle);
}

texec_status_t texec_executor_submit_many(texec_executor_t* ex, const texec_executor_submit_info_t* infos, size_t count, texec_task_group_t** out_group) {
  if (!ex || !out_group) return TEXEC_STATUS_INVALID_ARGUMENT;
  return ex->vtbl->submit_many(ex->impl, infos, count, out_group);
}

void texec_executor_shutdown(texec_executor_t* ex) {
  if (!ex) return;
  ex->vtbl->shutdown(ex->impl);
}

void texec_executor_await_termination(texec_executor_t* ex) {
  if (!ex) return;
  ex->vtbl->await_termination(ex->impl);
}

void texec_executor_destroy(texec_executor_t* ex) {
  if (!ex) return;
  ex->vtbl->destroy(ex->impl);
  texec_free(ex->impl->alloc, ex, sizeof(*ex), _Alignof(texec_executor_t));
}

texec_status_t texec_executor_query(const texec_executor_t* ex, texec_executor_capability_t cap, void* out_value) {
  if (!ex || !out_value) return TEXEC_STATUS_INVALID_ARGUMENT;
  return ex->vtbl->query(ex->impl, cap, out_value);
}
