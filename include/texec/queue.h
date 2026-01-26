#pragma once

#include <stdint.h>

#include "texec/base.h"
#include "texec/queue_create_info.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct texec_queue texec_queue_t;

texec_status_t texec_queue_create(const texec_queue_create_info_t* info, texec_queue_t** out_q);
texec_status_t texec_queue_destroy(texec_queue_t* q);
void texec_queue_close(texec_queue_t* q);

texec_status_t texec_queue_try_push(texec_queue_t* q, uintptr_t item);
texec_status_t texec_queue_try_pop(texec_queue_t* q, uintptr_t* out_item);

texec_status_t texec_queue_push(texec_queue_t* q, uintptr_t item);
texec_status_t texec_queue_pop(texec_queue_t* q, uintptr_t* out_item);

static inline texec_status_t texec_queue_try_push_ptr(texec_queue_t* q, void* p) {
  return texec_queue_try_push(q, (uintptr_t)p);
}

static inline texec_status_t texec_queue_try_pop_ptr(texec_queue_t* q, void** out_p) {
  uintptr_t item = 0;
  texec_status_t st = texec_queue_try_pop(q, &item);
  if (st == TEXEC_STATUS_OK) *out_p = (void*)item;
  return st;
}

static inline texec_status_t texec_queue_push_ptr(texec_queue_t* q, void* p) {
  return texec_queue_push(q, (uintptr_t)p);
}

static inline texec_status_t texec_queue_pop_ptr(texec_queue_t* q, void** out_p) {
  uintptr_t item = 0;
  texec_status_t st = texec_queue_pop(q, &item);
  if (st == TEXEC_STATUS_OK) *out_p = (void*)item;
  return st;
}

#ifdef __cplusplus
}
#endif
