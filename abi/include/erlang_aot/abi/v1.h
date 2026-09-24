#pragma once
#include <stdint.h>

#define EAOT_ABI_VERSION 1u
#define EAOT_V1_PRIMARY_MASK 0x3u
#define EAOT_V1_SMALL_INTEGER_TAG 0xfu
#define EAOT_V1_SMALL_INTEGER_BITS 4u

/* Generated functions use the platform C convention, including Windows x86. */
#if defined(_WIN32)
#define EAOT_V1_CALL __cdecl
#else
#define EAOT_V1_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Match the runtime target's unsigned pointer width; never use the compiler host width. */
typedef uintptr_t eaot_v1_term;
/* A live runtime-owned process; its definition never crosses the generated-code boundary. */
typedef struct eaot_v1_context eaot_v1_context;
/* Arity belongs to the resolved identity; zero arguments permit a null array. */
typedef eaot_v1_term(EAOT_V1_CALL eaot_v1_function)(eaot_v1_context *, const eaot_v1_term *);

#ifdef __cplusplus
}
static_assert(sizeof(eaot_v1_term) == 4 || sizeof(eaot_v1_term) == 8);
static_assert(alignof(eaot_v1_term) == sizeof(eaot_v1_term));
#endif
