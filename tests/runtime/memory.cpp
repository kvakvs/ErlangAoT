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
    const auto allocated = heap.allocate(4);
    require(allocated && allocated->size() == 4 * sizeof(Word), "boundary allocation failed");
    require(heap.allocate(1) == std::unexpected(HeapError::limit_exceeded), "exhausted budget ignored");
    require(heap.collect() == std::unexpected(HeapError::not_implemented), "collector fabricated statistics");
    require(heap.used_words() == 4 && heap.capacity_words() == 4, "rejection changed accounting");
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
    require(heap.allocate(max_bytes / sizeof(Word)) == std::unexpected(HeapError::out_of_memory),
            "impossible backing allocation did not fail safely");
    require(heap.capacity_words() == 0, "budget allocated backing storage");
    require(runtime.destroy_context(*context) == Status::ok, "budget context cleanup failed");
}

// Growth and aborted construction preserve earlier addresses and restore exact capacity/word accounting.
void check_reservations(Runtime &runtime) {
    auto *context = runtime.create_context({2 * sizeof(Word), 32 * sizeof(Word)}).value();
    auto &heap = context->heap();
    // Raw words must stay parseable: store a nonzero one-word filler header to detect damage.
    constexpr auto filler = static_cast<Word>(BoxedKind::filler) << 2;
    auto first = heap.allocate(1).value();
    *reinterpret_cast<Word *>(first.data()) = filler;
    {
        auto reservation = heap.reserve(8).value();
        require(reservation.bytes().size() == 8 * sizeof(Word), "wrong reservation size");
        require(heap.reserve(1) == std::unexpected(HeapError::unsafe_point), "overlapping reservation accepted");
        auto moved = std::move(reservation);
        require(reservation.bytes().empty(), "moved reservation kept access");
    }
    require(heap.used_words() == 1 && heap.capacity_words() == 2, "rollback kept backing or accounting");
    require(heap.reserve(1, 3) == std::unexpected(HeapError::invalid_size), "non-power alignment accepted");
    {
        auto aligned = heap.reserve(2, alignof(std::max_align_t)).value();
        require(reinterpret_cast<std::uintptr_t>(aligned.bytes().data()) % alignof(std::max_align_t) == 0,
                "reservation is misaligned");
        require(aligned.commit().has_value(), "aligned commit failed");
    }
    require(*reinterpret_cast<const Word *>(first.data()) == filler, "growth moved or damaged committed data");
    require(heap.verify().has_value(), "raw allocations left an unparseable heap");
    {
        auto committed = heap.reserve(1).value();
        require(committed.commit().has_value(), "commit failed");
        require(committed.bytes().empty() && committed.commit() == std::unexpected(HeapError::invalid_size),
                "committed reservation kept mutation access");
    }
    {
        auto expired = heap.reserve(1).value();
        require(runtime.destroy_context(context) == Status::ok, "context teardown failed");
        require(expired.bytes().empty() && expired.commit() == std::unexpected(HeapError::expired_context),
                "reservation used expired context");
    }
}

// Invalid host values cannot arise from valid Erlang or the linked consumer's successful copies.
void check_boundaries() {
    auto runtime = Runtime::start().value();
    check_options(*runtime);
    check_reservations(*runtime);
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
