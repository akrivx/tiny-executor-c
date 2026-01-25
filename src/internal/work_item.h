#pragma once

#include "texec/base.h"
#include "texec/task.h"
#include "texec/task_handle.h"

#include "internal/allocator.h"

typedef struct texec_work_item {
  texec_task_t task;
  texec_task_handle_t* handle;
  const void* trace_context;
} texec_work_item_t;

static inline texec_work_item_t* texec_work_item_allocate(const texec_allocator_t* alloc) {
  return texec_allocate(alloc, sizeof(texec_work_item_t), _Alignof(texec_work_item_t));
}

static inline void texec_work_item_destroy(texec_work_item_t* wi, const texec_allocator_t* alloc) {
  texec_task_handle_release(wi->handle);
  texec_free(alloc, wi, sizeof(*wi), _Alignof(texec_work_item_t));
}
