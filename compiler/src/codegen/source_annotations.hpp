#pragma once
#include "source_locations.hpp"
#include <cstddef>
#include <erlang_aot/compiler/ast/module.hpp>
#include <llvm/IR/AssemblyAnnotationWriter.h>
#include <map>
#include <vector>

namespace llvm {
class Module;
class raw_ostream;
} // namespace llvm

namespace erlang_aot::codegen {
// Prepare source comments before entering LLVM's assembly writer, which must not receive C++ exceptions.
class SourceAnnotations final : public llvm::AssemblyAnnotationWriter {
  public:
    // Match surviving instruction locations to owned sources and bound aggregate comment storage.
    SourceAnnotations(const llvm::Module &module, const ast::Module &syntax, const SourceScopes &sources,
                      std::size_t capacity);
    // List physical source filenames once at the beginning of the textual snapshot.
    void print_sources(llvm::raw_ostream &stream) const;
    // Append the original source line beside its instruction without allocating inside LLVM.
    void printInfoComment(const llvm::Value &value, llvm::formatted_raw_ostream &stream) noexcept override;

  private:
    // Own the bounded filename header separately from instruction comments.
    std::vector<std::byte> header_;
    // Own preformatted comments for this snapshot; LLVM instructions remain borrowed until printing ends.
    std::map<const llvm::Value *, std::vector<std::byte>> comments_;
};
} // namespace erlang_aot::codegen
