#include "../project/paths.hpp"
#include "toolchain.hpp"
#include <llvm/Object/Archive.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/TargetParser/Triple.h>
#include <stdexcept>

namespace erlang_aot::linking {
namespace {
struct ObjectKind {
    // Architecture and container format of one native archive member.
    llvm::Triple::ArchType arch = llvm::Triple::UnknownArch;
    llvm::Triple::ObjectFormatType format = llvm::Triple::UnknownObjectFormat;
};

struct Survey {
    // Count native members and keep the first one that cannot link with the target.
    std::size_t objects = 0;
    std::optional<ObjectKind> mismatch;
};

// Locate this compiler executable; its address anchors the lookup on hosts without /proc.
std::filesystem::path compiler_directory() {
    static int anchor = 0;
    return project::native_path(llvm::sys::fs::getMainExecutable(nullptr, &anchor)).parent_path();
}

} // namespace

std::filesystem::path library_directory() {
    return (compiler_directory() / project::native_path(ERLANG_AOT_DEFAULT_LIBRARY)).lexically_normal();
}

namespace {
// Classify a member as a native object; bitcode, import and other members are skipped.
std::optional<ObjectKind> member_kind(const llvm::object::Archive::Child &child) {
    auto binary = child.getAsBinary();
    if (!binary) {
        llvm::consumeError(binary.takeError());
        return std::nullopt;
    }
    const auto *object = llvm::dyn_cast<llvm::object::ObjectFile>(binary->get());
    if (object == nullptr) {
        return std::nullopt;
    }
    return ObjectKind{object->getArch(), object->getTripleObjectFormat()};
}

// Inspect members until the first mismatch; malformed archives throw.
Survey survey(const llvm::object::Archive &archive, const llvm::Triple &target) {
    Survey result;
    llvm::Error error = llvm::Error::success();
    for (const auto &child : archive.children(error)) {
        const auto kind = member_kind(child);
        if (!kind) {
            continue;
        }
        ++result.objects;
        if (kind->arch != target.getArch() || kind->format != target.getObjectFormat()) {
            result.mismatch = kind;
            break;
        }
    }
    if (error) {
        throw std::runtime_error("cannot read runtime library members: " + llvm::toString(std::move(error)));
    }
    return result;
}

// Describe a member kind as "<arch> <format>" for mismatch diagnostics.
std::string describe(const ObjectKind &kind) {
    return llvm::Triple::getArchTypeName(kind.arch).str() + " " +
           llvm::Triple::getObjectFormatTypeName(kind.format).str();
}
} // namespace

std::filesystem::path find_runtime_library(const std::optional<std::filesystem::path> &library) {
    const auto path =
        library ? project::absolute_path(std::filesystem::current_path(), *library)
                : (compiler_directory() / project::native_path(ERLANG_AOT_DEFAULT_RUNTIME)).lexically_normal();
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        throw std::runtime_error("runtime library not found: " + project::path_text(path) +
                                 "; build the erlang_runtime target or pass --runtime-library");
    }
    return path;
}

void check_runtime_target(const std::filesystem::path &library, const std::string &target_triple) {
    const auto name = project::path_text(library);
    auto buffer = llvm::MemoryBuffer::getFile(utf8_path(library));
    if (!buffer) {
        throw std::runtime_error("cannot read runtime library " + name + ": " + buffer.getError().message());
    }
    auto archive = llvm::object::Archive::create((*buffer)->getMemBufferRef());
    if (!archive) {
        throw std::runtime_error("runtime library is not a static library: " + name + ": " +
                                 llvm::toString(archive.takeError()));
    }
    const auto result = survey(**archive, llvm::Triple(target_triple));
    if (result.objects == 0) {
        throw std::runtime_error("runtime library contains no native objects: " + name);
    }
    if (result.mismatch) {
        throw std::runtime_error("runtime library " + name + " contains " + describe(*result.mismatch) +
                                 " objects, but the executable targets " + target_triple +
                                 "; pass --runtime-library built for that target");
    }
}
} // namespace erlang_aot::linking
