#include <erlang_aot/runtime/callable.hpp>

namespace erlang_aot::runtime {
namespace {
// Construct the only supported signature without inferring types from term contents.
FunctionKey generic_key(std::string_view name, std::size_t arity) {
    return {std::string(name), arity, std::vector<std::type_index>(arity, typeid(Term))};
}
} // namespace

RegistryResult<void> ModuleRegistry::add(std::string_view name, std::size_t arity, Callable &&target) {
    if (frozen_) {
        return std::unexpected(RegistryError::frozen);
    }
    if (name.empty() || arity > 255 || !target) {
        return std::unexpected(RegistryError::invalid_entry);
    }
    try {
        if (!entries_.try_emplace(generic_key(name, arity), std::move(target)).second) {
            return std::unexpected(RegistryError::duplicate_key);
        }
        return {};
    } catch (const std::bad_alloc &) {
        return std::unexpected(RegistryError::resource_limit);
    }
}

RegistryResult<const Callable *> ModuleRegistry::find(std::string_view name, std::size_t arity) const {
    if (arity > 255) {
        return std::unexpected(RegistryError::invalid_entry);
    }
    try {
        const auto found = entries_.find(generic_key(name, arity));
        if (found == entries_.end()) {
            return std::unexpected(RegistryError::function_not_found);
        }
        return &found->second;
    } catch (const std::bad_alloc &) {
        return std::unexpected(RegistryError::resource_limit);
    }
}

std::vector<FunctionKey> ModuleRegistry::keys() const {
    std::vector<FunctionKey> result;
    result.reserve(entries_.size());
    for (const auto &entry : entries_) {
        result.push_back(entry.first);
    }
    return result;
}

bool ModuleRegistry::frozen() const noexcept { return frozen_; }

void ModuleRegistry::freeze() noexcept { frozen_ = true; }
} // namespace erlang_aot::runtime
