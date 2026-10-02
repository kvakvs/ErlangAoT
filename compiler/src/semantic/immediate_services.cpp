#include "services.hpp"
#include <array>

namespace erlang_aot::semantic {
using Op = abi::v1::ImmediateOperation;

std::optional<Op> immediate_operator(ast::BinaryOperator operation) {
    static const std::map<ast::BinaryOperator, Op> operators{
        {ast::BinaryOperator::exact_equal, Op::exact_equal},
        {ast::BinaryOperator::exact_not_equal, Op::exact_not_equal},
        {ast::BinaryOperator::equal, Op::equal},
        {ast::BinaryOperator::not_equal, Op::not_equal},
        {ast::BinaryOperator::less, Op::less},
        {ast::BinaryOperator::less_equal, Op::less_equal},
        {ast::BinaryOperator::greater, Op::greater},
        {ast::BinaryOperator::greater_equal, Op::greater_equal}};
    const auto found = operators.find(operation);
    return found == operators.end() ? std::nullopt : std::optional{found->second};
}

std::optional<Op> immediate_service(const FunctionKey &key) {
    static const std::map<FunctionKey, Op> signatures{{{U"is_atom", 1}, Op::is_atom},
                                                      {{U"is_integer", 1}, Op::is_integer},
                                                      {{U"is_number", 1}, Op::is_number},
                                                      {{U"is_boolean", 1}, Op::is_boolean},
                                                      {{U"is_tuple", 1}, Op::is_tuple},
                                                      {{U"is_list", 1}, Op::is_list},
                                                      {{U"is_binary", 1}, Op::is_binary},
                                                      {{U"is_bitstring", 1}, Op::is_bitstring},
                                                      {{U"is_float", 1}, Op::is_float},
                                                      {{U"is_map", 1}, Op::is_map},
                                                      {{U"is_pid", 1}, Op::is_pid},
                                                      {{U"is_port", 1}, Op::is_port},
                                                      {{U"is_reference", 1}, Op::is_reference},
                                                      {{U"is_function", 1}, Op::is_function},
                                                      {{U"is_function", 2}, Op::is_function_arity},
                                                      {{U"tuple_size", 1}, Op::tuple_size},
                                                      {{U"length", 1}, Op::length},
                                                      {{U"size", 1}, Op::size},
                                                      {{U"element", 2}, Op::element},
                                                      {{U"hd", 1}, Op::hd},
                                                      {{U"tl", 1}, Op::tl},
                                                      {{U"min", 2}, Op::minimum},
                                                      {{U"max", 2}, Op::maximum},
                                                      {{U"=:=", 2}, Op::exact_equal},
                                                      {{U"=/=", 2}, Op::exact_not_equal},
                                                      {{U"==", 2}, Op::equal},
                                                      {{U"/=", 2}, Op::not_equal},
                                                      {{U"<", 2}, Op::less},
                                                      {{U"=<", 2}, Op::less_equal},
                                                      {{U">", 2}, Op::greater},
                                                      {{U">=", 2}, Op::greater_equal}};
    const auto found = signatures.find(key);
    return found == signatures.end() ? std::nullopt : std::optional{found->second};
}
} // namespace erlang_aot::semantic
