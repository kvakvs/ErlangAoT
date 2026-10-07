#pragma once
#include <erlang_aot/compiler/mangling.hpp>
#include <llvm/TargetParser/Triple.h>
#include <string_view>

// Native C++ declarations of runtime services called by generated code; mirrors abi/include/erlang_aot/abi.
namespace erlang_aot::codegen::services {
using mangling::Char;
using mangling::Const;
using mangling::Enum;
using mangling::Function;
using mangling::Int32;
using mangling::Pointer;
using mangling::Size;
using mangling::UInt8;
using mangling::Void;

// Opaque runtime/process context passed first to every service.
using Context = Pointer<Void>;
// Target-width term word (abi::v1::TermWord) inputs and output slots.
using Words = Pointer<Const<Size>>;
using Slot = Pointer<Size>;
using Bytes = Pointer<Const<Char>>;
using Descriptor = Pointer<Const<Void>>;
using Reason = Enum<"erlang_aot::abi::v1::ErrorReason">;

using Integer = Function<"erlang_aot_integer_v1", UInt8, Context, Bytes, Size, Slot>;
using Float = Function<"erlang_aot_float_v1", UInt8, Context, Bytes, Size, Slot>;
using Immediate = Function<"erlang_aot_immediate_v1", UInt8, Context, UInt8, Size, Size, Slot>;
using Construct = Function<"erlang_aot_construct_v1", UInt8, Context, UInt8, Words, Size, Slot>;
using Inspect = Function<"erlang_aot_inspect_v1", UInt8, Context, UInt8, Size, Size, Slot>;
using Bits = Function<"erlang_aot_bits_v1", UInt8, Context, UInt8, Words, Size, Slot>;
using Map = Function<"erlang_aot_map_v1", UInt8, Context, UInt8, Words, Size, Slot>;
using Record = Function<"erlang_aot_record_v1", UInt8, Context, UInt8, UInt8, Descriptor, Words, Size, Slot>;
using Display = Function<"erlang_aot_display_v1", UInt8, Context, Size, Slot>;
using Halt = Function<"erlang_aot_halt_v1", UInt8, Context, Size>;
using Exact = Function<"erlang_aot_exact_v1", UInt8, Context, Size, Size>;
using CallFailed = Function<"erlang_aot_call_failed_v2", UInt8, Context>;
using Raise = Function<"erlang_aot_raise_v2", UInt8, Context, Reason, Size>;
using Catch = Function<"erlang_aot_catch_v1", UInt8, Context, Slot>;
using Exception = Function<"erlang_aot_exception_v2", UInt8, Context, Slot, Slot, Slot>;
using Reraise = Function<"erlang_aot_reraise_v2", UInt8, Context, Size, Size, Size>;
using Error = Function<"erlang_aot_error_v1", UInt8, Context, Size, Size>;
// Function values (abi/funs.hpp): build a fun, and check a called one, returning the FrameDescriptor to enter.
using MakeFun = Function<"erlang_aot_make_fun_v1", UInt8, Context, Descriptor, Words, Size, Slot>;
using Apply = Function<"erlang_aot_apply_v1", Descriptor, Context, Size, Size, Slot>;
// Dynamic calls: M:F(Args), apply/2,3 (arguments unpacked into the registers) and fun M:F/A with variables.
using Call = Function<"erlang_aot_call_v1", Descriptor, Context, Size, Size, Size>;
using ApplyList = Function<"erlang_aot_apply_list_v1", Descriptor, Context, Size, Size, Slot>;
using CallList = Function<"erlang_aot_call_list_v1", Descriptor, Context, Size, Size, Size, Slot>;
using MakeExternalFun = Function<"erlang_aot_make_external_fun_v1", UInt8, Context, Size, Size, Size, Slot>;
// Frame transfers (abi/frames.hpp): each returns the continuation code generated code tail-calls next.
using Code = Pointer<Void>;
using Enter = Function<"erlang_aot_enter_v1", Code, Context, Descriptor>;
using Tail = Function<"erlang_aot_tail_v1", Code, Context, Descriptor>;
using Return = Function<"erlang_aot_return_v1", Code, Context, Size>;
// Loop-head safepoint: may collect and rewrite term slots (docs/runtime-heap.md#collection-in-generated-code).
using Safepoint = Function<"erlang_aot_safepoint_v1", Void, Context>;
using Frame = Function<"erlang_aot_frame_v1", Slot, Context>;
using Registers = Function<"erlang_aot_registers_v1", Slot, Context>;
using Invoke = Function<"erlang_aot_invoke_v1", Size, Context, Descriptor, Words>;
using RegisterModule = Function<"erlang_aot_register_module_v4", UInt8, Context, Descriptor>;
using Atom = Function<"erlang_aot_atom_v3", Size, Context, Size, Descriptor>;
// Program startup called by the generated native `main` (abi/startup.hpp).
using Main = Function<"erlang_aot_main_v1", Int32, Int32, Pointer<Pointer<Char>>, Descriptor>;

// Mangling family and pointer width of the emitted target, independent of the compiler host.
inline mangling::Target target(const llvm::Triple &triple) {
    return {triple.isWindowsMSVCEnvironment() ? mangling::Scheme::microsoft : mangling::Scheme::itanium,
            triple.isArch64Bit()};
}

// Linker spelling of `Service` for the emitted target.
template <typename Service> std::string_view symbol(const llvm::Triple &triple) {
    return Service::symbol(target(triple));
}
} // namespace erlang_aot::codegen::services
