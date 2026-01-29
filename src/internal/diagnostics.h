#pragma once

#include "texec/diagnostics.h"

static inline void texec_diagnostics_on_submit(const texec_diagnostics_t* diag, const struct texec_submit_info_t* submit_info) {
  if (!diag) return;
  diag->on_submit(diag->user, submit_info);
}

static inline void texec_diagnostics_on_task_begin(const texec_diagnostics_t* diag, const struct texec_task* task, const void* trace_context) {
  if (!diag) return;
  diag->on_task_begin(diag->user, task, trace_context);
}

static inline void texec_diagnostics_on_task_end(const texec_diagnostics_t* diag, const struct texec_task* task, const void* trace_context, int task_result) {
  if (!diag) return;
  diag->on_task_end(diag->user, task, trace_context, task_result);
}
