#include "texec/base.h"
#include "internal/allocator.h"

#include <stdlib.h>

static void* standard_alloc(void* user, size_t size, size_t align) {
  (void)user;
  (void)align;
  return malloc(size);
}

static void standard_free(void* user, void* ptr, size_t size, size_t align) {
  (void)user;
  (void)size;
  (void)align;
  free(ptr);
}

static const texec_allocator_t standard_allocator = {
  .user = NULL,
  .alloc = &standard_alloc,
  .free = &standard_free
};

static const texec_allocator_t* default_allocator = &standard_allocator;

void texec_set_default_allocator(const texec_allocator_t* allocator) {
  if (allocator) {
    default_allocator = allocator;
  }
}

const texec_allocator_t* texec_get_default_allocator(void) {
  return default_allocator;
}
