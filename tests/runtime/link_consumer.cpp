#include <erlang_aot/abi/runtime.h>

// A generated-entry-shaped host function borrows the live context without exposing its definition.
eaot_v1_term EAOT_V1_CALL identity(eaot_v1_context *context, const eaot_v1_term *arguments) {
    return context == nullptr ? 0 : arguments[0];
}

// Link solely through the generated-program target and tear contexts down before their runtime.
int main() {
    eaot_v1_runtime *runtime = nullptr;
    eaot_v1_context *context = nullptr;
    if (eaot_v1_runtime_start(nullptr, &runtime) != EAOT_V1_STATUS_OK) {
        return 1;
    }
    if (eaot_v1_context_create(runtime, nullptr, &context) != EAOT_V1_STATUS_OK) {
        eaot_v1_runtime_shutdown(&runtime);
        return 2;
    }
    eaot_v1_function *entry = identity;
    const eaot_v1_term argument = 0x2af;
    const bool matched = entry(context, &argument) == argument;
    const auto destroyed = eaot_v1_context_destroy(runtime, &context);
    const auto stopped = eaot_v1_runtime_shutdown(&runtime);
    return matched && destroyed == EAOT_V1_STATUS_OK && stopped == EAOT_V1_STATUS_OK ? 0 : 3;
}
