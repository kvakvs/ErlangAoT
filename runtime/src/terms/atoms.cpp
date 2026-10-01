#include <atomic>
#include <erlang_aot/runtime/atoms.hpp>
#include <limits>

namespace erlang_aot::runtime {
namespace {
// Never reuse a raw atom word, even across destroyed runtimes or aborted registrations.
TermResult<Word> reserve_word() noexcept {
    static std::atomic<Word> next{0};
    constexpr auto maximum = std::numeric_limits<Word>::max() >> 6;
    auto candidate = next.load(std::memory_order_relaxed);
    do {
        if (candidate > maximum) {
            return std::unexpected(TermError::resource_limit);
        }
    } while (!next.compare_exchange_weak(candidate, candidate + 1, std::memory_order_relaxed));
    return (candidate << 6) | Word{0xb};
}
} // namespace

TermResult<Term> AtomStorage::intern(std::string_view spelling) noexcept {
    if (!valid_atom_spelling(spelling)) {
        return std::unexpected(TermError::invalid_encoding);
    }
    const auto found = names_.find(spelling);
    if (found != names_.end()) {
        return lookup(found->second->word);
    }
    if (names_.size() >= limit_) {
        return std::unexpected(TermError::resource_limit);
    }
    const auto word = reserve_word();
    if (!word) {
        return std::unexpected(word.error());
    }
    try {
        auto value = std::make_shared<const AtomValue>(*word, spelling);
        const auto entry = names_.emplace(value->spelling, value).first;
        try {
            words_.emplace(*word, value);
        } catch (...) {
            names_.erase(entry);
            throw;
        }
        return lookup(*word);
    } catch (...) {
        return std::unexpected(TermError::resource_limit);
    }
}

TermResult<Term> AtomStorage::lookup(Word word) const noexcept {
    const auto found = words_.find(word);
    if (found == words_.end()) {
        return std::unexpected(TermError::wrong_owner);
    }
    Term result;
    result.value_ = word;
    result.atom_ = found->second;
    return result;
}
} // namespace erlang_aot::runtime
