#include "../semantic/records.hpp"
#include "lowering_state.hpp"

namespace erlang_aot::codegen {
namespace {
// Body access owns its offending value before cleanup; the same mismatch silently rejects a guard.
llvm::BasicBlock *bad_record(ExpressionLowering &state, llvm::Value *payload) {
    if (state.rejection) {
        return state.rejection;
    }
    auto *saved = state.builder.GetInsertBlock();
    auto *failure = llvm::BasicBlock::Create(state.entry.getContext(), "record.badrecord", &state.entry);
    state.builder.SetInsertPoint(failure);
    raise_reason(state, abi::v1::ErrorReason::badrecord, payload);
    state.builder.SetInsertPoint(saved);
    return failure;
}

// Check exact tuple shape and tag; fields may be extracted once this returns.
void check_record(ExpressionLowering &state, const semantic::RecordLayout &layout, llvm::Value *value,
                  llvm::BasicBlock *mismatch) {
    (void)lower_inspection(state, abi::v1::ContainerInspection::tuple_shape, value, layout.fields.size() + 1, mismatch);
    auto *tag = lower_inspection(state, abi::v1::ContainerInspection::tuple_element, value, 0, mismatch);
    auto *test = lower_exact(state, tag, lower_atom(state, layout.name));
    auto *field = llvm::BasicBlock::Create(state.entry.getContext(), "record.field", &state.entry);
    state.builder.CreateCondBr(test, field, mismatch);
    state.builder.SetInsertPoint(field);
}

// Extract one declared zero-based field after the shape check.
llvm::Value *access(ExpressionLowering &state, const ast::RecordAccess &access) {
    const auto &layout = *semantic::record_layout(state.module, access.identity);
    auto *value = state.values.at(&state.module.syntax->expression(access.base));
    auto *mismatch = bad_record(state, value);
    check_record(state, layout, value, mismatch);
    return lower_inspection(state, abi::v1::ContainerInspection::tuple_element, value,
                            *semantic::record_field(layout, access.field) + 1, mismatch);
}

// Build a new tuple from the already evaluated update values and the record's other fields.
llvm::Value *update(ExpressionLowering &state, const ast::RecordExpression &record) {
    const auto &layout = *semantic::record_layout(state.module, record.identity);
    auto *value = state.values.at(&state.module.syntax->expression(*record.base));
    auto *mismatch = bad_record(state, value);
    check_record(state, layout, value, mismatch);
    std::vector<llvm::Value *> values(layout.fields.size() + 1);
    values[0] = lower_atom(state, layout.name);
    for (const auto &field : record.fields) {
        const auto position = *semantic::record_field(layout, std::get<ast::Atom>(field.name)) + 1;
        values[position] = state.values.at(&state.module.syntax->expression(field.value));
    }
    for (std::size_t i = 1; i < values.size(); ++i) {
        if (!values[i]) {
            values[i] = lower_inspection(state, abi::v1::ContainerInspection::tuple_element, value, i, mismatch);
        }
    }
    return lower_tuple(state, values);
}
} // namespace

llvm::Value *lower_record(ExpressionLowering &state, const ast::ExprId &id) {
    const auto &expression = state.module.syntax->expression(id);
    if (const auto *record = std::get_if<ast::RecordExpression>(&expression.value)) {
        if (record->base) {
            return update(state, *record);
        }
        const auto &layout = *semantic::record_layout(state.module, record->identity);
        auto values = state.record_values.at(&expression);
        values.insert(values.begin(), lower_atom(state, layout.name));
        return lower_tuple(state, values);
    }
    if (const auto *field = std::get_if<ast::RecordAccess>(&expression.value)) {
        return access(state, *field);
    }
    if (const auto *index = std::get_if<ast::RecordIndex>(&expression.value)) {
        const auto &layout = *semantic::record_layout(state.module, index->record, expression.source);
        return lower_integer(state, std::to_string(*semantic::record_field(layout, index->field) + 2));
    }
    return nullptr;
}
} // namespace erlang_aot::codegen
