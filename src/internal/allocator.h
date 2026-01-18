#pragma once

#include <stddef.h>
#include "texec/base.h"

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

const texec_allocator_t* texec_get_default_allocator(void);

static inline void* texec_allocate(const texec_allocator_t* allocator, size_t size, size_t align) {
  return allocator->alloc(allocator->user, size, align);
}

static inline void texec_free(const texec_allocator_t* allocator, void* ptr, size_t size, size_t align) {
  allocator->free(allocator->user, ptr, size, align);
}

#ifdef __cplusplus
}
#endif __cplusplus
