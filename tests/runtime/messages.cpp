#include "scheduler/executor.hpp"
#include <array>
#include <clause/runtime/runtime.hpp>
#include <iostream>
#include <stdexcept>
#include <vector>

// Messages (docs/processes.md#messages) between contexts of one runtime: each sender's messages arrive in order,
// self-sends and sends to ended processes behave as in OTP, messages are roots of the receiver until received, the
// receive position survives arrivals, and a copy the receiver's heap refuses delivers nothing.
namespace {
using namespace clause::runtime;
using Status = clause::abi::v1::Status;

// Keep every check active in optimized builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// The pid word of a context.
Word pid(ProcessContext &context) { return TermFactory(context).pid(context.identity()).value().word(); }

// Send the tuple {Tag, [1..Count]} from `sender` to `receiver`.
TermResult<void> send(ProcessContext &sender, ProcessContext &receiver, std::int64_t tag, std::size_t count = 3) {
    TermFactory factory(sender);
    std::vector<Term> items;
    for (std::size_t i = 1; i <= count; ++i) {
        items.push_back(factory.integer(static_cast<std::int64_t>(i)).value());
    }
    const auto message = factory.tuple(std::array{factory.integer(tag).value(), factory.list(items).value()}).value();
    return detail::Executor::send(sender, pid(receiver), message);
}

// The tag of a {Tag, List} message word in `context`.
std::int64_t tag(ProcessContext &context, Word message) {
    return Term::from_word(message, context).value().tuple_element(0).value().integer_value().value();
}

// Messages of one sender arrive in order, mixed with self-sends; they survive a collection of the receiver.
void ordering(Runtime &runtime) {
    auto &first = *runtime.create_context().value();
    auto &second = *runtime.create_context().value();
    for (std::int64_t i = 1; i <= 3; ++i) {
        require(send(first, second, i).has_value() && send(second, second, 10 + i).has_value(), "send failed");
    }
    require(second.mailbox().size() == 6, "messages lost");
    require(second.heap().collect().has_value(), "collection with messages failed");
    std::vector<std::int64_t> tags;
    while (const auto message = second.mailbox().peek()) {
        tags.push_back(tag(second, *message));
        second.mailbox().take();
    }
    require(tags == std::vector<std::int64_t>{1, 11, 2, 12, 3, 13}, "messages out of order");
    require(second.heap().verify().has_value(), "heap damaged");
    require(runtime.destroy_context(&first) == Status::ok && runtime.destroy_context(&second) == Status::ok,
            "teardown failed");
}

// A receive skips messages it leaves; messages arriving after it examined all are examined next; take and restart
// start the next receive at the oldest message.
void positions(Runtime &runtime) {
    auto &sender = *runtime.create_context().value();
    auto &receiver = *runtime.create_context().value();
    auto &box = receiver.mailbox();
    require(!box.peek() && !box.unexamined(), "empty mailbox has a message");
    require(send(sender, receiver, 1).has_value() && send(sender, receiver, 2).has_value(), "send failed");
    require(box.unexamined() && tag(receiver, *box.peek()) == 1, "first message not examined first");
    box.skip();
    require(tag(receiver, *box.peek()) == 2, "skip did not advance");
    box.skip();
    require(!box.peek() && !box.unexamined(), "scan passed the end");
    require(send(sender, receiver, 3).has_value() && box.unexamined(), "arrival not seen");
    require(tag(receiver, *box.peek()) == 3, "arrival after the end not examined next");
    box.take();
    require(tag(receiver, *box.peek()) == 1 && box.size() == 2, "take did not restart at the oldest message");
    box.skip();
    box.restart();
    require(tag(receiver, *box.peek()) == 1, "restart did not return to the oldest message");
    require(runtime.destroy_context(&sender) == Status::ok && runtime.destroy_context(&receiver) == Status::ok,
            "teardown failed");
}

// A send to an ended process succeeds and delivers nothing; a receiver whose heap cap refuses the copy gets nothing.
void refused(Runtime &runtime) {
    auto &sender = *runtime.create_context().value();
    auto &ended = *runtime.create_context().value();
    const auto gone = pid(ended);
    require(runtime.destroy_context(&ended) == Status::ok, "teardown failed");
    const auto message = TermFactory(sender).atom("late").value();
    require(detail::Executor::send(sender, gone, message).has_value(), "send to an ended process failed");
    auto &small = *runtime.create_context({32, 4096 * sizeof(Word)}).value();
    const auto copied = send(sender, small, 1, 10'000);
    require(!copied && copied.error() == TermError::resource_limit && small.mailbox().size() == 0,
            "a refused copy was delivered");
    require(send(sender, small, 2).has_value() && small.mailbox().size() == 1, "small message refused");
    require(runtime.destroy_context(&sender) == Status::ok && runtime.destroy_context(&small) == Status::ok,
            "teardown failed");
    require(runtime.memory_bytes() == 0, "messages kept memory after teardown");
}
} // namespace

int main() {
    try {
        auto runtime = Runtime::start().value();
        ordering(*runtime);
        positions(*runtime);
        refused(*runtime);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
