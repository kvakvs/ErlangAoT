#include <erlang_aot/abi/runtime.h>
#include <erlang_aot/runtime/runtime.hpp>

using erlang_aot::runtime::HeapOptions;
using erlang_aot::runtime::ProcessContext;
using erlang_aot::runtime::Runtime;
using erlang_aot::runtime::RuntimeOptions;

extern "C" eaot_v1_status EAOT_V1_CALL eaot_v1_runtime_start(const eaot_v1_runtime_options *options,
                                                             eaot_v1_runtime **output) noexcept {
    if (output == nullptr || *output != nullptr) {
        return EAOT_V1_STATUS_INVALID_ARGUMENT;
    }
    if (options != nullptr &&
        (options->abi_version != EAOT_ABI_VERSION || options->term_bits != sizeof(eaot_v1_term) * 8)) {
        return EAOT_V1_STATUS_ABI_MISMATCH;
    }
    const auto settings = options == nullptr ? RuntimeOptions{} : RuntimeOptions{options->max_contexts};
    auto result = Runtime::start(settings);
    if (!result) {
        return result.error();
    }
    *output = reinterpret_cast<eaot_v1_runtime *>(result->release());
    return EAOT_V1_STATUS_OK;
}

extern "C" eaot_v1_status EAOT_V1_CALL eaot_v1_runtime_shutdown(eaot_v1_runtime **runtime) noexcept {
    if (runtime == nullptr) {
        return EAOT_V1_STATUS_INVALID_ARGUMENT;
    }
    if (*runtime == nullptr) {
        return EAOT_V1_STATUS_OK;
    }
    auto *owner = reinterpret_cast<Runtime *>(*runtime);
    const auto status = owner->shutdown();
    if (status == EAOT_V1_STATUS_OK) {
        delete owner;
        *runtime = nullptr;
    }
    return status;
}

extern "C" eaot_v1_status EAOT_V1_CALL eaot_v1_context_create(eaot_v1_runtime *runtime,
                                                              const eaot_v1_context_options *options,
                                                              eaot_v1_context **output) noexcept {
    if (runtime == nullptr || output == nullptr || *output != nullptr) {
        return EAOT_V1_STATUS_INVALID_ARGUMENT;
    }
    const auto settings =
        options == nullptr ? HeapOptions{} : HeapOptions{options->heap_chunk_bytes, options->heap_limit_bytes};
    auto context = reinterpret_cast<Runtime *>(runtime)->create_context(settings);
    if (!context) {
        return context.error();
    }
    *output = (*context)->abi_handle();
    return EAOT_V1_STATUS_OK;
}

extern "C" eaot_v1_status EAOT_V1_CALL eaot_v1_context_destroy(eaot_v1_runtime *runtime,
                                                               eaot_v1_context **context) noexcept {
    if (runtime == nullptr || context == nullptr) {
        return EAOT_V1_STATUS_INVALID_ARGUMENT;
    }
    if (*context == nullptr) {
        return EAOT_V1_STATUS_OK;
    }
    const auto status =
        reinterpret_cast<Runtime *>(runtime)->destroy_context(reinterpret_cast<ProcessContext *>(*context));
    if (status == EAOT_V1_STATUS_OK) {
        *context = nullptr;
    }
    return status;
}
