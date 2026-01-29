#include "texec/texec.h"

#include <stdio.h>

static int hello_task(void* user) {
  const char* label = (const char*)user;
  printf("texec example: %s\n", label ? label : "hello");
  return 0;
}

int main(void) {
  const texec_executor_create_thread_pool_info_t tpci = {
    .header = {.type = TEXEC_STRUCTURE_TYPE_EXECUTOR_CREATE_THREAD_POOL_INFO, .next = NULL},
    .thread_count = 2,
    .queue_capacity = 32,
    .backpressure = TEXEC_BACKPRESSURE_BLOCK,
  };

  const texec_executor_create_info_t eci = {
    .header = {.type = TEXEC_STRUCTURE_TYPE_EXECUTOR_CREATE_INFO, .next = &tpci},
    .kind = TEXEC_EXECUTOR_KIND_THREAD_POOL,
  };

  texec_executor_t* ex = NULL;
  texec_status_t st = texec_executor_create(&eci, NULL, &ex);

  if (st != TEXEC_STATUS_OK) {
    fprintf(stderr, "failed to create executor: %d\n", (int)st);
    return 1;
  }

  const texec_submit_info_t submit = {
    .header = {.type = TEXEC_STRUCTURE_TYPE_SUBMIT_INFO, .next = NULL},
    .task = {
      .fn = hello_task,
      .ctx = "work item",
    },
  };

  texec_task_handle_t* handle = NULL;
  st = texec_executor_submit(ex, &submit, &handle);

  int ret = 0;
  if (st == TEXEC_STATUS_OK) {
    texec_task_handle_wait(handle);

    int result = -1;
    st = texec_task_handle_result(handle, &result);
    if (st == TEXEC_STATUS_OK) {
      printf("task returned %d\n", result);
    } else {
      fprintf(stderr, "failed to get task result", (int)st);
    }

    texec_task_handle_release(handle);
  } else {
    fprintf(stderr, "submit failed: %d\n", (int)st);
    ret = 1;
  }

  texec_executor_close(ex);
  texec_executor_join(ex);
  texec_executor_destroy(ex);

  return ret;
}
