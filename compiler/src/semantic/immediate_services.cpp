#include "services.hpp"
#include <array>

namespace erlang_aot::semantic {
using Op = abi::v1::ImmediateOperation;

std::optional<Op> immediate_operator(ast::BinaryOperator operation) {
    static const std::map<ast::BinaryOperator, Op> operators{
        {ast::BinaryOperator::add, Op::add},
        {ast::BinaryOperator::subtract, Op::subtract},
        {ast::BinaryOperator::multiply, Op::multiply},
        {ast::BinaryOperator::divide, Op::divide},
        {ast::BinaryOperator::integer_divide, Op::integer_divide},
        {ast::BinaryOperator::remainder, Op::remainder},
        {ast::BinaryOperator::bit_and, Op::bit_and},
        {ast::BinaryOperator::bit_or, Op::bit_or},
        {ast::BinaryOperator::bit_xor, Op::bit_xor},
        {ast::BinaryOperator::shift_left, Op::shift_left},
        {ast::BinaryOperator::shift_right, Op::shift_right},
        {ast::BinaryOperator::exact_equal, Op::exact_equal},
        {ast::BinaryOperator::exact_not_equal, Op::exact_not_equal},
        {ast::BinaryOperator::equal, Op::equal},
        {ast::BinaryOperator::not_equal, Op::not_equal},
        {ast::BinaryOperator::less, Op::less},
        {ast::BinaryOperator::less_equal, Op::less_equal},
        {ast::BinaryOperator::greater, Op::greater},
        {ast::BinaryOperator::greater_equal, Op::greater_equal},
        {ast::BinaryOperator::logical_and, Op::logical_and},
        {ast::BinaryOperator::logical_or, Op::logical_or},
        {ast::BinaryOperator::logical_xor, Op::logical_xor}};
    const auto found = operators.find(operation);
    return found == operators.end() ? std::nullopt : std::optional{found->second};
}

std::optional<Op> immediate_service(const FunctionKey &key) {
    static const std::map<FunctionKey, Op> signatures{{{U"is_atom", 1}, Op::is_atom},
                                                      {{U"is_integer", 3}, Op::is_integer_range},
                                                      {{U"is_record", 2}, Op::is_record},
                                                      {{U"is_record", 3}, Op::is_record},
                                                      {{U"+", 2}, Op::add},
                                                      {{U"-", 2}, Op::subtract},
                                                      {{U"*", 2}, Op::multiply},
                                                      {{U"/", 2}, Op::divide},
                                                      {{U"float", 1}, Op::to_float},
                                                      {{U"round", 1}, Op::round},
                                                      {{U"trunc", 1}, Op::trunc},
                                                      {{U"floor", 1}, Op::floor},
                                                      {{U"ceil", 1}, Op::ceil},
                                                      {{U"map_size", 1}, Op::map_size},
                                                      {{U"bit_size", 1}, Op::bit_size},
                                                      {{U"byte_size", 1}, Op::byte_size},
                                                      {{U"binary_part", 2}, Op::binary_part},
                                                      {{U"binary_part", 3}, Op::binary_part},
                                                      {{U"map_get", 2}, Op::map_get},
                                                      {{U"is_map_key", 2}, Op::is_map_key},
                                                      {{U"div", 2}, Op::integer_divide},
                                                      {{U"rem", 2}, Op::remainder},
                                                      {{U"band", 2}, Op::bit_and},
                                                      {{U"bor", 2}, Op::bit_or},
                                                      {{U"bxor", 2}, Op::bit_xor},
                                                      {{U"bsl", 2}, Op::shift_left},
                                                      {{U"bsr", 2}, Op::shift_right},
                                                      {{U"+", 1}, Op::positive},
                                                      {{U"-", 1}, Op::negative},
                                                      {{U"bnot", 1}, Op::bit_not},
                                                      {{U"abs", 1}, Op::absolute},
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
                                                      {{U">=", 2}, Op::greater_equal},
                                                      {{U"not", 1}, Op::logical_not},
                                                      {{U"and", 2}, Op::logical_and},
                                                      {{U"or", 2}, Op::logical_or},
                                                      {{U"xor", 2}, Op::logical_xor}};
    const auto found = signatures.find(key);
    return found == signatures.end() ? std::nullopt : std::optional{found->second};
}

std::optional<Op> immediate_unary(ast::UnaryOperator operation) {
    static const std::map<ast::UnaryOperator, Op> operators{{ast::UnaryOperator::positive, Op::positive},
                                                            {ast::UnaryOperator::negative, Op::negative},
                                                            {ast::UnaryOperator::bit_not, Op::bit_not},
                                                            {ast::UnaryOperator::logical_not, Op::logical_not}};
    const auto found = operators.find(operation);
    return found == operators.end() ? std::nullopt : std::optional{found->second};
}
} // namespace erlang_aot::semantic
