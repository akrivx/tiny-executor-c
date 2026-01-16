#include "texec/base.h"
#include "allocator_internal.h"

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

static texec_allocator_t standard_allocator = {
  .user = NULL,
  .alloc = &standard_alloc,
  .free = &standard_free
};

static texec_allocator_t* default_allocator = &standard_alloc;

void texec_set_default_allocator(texec_allocator_t* allocator) {
  if (allocator) {
    default_allocator = allocator;
  }
}

texec_allocator_t* texec__get_default_allocator(void) {
  return default_allocator;
}
