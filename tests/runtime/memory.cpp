#include <array>
#include <erlang_aot/runtime/runtime.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

using namespace erlang_aot::runtime;
using erlang_aot::abi::v1::Status;

static_assert(!std::is_move_constructible_v<ProcessHeap>);
static_assert(!std::is_copy_constructible_v<ProcessHeap>);

namespace {
// Keep all checks active in Release and report the first violated ownership contract.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Validate word requests before any byte multiplication, without publishing storage or fake GC results.
void check_requests(ProcessHeap &heap) {
    require(heap.allocate(0) == std::unexpected(HeapError::invalid_size), "zero allocation accepted");
    constexpr auto max_words = std::numeric_limits<std::size_t>::max() / sizeof(Word);
    require(heap.allocate(max_words + 1) == std::unexpected(HeapError::invalid_size), "byte overflow accepted");
    require(heap.allocate(max_words) == std::unexpected(HeapError::limit_exceeded), "large request missed limit");
    require(heap.allocate(5) == std::unexpected(HeapError::limit_exceeded), "word budget ignored");
    require(heap.allocate(4) == std::unexpected(HeapError::not_implemented), "boundary allocation fabricated");
    require(heap.allocate(1) == std::unexpected(HeapError::not_implemented), "small allocation fabricated");
    require(heap.collect() == std::unexpected(HeapError::not_implemented), "collector fabricated statistics");
    require(heap.used_words() == 0 && heap.capacity_words() == 0, "rejection changed accounting");
}

// Check byte policy independently of requests, including target-word edges and a maximum valid budget.
void check_options(Runtime &runtime) {
    const std::array invalid{HeapOptions{0, sizeof(Word)}, HeapOptions{sizeof(Word), 0},
                             HeapOptions{2 * sizeof(Word), sizeof(Word)}, HeapOptions{1, sizeof(Word)},
                             HeapOptions{sizeof(Word), sizeof(Word) + 1}};
    for (const auto options : invalid) {
        require(runtime.create_context(options) == std::unexpected(Status::invalid_argument), "invalid byte policy");
    }
    constexpr auto max_bytes = std::numeric_limits<std::size_t>::max() / sizeof(Word) * sizeof(Word);
    auto context = runtime.create_context({sizeof(Word), max_bytes});
    require(context.has_value(), "lazy maximum budget failed");
    auto &heap = (*context)->heap();
    require(heap.allocate(max_bytes / sizeof(Word)) == std::unexpected(HeapError::not_implemented),
            "maximum byte budget overflowed");
    require(heap.capacity_words() == 0, "budget allocated backing storage");
    require(runtime.destroy_context(*context) == Status::ok, "budget context cleanup failed");
}

// Invalid host values cannot arise from valid Erlang or the linked consumer's successful copies.
void check_boundaries() {
    auto runtime = Runtime::start().value();
    check_options(*runtime);
    auto *context = runtime->create_context({sizeof(Word), 4 * sizeof(Word)}).value();
    auto *other = runtime->create_context({sizeof(Word), sizeof(Word)}).value();
    check_requests(context->heap());
    require(other->heap().allocate(2) == std::unexpected(HeapError::limit_exceeded), "owners share budgets");
    require(other->heap().add(Term{}) == std::unexpected(TermError::invalid_encoding), "invalid slot added");
    require(Term{}.copy_to(context->heap()) == std::unexpected(TermError::invalid_encoding), "invalid slot copied");
}
} // namespace

// Exercise implemented boundaries without manufacturing heap terms or calling freed owner pointers.
int main() {
    try {
        check_boundaries();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
