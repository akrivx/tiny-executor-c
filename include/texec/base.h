#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum texec_status {
  TEXEC_STATUS_OK = 0,
  TEXEC_STATUS_NOT_READY,
  TEXEC_STATUS_REJECTED,
  TEXEC_STATUS_BUSY,
  TEXEC_STATUS_CLOSED,
  TEXEC_STATUS_UNSUPPORTED,
  TEXEC_STATUS_INVALID_ARGUMENT,
  TEXEC_STATUS_INVALID_STATE,
  TEXEC_STATUS_OUT_OF_MEMORY,
  TEXEC_STATUS_INTERNAL_ERROR
} texec_status_t;

typedef enum texec_structure_type {
  TEXEC_STRUCTURE_TYPE_EXECUTOR_CREATE_INFO             = 0x1000,
  TEXEC_STRUCTURE_TYPE_EXECUTOR_SUBMIT_INFO             = 0x2000,
  TEXEC_STRUCTURE_TYPE_TASK_GROUP_CREATE_INFO           = 0x3000,
  TEXEC_STRUCTURE_TYPE_QUEUE_CREATE_INFO                = 0x4000,
  
  TEXEC_STRUCTURE_TYPE_EXECUTOR_CREATE_INLINE_INFO      = 0x1001,
  TEXEC_STRUCTURE_TYPE_EXECUTOR_CREATE_THREAD_POOL_INFO = 0x1002,
  TEXEC_STRUCTURE_TYPE_EXECUTOR_CREATE_ALLOCATOR_INFO   = 0x1003,
  TEXEC_STRUCTURE_TYPE_EXECUTOR_CREATE_DIAGNOSTICS_INFO = 0x1004,
  
  TEXEC_STRUCTURE_TYPE_EXECUTOR_SUBMIT_PRIORITY         = 0x2001,
  TEXEC_STRUCTURE_TYPE_EXECUTOR_SUBMIT_DEADLINE         = 0x2002,
  TEXEC_STRUCTURE_TYPE_EXECUTOR_SUBMIT_TRACE_CONTEXT    = 0x2003,
  
  TEXEC_STRUCTURE_TYPE_TASK_GROUP_CREATE_ALLOCATOR_INFO = 0x3001,

  TEXEC_STRUCTURE_TYPE_QUEUE_CREATE_FULL_POLICY_INFO    = 0x4001,
  TEXEC_STRUCTURE_TYPE_QUEUE_CREATE_ALLOCATOR_INFO      = 0x4002,
} texec_structure_type_t;

typedef void* (*texec_alloc_fn_t)(void* user, size_t size, size_t align);
typedef void (*texec_free_fn_t)(void* user, void* ptr, size_t size, size_t align);

typedef struct texec_allocator {
  void* user;
  texec_alloc_fn_t allocate;
  texec_free_fn_t free;
} texec_allocator_t;

void texec_set_default_allocator(const texec_allocator_t* allocator);

typedef struct texec_structure_header {
  texec_structure_type_t type;
  const void* next;
} texec_structure_header_t;

static inline const void* texec_structure_find(const void* first, texec_structure_type_t type) {
  const texec_structure_header_t* it = (const texec_structure_header_t*)first;
  while (it && it->type != type) {
    it = (const texec_structure_header_t*)it->next;
  }
  return it;
}

#ifdef __cplusplus
}
#endif
