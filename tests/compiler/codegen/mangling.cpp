#include "codegen/runtime_symbols.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace erlang_aot::codegen;

// Hand-written spellings previously hardcoded in codegen, each verified against Clang declarations.
struct Expected {
    std::string_view msvc64;
    std::string_view msvc32;
    std::string_view itanium64;
    std::string_view itanium32;
};

// Triple strings parse without a configured backend, so every ABI is checked on every host.
struct Platform {
    const char *triple;
    std::string_view Expected::*spelling;
};

constexpr std::array platforms{
    Platform{"x86_64-pc-windows-msvc", &Expected::msvc64},
    Platform{"i686-pc-windows-msvc", &Expected::msvc32},
    Platform{"x86_64-unknown-linux-gnu", &Expected::itanium64},
    Platform{"aarch64-unknown-linux-gnu", &Expected::itanium64},
    Platform{"arm64-apple-macosx14.0.0", &Expected::itanium64},
    Platform{"i686-unknown-linux-gnu", &Expected::itanium32},
    Platform{"armv7-unknown-linux-gnueabihf", &Expected::itanium32},
};

// Compare one service's mangled spelling with the former literal on every supported platform.
template <typename Service> void check(const Expected &expected) {
    for (const auto &platform : platforms) {
        const auto actual = services::symbol<Service>(llvm::Triple(platform.triple));
        if (actual != expected.*platform.spelling) {
            throw std::runtime_error(std::string(platform.triple) + ": got " + std::string(actual) + ", expected " +
                                     std::string(expected.*platform.spelling));
        }
    }
}

// Every runtime service keeps the exact spelling previously hardcoded in codegen.
int main() {
    try {
        check<services::Integer>({"?erlang_aot_integer_v1@@YAEPEAXPEBD_KPEA_K@Z",
                                  "?erlang_aot_integer_v1@@YAEPAXPBDIPAI@Z", "_Z21erlang_aot_integer_v1PvPKcmPm",
                                  "_Z21erlang_aot_integer_v1PvPKcjPj"});
        check<services::Float>({"?erlang_aot_float_v1@@YAEPEAXPEBD_KPEA_K@Z", "?erlang_aot_float_v1@@YAEPAXPBDIPAI@Z",
                                "_Z19erlang_aot_float_v1PvPKcmPm", "_Z19erlang_aot_float_v1PvPKcjPj"});
        check<services::Immediate>({"?erlang_aot_immediate_v1@@YAEPEAXE_K1PEA_K@Z",
                                    "?erlang_aot_immediate_v1@@YAEPAXEIIPAI@Z", "_Z23erlang_aot_immediate_v1PvhmmPm",
                                    "_Z23erlang_aot_immediate_v1PvhjjPj"});
        check<services::Construct>({"?erlang_aot_construct_v1@@YAEPEAXEPEB_K_KPEA_K@Z",
                                    "?erlang_aot_construct_v1@@YAEPAXEPBIIPAI@Z",
                                    "_Z23erlang_aot_construct_v1PvhPKmmPm", "_Z23erlang_aot_construct_v1PvhPKjjPj"});
        check<services::Inspect>({"?erlang_aot_inspect_v1@@YAEPEAXE_K1PEA_K@Z",
                                  "?erlang_aot_inspect_v1@@YAEPAXEIIPAI@Z", "_Z21erlang_aot_inspect_v1PvhmmPm",
                                  "_Z21erlang_aot_inspect_v1PvhjjPj"});
        check<services::Bits>({"?erlang_aot_bits_v1@@YAEPEAXEPEB_K_KPEA_K@Z", "?erlang_aot_bits_v1@@YAEPAXEPBIIPAI@Z",
                               "_Z18erlang_aot_bits_v1PvhPKmmPm", "_Z18erlang_aot_bits_v1PvhPKjjPj"});
        check<services::Map>({"?erlang_aot_map_v1@@YAEPEAXEPEB_K_KPEA_K@Z", "?erlang_aot_map_v1@@YAEPAXEPBIIPAI@Z",
                              "_Z17erlang_aot_map_v1PvhPKmmPm", "_Z17erlang_aot_map_v1PvhPKjjPj"});
        check<services::Display>({"?erlang_aot_display_v1@@YAEPEAX_KPEA_K@Z", "?erlang_aot_display_v1@@YAEPAXIPAI@Z",
                                  "_Z21erlang_aot_display_v1PvmPm", "_Z21erlang_aot_display_v1PvjPj"});
        check<services::Halt>({"?erlang_aot_halt_v1@@YAEPEAX_K@Z", "?erlang_aot_halt_v1@@YAEPAXI@Z",
                               "_Z18erlang_aot_halt_v1Pvm", "_Z18erlang_aot_halt_v1Pvj"});
        check<services::Main>({"?erlang_aot_main_v1@@YAHHPEAPEADPEBX@Z", "?erlang_aot_main_v1@@YAHHPAPADPBX@Z",
                               "_Z18erlang_aot_main_v1iPPcPKv", "_Z18erlang_aot_main_v1iPPcPKv"});
        check<services::Exact>({"?erlang_aot_exact_v1@@YAEPEAX_K1@Z", "?erlang_aot_exact_v1@@YAEPAXII@Z",
                                "_Z19erlang_aot_exact_v1Pvmm", "_Z19erlang_aot_exact_v1Pvjj"});
        check<services::CallFailed>({"?erlang_aot_call_failed_v2@@YAEPEAX@Z", "?erlang_aot_call_failed_v2@@YAEPAX@Z",
                                     "_Z25erlang_aot_call_failed_v2Pv", "_Z25erlang_aot_call_failed_v2Pv"});
        check<services::Raise>({"?erlang_aot_raise_v2@@YAEPEAXW4ErrorReason@v1@abi@erlang_aot@@_K@Z",
                                "?erlang_aot_raise_v2@@YAEPAXW4ErrorReason@v1@abi@erlang_aot@@I@Z",
                                "_Z19erlang_aot_raise_v2PvN10erlang_aot3abi2v111ErrorReasonEm",
                                "_Z19erlang_aot_raise_v2PvN10erlang_aot3abi2v111ErrorReasonEj"});
        check<services::Catch>({"?erlang_aot_catch_v1@@YAEPEAXPEA_K@Z", "?erlang_aot_catch_v1@@YAEPAXPAI@Z",
                                "_Z19erlang_aot_catch_v1PvPm", "_Z19erlang_aot_catch_v1PvPj"});
        check<services::Exception>({"?erlang_aot_exception_v2@@YAEPEAXPEA_K11@Z",
                                    "?erlang_aot_exception_v2@@YAEPAXPAI11@Z", "_Z23erlang_aot_exception_v2PvPmS0_S0_",
                                    "_Z23erlang_aot_exception_v2PvPjS0_S0_"});
        check<services::Reraise>({"?erlang_aot_reraise_v2@@YAEPEAX_K11@Z", "?erlang_aot_reraise_v2@@YAEPAXIII@Z",
                                  "_Z21erlang_aot_reraise_v2Pvmmm", "_Z21erlang_aot_reraise_v2Pvjjj"});
        check<services::Error>({"?erlang_aot_error_v1@@YAEPEAX_K1@Z", "?erlang_aot_error_v1@@YAEPAXII@Z",
                                "_Z19erlang_aot_error_v1Pvmm", "_Z19erlang_aot_error_v1Pvjj"});
        check<services::Enter>({"?erlang_aot_enter_v1@@YAPEAXPEAXPEBX@Z", "?erlang_aot_enter_v1@@YAPAXPAXPBX@Z",
                                "_Z19erlang_aot_enter_v1PvPKv", "_Z19erlang_aot_enter_v1PvPKv"});
        check<services::Tail>({"?erlang_aot_tail_v1@@YAPEAXPEAXPEBX@Z", "?erlang_aot_tail_v1@@YAPAXPAXPBX@Z",
                               "_Z18erlang_aot_tail_v1PvPKv", "_Z18erlang_aot_tail_v1PvPKv"});
        check<services::Return>({"?erlang_aot_return_v1@@YAPEAXPEAX_K@Z", "?erlang_aot_return_v1@@YAPAXPAXI@Z",
                                 "_Z20erlang_aot_return_v1Pvm", "_Z20erlang_aot_return_v1Pvj"});
        check<services::Frame>({"?erlang_aot_frame_v1@@YAPEA_KPEAX@Z", "?erlang_aot_frame_v1@@YAPAIPAX@Z",
                                "_Z19erlang_aot_frame_v1Pv", "_Z19erlang_aot_frame_v1Pv"});
        check<services::Registers>({"?erlang_aot_registers_v1@@YAPEA_KPEAX@Z", "?erlang_aot_registers_v1@@YAPAIPAX@Z",
                                    "_Z23erlang_aot_registers_v1Pv", "_Z23erlang_aot_registers_v1Pv"});
        check<services::Invoke>({"?erlang_aot_invoke_v1@@YA_KPEAXPEBXPEB_K@Z", "?erlang_aot_invoke_v1@@YAIPAXPBXPBI@Z",
                                 "_Z20erlang_aot_invoke_v1PvPKvPKm", "_Z20erlang_aot_invoke_v1PvPKvPKj"});
        check<services::RegisterModule>(
            {"?erlang_aot_register_module_v4@@YAEPEAXPEBX@Z", "?erlang_aot_register_module_v4@@YAEPAXPBX@Z",
             "_Z29erlang_aot_register_module_v4PvPKv", "_Z29erlang_aot_register_module_v4PvPKv"});
        check<services::Atom>({"?erlang_aot_atom_v3@@YA_KPEAX_KPEBX@Z", "?erlang_aot_atom_v3@@YAIPAXIPBX@Z",
                               "_Z18erlang_aot_atom_v3PvmPKv", "_Z18erlang_aot_atom_v3PvjPKv"});
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
