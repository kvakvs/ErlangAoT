#pragma once

namespace erlang_aot::codegen {
class Compilation;

// Mandatory gate before each emission: check current target settings, function bodies and whole modules.
// Never cache success across IR mutations; failure diagnoses and invalidates all staged batch output.
bool verify_ir(Compilation &compilation);
} // namespace erlang_aot::codegen
