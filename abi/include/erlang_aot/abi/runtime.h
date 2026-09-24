#pragma once
#include "status.h"
#include "v1.h"
#include <stddef.h>

/* Own runtime-wide services; destroy all contexts before explicit shutdown. */
typedef struct eaot_v1_runtime eaot_v1_runtime;

typedef struct eaot_v1_runtime_options {
    /* Require the header's ABI version and this runtime target's term width. */
    uint32_t abi_version;
    uint32_t term_bits;
    /* Bound simultaneously live contexts; zero is invalid. Null options select 1024. */
    size_t max_contexts;
} eaot_v1_runtime_options;

typedef struct eaot_v1_context_options {
    /* Reserve byte-based heap budgets; backing storage remains lazy until allocation is implemented. */
    size_t heap_chunk_bytes;
    size_t heap_limit_bytes;
} eaot_v1_context_options;

#ifdef __cplusplus
extern "C" {
#define EAOT_RUNTIME_NOEXCEPT noexcept
#else
#define EAOT_RUNTIME_NOEXCEPT
#endif

/* Publish only on success; output must address a null handle. Null options select defaults. */
eaot_v1_status EAOT_V1_CALL eaot_v1_runtime_start(const eaot_v1_runtime_options *options,
                                                  eaot_v1_runtime **output) EAOT_RUNTIME_NOEXCEPT;
/* BUSY preserves the live runtime; successful or repeated shutdown clears/keeps the caller's null handle. */
eaot_v1_status EAOT_V1_CALL eaot_v1_runtime_shutdown(eaot_v1_runtime **runtime) EAOT_RUNTIME_NOEXCEPT;
/* Require a live runtime and an initially null output; the runtime owns the resulting stable context. */
eaot_v1_status EAOT_V1_CALL eaot_v1_context_create(eaot_v1_runtime *runtime, const eaot_v1_context_options *options,
                                                   eaot_v1_context **output) EAOT_RUNTIME_NOEXCEPT;
/* Reject another runtime's context without dereferencing it; success invalidates lifetime tokens and clears the handle.
 */
eaot_v1_status EAOT_V1_CALL eaot_v1_context_destroy(eaot_v1_runtime *runtime,
                                                    eaot_v1_context **context) EAOT_RUNTIME_NOEXCEPT;

#undef EAOT_RUNTIME_NOEXCEPT
#ifdef __cplusplus
}
#endif
