#include <expected>
#include <windows.h>

// Verify the SDK import library and the C++23 library required by the runtime.
int main() {
    const std::expected<DWORD, int> process = GetCurrentProcessId();
    return process.has_value() ? 0 : 1;
}
