#include "portions.hpp"
#include <array>

namespace clause::runtime::builtins {
namespace {
Word build_continue(ProcessContext &context, Arguments state);

// Continuation of build_list: [Tail], the list built so far.
constexpr BuiltinFrame BUILD_FRAME = continuation_frame(&guarded<build_continue>, 1);

Word build_continue(ProcessContext &context, Arguments state) {
    return build_list(context, term_of(context, state[0]));
}
} // namespace

Word count_word(ProcessContext &context, std::size_t count) {
    return need(TermFactory(context).integer(static_cast<std::int64_t>(count))).word();
}

std::size_t count_of(ProcessContext &context, Word word) {
    return static_cast<std::size_t>(need(term_of(context, word).integer_value()));
}

Word build_list(ProcessContext &context, const Term &tail) {
    auto &stack = context.stack();
    auto &state = trap_state<ListState>(context);
    auto &words = state.words();
    const auto count = std::min(words.size() - state.base, stack.budget());
    const auto first = words.size() - count;
    const auto list = need(TermFactory(context).list_words(std::span(words).subspan(first), tail));
    words.resize(first);
    stack.spend(count);
    if (words.size() > state.base) {
        stack.trap(BUILD_FRAME.frame, std::array{list.word()});
        return 0;
    }
    stack.drop_trap_state();
    return list.word();
}
} // namespace clause::runtime::builtins
