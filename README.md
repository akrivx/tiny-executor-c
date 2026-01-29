# tiny-executor-c (texec)

A modern C executor library with a minimal public API and extensible execution backends.

## Highlights
- Small, C17/C23-friendly API surface.
- Pluggable executors (inline + thread pool).
- Task handles, task groups, and a bounded queue implementation.
- Structured "pNext" style extension chains for future features.
- Optional diagnostics hooks (submit/begin/end).

## Quick start

```c
#include "texec/texec.h"
#include <stdio.h>

static int hello_task(void* user) {
  const char* label = (const char*)user;
  printf("texec example: %s\n", label ? label : "hello");
  return 0;
}

int main(void) {
  const texec_executor_create_thread_pool_info_t tpci = {
    .header = {.type = TEXEC_STRUCT_TYPE_EXECUTOR_CREATE_THREAD_POOL_INFO, .next = NULL},
    .thread_count = 2,
    .queue_capacity = 32,
    .backpressure = TEXEC_BACKPRESSURE_BLOCK,
  };

  const texec_executor_create_info_t eci = {
    .header = {.type = TEXEC_STRUCT_TYPE_EXECUTOR_CREATE_INFO, .next = &tpci},
    .kind = TEXEC_EXECUTOR_KIND_THREAD_POOL,
  };

  texec_executor_t* ex = NULL;
  texec_status_t st = texec_executor_create(&eci, NULL, &ex);
  if (st != TEXEC_STATUS_OK) return 1;

  const texec_submit_info_t submit = {
    .header = {.type = TEXEC_STRUCT_TYPE_SUBMIT_INFO, .next = NULL},
    .task = {.run = hello_task, .ctx = "work item"},
  };

  texec_task_handle_t* handle = NULL;
  st = texec_executor_submit(ex, &submit, &handle);

  if (st == TEXEC_STATUS_OK) {
    int result = -1;
    if (texec_task_handle_result(handle, &result) == TEXEC_STATUS_OK) {
      printf("task returned %d\n", result);
    }
    texec_task_handle_release(handle);
  }

  texec_executor_close(ex);
  texec_executor_join(ex);
  texec_executor_destroy(ex);
  return 0;
}
```

## Build

Requirements:
- CMake 3.25+
- C compiler with C17 (C23 if available on MSVC)

```bash
cmake -S . -B out
cmake --build out --config Release
```

Build examples (enabled by default):
```bash
cmake -S . -B out -DTEXEC_BUILD_EXAMPLES=ON
cmake --build out --config Release
```

## Install (CMake)

```bash
cmake -S . -B out -DCMAKE_INSTALL_PREFIX=... 
cmake --build out --target install --config Release
```

CMake target:
```cmake
find_package(texec CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE texec::texec)
```

## Core concepts

### Executors
Executors are created with a base `texec_executor_create_info_t` and an optional chained extension struct (`header.next`) for backend-specific options.

Available kinds:
- `TEXEC_EXECUTOR_KIND_INLINE`
- `TEXEC_EXECUTOR_KIND_THREAD_POOL`

Thread pool options:
- `thread_count`
- `queue_capacity`
- `backpressure` (`REJECT`, `BLOCK`, `CALLER_RUNS`)

### Tasks
A task is just a function pointer and a context:
```c
typedef int (*texec_task_run_t)(void* ctx);
typedef void (*texec_task_on_complete_fn_t)(void* ctx);

typedef struct texec_task {
  texec_task_run_t run;
  void* ctx;
  texec_task_on_complete_fn_t on_complete; // optional
} texec_task_t;
```

### Task handles
Submit returns a handle you can wait on:
- `texec_task_handle_wait`
- `texec_task_handle_result`
- `texec_task_handle_try_result`
- `texec_task_handle_is_done`
- `texec_task_handle_retain` / `texec_task_handle_release`

### Task groups
You can create a group, add handles, and wait for the group:
- `texec_task_group_create`
- `texec_task_group_add`
- `texec_task_group_wait`

### Queue
A small, thread-safe bounded queue (push/pop and try variants). Useful for building your own abstractions.

## Extensions (pNext chains)
Many structs have a `header` with a `type` and `next`. You can chain optional structs to enable features. Example:

```c
texec_submit_priority_info_t pri = {
  .header = {.type = TEXEC_STRUCT_TYPE_SUBMIT_PRIORITY, .next = NULL},
  .priority = TEXEC_SUBMIT_PRIORITY_HIGH,
};

texec_submit_info_t submit = {
  .header = {.type = TEXEC_STRUCT_TYPE_SUBMIT_INFO, .next = &pri},
  .task = { .run = my_task, .ctx = data },
};
```

## Diagnostics
Attach callbacks to observe submissions and task lifecycle:

```c
texec_diagnostics_t diag = {
  .on_submit = my_on_submit,
  .on_task_begin = my_on_begin,
  .on_task_end = my_on_end,
};

texec_executor_create_diagnostics_info_t dci = {
  .header = {.type = TEXEC_STRUCT_TYPE_EXECUTOR_CREATE_DIAGNOSTICS_INFO, .next = NULL},
  .diag = &diag,
};
```

## Error handling
All public API calls return `texec_status_t`. Common values:
- `TEXEC_STATUS_OK`
- `TEXEC_STATUS_NOT_READY`
- `TEXEC_STATUS_REJECTED`
- `TEXEC_STATUS_BUSY`
- `TEXEC_STATUS_CLOSED`
- `TEXEC_STATUS_UNSUPPORTED`
- `TEXEC_STATUS_INVALID_ARGUMENT`
- `TEXEC_STATUS_OUT_OF_MEMORY`
- `TEXEC_STATUS_INTERNAL_ERROR`

## Allocators
Provide custom allocation hooks (optional):

```c
texec_allocator_t alloc = {
  .user = my_user_ptr,
  .allocate = my_alloc,
  .free = my_free,
};

texec_executor_create(&eci, &alloc, &ex);
```

You can also override the global default with `texec_set_default_allocator`.

## Threading and lifecycle
- `texec_executor_close(ex)` stops new submissions.
- `texec_executor_join(ex)` waits for in-flight tasks.
- `texec_executor_destroy(ex)` frees resources.

## Versioning
Current version: 0.1.0.

## Examples
See `examples/texec_example.c`.

## License
MIT. See `LICENSE`.
