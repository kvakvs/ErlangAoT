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

// Immediate copies retain no source/destination storage, even across independent runtime owners.
void check_copies(ProcessHeap &source, ProcessHeap &destination) {
    const std::array words{*encode_integer(-42), Word{0x2b}, Word{0x3b}};
    for (const Word word : words) {
        const auto value = Term::from_word(word);
        require(value.has_value(), "immediate fixture failed");
        const auto copied = value->copy_to(destination);
        const auto added = destination.add(*value);
        const auto same_heap = value->copy_to(source);
        require(copied && added && same_heap, "immediate copying failed");
        require(copied->word() == word && added->word() == word && same_heap->word() == word, "copy changed bits");
    }
    require(destination.add(Term{}) == std::unexpected(TermError::invalid_encoding), "invalid slot was added");
    require(Term{}.copy_to(source) == std::unexpected(TermError::invalid_encoding), "invalid slot was copied");
    require(source.capacity_words() == 0 && destination.used_words() == 0, "immediate copy allocated heap storage");
}

// Destroy one owner while retaining another and prove tokens and immediate values survive appropriately.
void check_owners() {
    auto runtime = Runtime::start();
    auto foreign = Runtime::start();
    require(runtime && foreign, "runtime setup failed");
    check_options(**runtime);
    auto first = (*runtime)->create_context({sizeof(Word), 4 * sizeof(Word)});
    auto second = (*runtime)->create_context({sizeof(Word), sizeof(Word)});
    auto other = (*foreign)->create_context();
    require(first && second && other, "context setup failed");
    auto first_lifetime = (*first)->lifetime().lock();
    auto second_lifetime = (*second)->lifetime().lock();
    require(&(*first)->heap() != &(*second)->heap(), "process heaps alias");
    require(&(*first)->mailbox() != &(*second)->mailbox(), "process mailboxes alias");
    check_requests((*first)->heap());
    require((*second)->heap().allocate(2) == std::unexpected(HeapError::limit_exceeded), "owners share budgets");
    check_copies((*first)->heap(), (*second)->heap());
    check_copies((*first)->heap(), (*other)->heap());
    const auto copied = Term::from_word(*encode_integer(42))->copy_to((*second)->heap());
    require(copied.has_value(), "retained copy failed");
    require((*runtime)->destroy_context(*first) == Status::ok, "source destruction failed");
    require(!first_lifetime->alive() && second_lifetime->alive(), "owner invalidation was not isolated");
    require(copied->integer_value() == 42, "source exit damaged immediate");
    runtime->reset();
    foreign->reset();
    require(!second_lifetime->alive(), "RAII teardown left live token");
    require(copied->integer_value() == 42, "destination exit damaged owner-independent immediate");
}
} // namespace

// Exercise implemented boundaries without manufacturing heap terms or calling freed owner pointers.
int main() {
    try {
        check_owners();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
