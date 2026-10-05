#include "../semantic/capabilities.hpp"
#include "lowering_state.hpp"
#include "source_locations.hpp"
#include <algorithm>
#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/abi/term.hpp>
#include <utility>

namespace erlang_aot::codegen {
namespace {
struct Cursor {
    // One generator of a qualifier: the frame slot holding the rest of its input, the input this iteration reads
    // and the values its patterns match (the element).
    llvm::Value *slot;
    llvm::Value *input = nullptr;
    std::vector<llvm::Value *> elements = {};
};

// A new block of the function being lowered.
llvm::BasicBlock *block(const ExpressionLowering &state, const char *name) {
    return llvm::BasicBlock::Create(state.entry.getContext(), name, &state.entry);
}

// Loop state lives in frame slots, so no SSA value crosses an iteration or an Erlang call.
llvm::Value *load(const ExpressionLowering &state, llvm::Value *slot, const char *name) {
    return state.builder.CreateAlignedLoad(state.word, slot, llvm::Align(state.word->getBitWidth() / 8), name);
}

void store(const ExpressionLowering &state, llvm::Value *value, llvm::Value *slot) {
    state.builder.CreateAlignedStore(value, slot, llvm::Align(state.word->getBitWidth() / 8));
}

// The expression's value, already lowered by the walk.
llvm::Value *value_of(const ExpressionLowering &state, const ast::ExprId &id) {
    return state.values.at(&state.module.syntax->expression(id));
}

// The inputs of every generator, as a tuple for the bad_generators error.
llvm::Value *inputs(ExpressionLowering &state, const std::vector<Cursor> &cursors) {
    std::vector<llvm::Value *> values;
    std::ranges::transform(cursors, std::back_inserter(values), &Cursor::input);
    return lower_tuple(state, values);
}

// Take the next element of every input; a generator whose input is not a cons cell ends the loop at `done`.
void next_elements(ExpressionLowering &state, std::vector<Cursor> &cursors, llvm::BasicBlock *done) {
    for (auto &cursor : cursors) {
        cursor.input = load(state, cursor.slot, "generator.input");
    }
    for (auto &cursor : cursors) {
        cursor.elements = {lower_inspection(state, abi::v1::ContainerInspection::cons_head, cursor.input, 0, done)};
        store(state, lower_inspection(state, abi::v1::ContainerInspection::cons_tail, cursor.input, 0, done),
              cursor.slot);
    }
}

// From the insertion block, continue at `next` once every input is exhausted; any other input raises
// bad_generator, or bad_generators with all inputs for a zip.
void exhausted(ExpressionLowering &state, const std::vector<Cursor> &cursors, llvm::BasicBlock *next) {
    llvm::Value *empty = state.builder.getTrue();
    for (const auto &cursor : cursors) {
        auto *nil = llvm::ConstantInt::get(state.word, abi::v1::empty_list);
        auto *test = state.builder.Insert(
            llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::CmpInst::ICMP_EQ, cursor.input, nil));
        empty = state.builder.CreateAnd(empty, test);
    }
    auto *bad = block(state, "generator.bad");
    state.builder.CreateCondBr(empty, next, bad);
    state.builder.SetInsertPoint(bad);
    if (cursors.size() == 1) {
        raise_reason(state, abi::v1::ErrorReason::bad_generator, cursors.front().input);
    } else {
        raise_reason(state, abi::v1::ErrorReason::bad_generators, inputs(state, cursors));
    }
}

struct MatchEdges {
    // Named continuations of a generator match, so acceptance and rejection cannot be swapped.
    llvm::BasicBlock *success;
    llvm::BasicBlock *mismatch;
};

// Match every element against its generator's patterns in order (only the strict generators' when `only_strict`).
void match_elements(ExpressionLowering &state, std::span<const ast::Qualifier> parts,
                    const std::vector<Cursor> &cursors, const bool only_strict, const MatchEdges edges) {
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (only_strict && !semantic::strict_generator(parts[i])) {
            continue;
        }
        const auto patterns = semantic::generator_patterns(parts[i]);
        for (std::size_t j = 0; j < patterns.size(); ++j) {
            auto *matched = block(state, "generator.matched");
            const auto plan = body_pattern_plan(state, semantic::pattern_root(*state.module.syntax, patterns[j]));
            lower_match_plan(state, plan, std::array{cursors[i].elements.at(j)}, matched, edges.mismatch);
            state.builder.SetInsertPoint(matched);
        }
    }
    state.builder.CreateBr(edges.success);
}

// A rejected element is skipped unless a strict generator's pattern rejects it: then a lone generator raises
// {badmatch, Element} and a zip bad_generators with all its inputs.
void reject(ExpressionLowering &state, std::span<const ast::Qualifier> parts, const std::vector<Cursor> &cursors,
            llvm::BasicBlock *skip) {
    const auto strict = std::ranges::count_if(parts, semantic::strict_generator);
    if (strict == 0) {
        state.builder.CreateBr(skip);
    } else if (parts.size() == 1) {
        raise_reason(state, abi::v1::ErrorReason::badmatch, cursors.front().elements.front());
    } else if (std::cmp_equal(strict, parts.size())) {
        raise_reason(state, abi::v1::ErrorReason::bad_generators, inputs(state, cursors));
    } else {
        auto *bad = block(state, "generator.strict");
        match_elements(state, parts, cursors, true, {.success = skip, .mismatch = bad});
        state.builder.SetInsertPoint(bad);
        raise_reason(state, abi::v1::ErrorReason::bad_generators, inputs(state, cursors));
    }
}

// Bind the patterns of every generator and continue in the loop body; rejected elements go to `reject`.
void bind_elements(ExpressionLowering &state, std::span<const ast::Qualifier> parts, const std::vector<Cursor> &cursors,
                   llvm::BasicBlock *skip) {
    const auto before = state.bindings;
    auto *body = block(state, "generator.body");
    auto *rejected = block(state, "generator.rejected");
    match_elements(state, parts, cursors, false, {.success = body, .mismatch = rejected});
    if (rejected->use_empty()) {
        rejected->eraseFromParent();
    } else {
        auto matched = std::move(state.bindings);
        state.bindings = before;
        state.builder.SetInsertPoint(rejected);
        reject(state, parts, cursors, skip);
        state.bindings = std::move(matched);
    }
    state.builder.SetInsertPoint(body);
}
} // namespace

Comprehension begin_comprehension(ExpressionLowering &state) {
    auto *accumulator = root_slot(state);
    store(state, llvm::ConstantInt::get(state.word, abi::v1::empty_list), accumulator);
    auto *end = block(state, "comprehension.end");
    return {.bindings = state.bindings, .accumulator = accumulator, .next = end, .end = end};
}

void lower_generators(ExpressionLowering &state, Comprehension &comprehension,
                      const ast::ComprehensionQualifier &qualifier) {
    const auto parts = semantic::zipped(qualifier);
    locate_source(state.builder, *state.module.syntax,
                  std::visit([](const auto &value) -> const ast::NodeSource & { return value.source; }, qualifier));
    std::vector<Cursor> cursors;
    for (const auto &part : parts) {
        cursors.push_back({root_slot(state)});
        store(state, value_of(state, *semantic::generator_input(part)), cursors.back().slot);
    }
    auto *head = block(state, "generator.next");
    auto *done = block(state, "generator.done");
    state.builder.CreateBr(head);
    state.builder.SetInsertPoint(head);
    next_elements(state, cursors, done);
    auto *elements = state.builder.GetInsertBlock();
    state.builder.SetInsertPoint(done);
    exhausted(state, cursors, comprehension.next);
    state.builder.SetInsertPoint(elements);
    bind_elements(state, parts, cursors, head);
    comprehension.next = head;
}

void lower_filter(ExpressionLowering &state, const Comprehension &comprehension, const ast::ExprId &filter) {
    const auto &expression = state.module.syntax->expression(filter);
    auto *pass = block(state, "filter.pass");
    if (state.function.guard_filters.contains(&expression)) {
        const ast::GuardSyntax guard{{{{filter}, expression.source}}, expression.source};
        lower_guard(state, guard, {.success = pass, .rejection = comprehension.next});
    } else {
        locate_source(state.builder, *state.module.syntax, expression.source);
        auto *value = state.values.at(&expression);
        auto *other = block(state, "filter.other");
        state.builder.CreateCondBr(lower_exact(state, value, lower_atom(state, ast::Atom{U"true"})), pass, other);
        state.builder.SetInsertPoint(other);
        auto *bad = block(state, "filter.bad");
        state.builder.CreateCondBr(lower_exact(state, value, lower_atom(state, ast::Atom{U"false"})),
                                   comprehension.next, bad);
        state.builder.SetInsertPoint(bad);
        raise_reason(state, abi::v1::ErrorReason::bad_filter, value);
    }
    state.builder.SetInsertPoint(pass);
}

void lower_templates(ExpressionLowering &state, const Comprehension &comprehension,
                     const std::span<llvm::Value *const> values) {
    std::vector<llvm::Value *> cells(values.rbegin(), values.rend());
    cells.push_back(load(state, comprehension.accumulator, "comprehension.accumulator"));
    store(state, lower_list(state, cells), comprehension.accumulator);
    state.builder.CreateBr(comprehension.next);
}

llvm::Value *finish_comprehension(ExpressionLowering &state, Comprehension &comprehension) {
    state.builder.SetInsertPoint(comprehension.end);
    state.bindings = std::move(comprehension.bindings);
    return lower_reverse(state, load(state, comprehension.accumulator, "comprehension.accumulator"));
}
} // namespace erlang_aot::codegen
