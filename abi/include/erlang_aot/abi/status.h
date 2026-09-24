#pragma once
#include <stdint.h>

/* Fixed-width transport for generated service failures; never substitute a term for an error. */
typedef uint32_t eaot_v1_status;
#define EAOT_V1_STATUS_OK UINT32_C(0)
#define EAOT_V1_STATUS_NOT_IMPLEMENTED UINT32_C(1)
#define EAOT_V1_STATUS_INVALID_ARGUMENT UINT32_C(2)
#define EAOT_V1_STATUS_DIAGNOSTIC_FAILURE UINT32_C(3)
