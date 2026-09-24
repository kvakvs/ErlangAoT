#pragma once
#include <stdint.h>

/* Fixed-width transport for generated service failures; never substitute a term for an error. */
typedef uint32_t eaot_v1_status;
#define EAOT_V1_STATUS_OK UINT32_C(0)
#define EAOT_V1_STATUS_NOT_IMPLEMENTED UINT32_C(1)
#define EAOT_V1_STATUS_INVALID_ARGUMENT UINT32_C(2)
#define EAOT_V1_STATUS_DIAGNOSTIC_FAILURE UINT32_C(3)

/* Lifecycle failures extend the versioned values without changing reporting status codes 0–3. */
#define EAOT_V1_STATUS_OUT_OF_MEMORY UINT32_C(4)
#define EAOT_V1_STATUS_BUSY UINT32_C(5)
#define EAOT_V1_STATUS_WRONG_OWNER UINT32_C(6)
#define EAOT_V1_STATUS_RESOURCE_LIMIT UINT32_C(7)
#define EAOT_V1_STATUS_STOPPED UINT32_C(8)
#define EAOT_V1_STATUS_ABI_MISMATCH UINT32_C(9)
#define EAOT_V1_STATUS_INTERNAL_ERROR UINT32_C(10)
