#pragma once
#include "code_server.hpp"
#include "runtime.hpp"
#include <erlang_aot/abi/modules.hpp>

namespace erlang_aot::runtime {
// Copy descriptor metadata and publish one frozen registry; retain image ownership through every handle.
CodeResult<std::shared_ptr<const LoadedModule>>
register_module(Runtime &runtime, const abi::v1::ModuleDescriptor &descriptor,
                std::shared_ptr<const CodeImage> image = CodeImage::linked());
} // namespace erlang_aot::runtime
