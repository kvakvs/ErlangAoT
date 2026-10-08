#include "../process/identities.hpp"
#include "../runtime_state.hpp"
#include "signal_messages.hpp"
#include <algorithm>
#include <array>
#include <new>

// The executor's port table (docs/ports.md): opening, output, closing, the port protocol of messages, and the links,
// monitors and exit signals of ports. A port's signals to processes hold no heap term when their target runs on
// another worker, so they wait for its slice to end (post) instead of making a builtin run again.
namespace erlang_aot::runtime::detail {
namespace {
// {Port, Tag} built in the heap of `process`.
TermResult<Term> port_message(ProcessContext &process, Word port, std::string_view tag) {
    TermFactory factory(process);
    const auto from = Term::from_word(port, process);
    const auto name = factory.atom(tag);
    return from && name ? factory.tuple(std::array{*from, *name}) : (from ? name : from);
}
} // namespace

bool Executor::is_port_word(Word word) noexcept { return TermTag{word}.get_kind() == TermKind::local_port; }

Port *Executor::open(Word port) const noexcept {
    if (!is_port_word(port)) {
        return nullptr;
    }
    const auto found = ports_.find(port_number(port));
    return found == ports_.end() ? nullptr : found->second.get();
}

Word Executor::open_port(ProcessContext &owner, std::unique_ptr<PortDriver> driver, PortOptions options,
                         std::string spelling) {
    const std::scoped_lock lock(mutex_);
    const auto number = runtime_.identity_numbers.issue_port();
    if (!number) {
        throw std::bad_alloc();
    }
    auto port = std::make_unique<Port>();
    port->number = *number;
    port->connected = pid_of(owner);
    port->links.push_back(port->connected);
    port->spelling = std::move(spelling);
    port->options = std::move(options);
    port->driver = std::move(driver);
    const auto word = detail::port_word(*number);
    owner.signals().link(word);
    try {
        ports_.emplace(*number, std::move(port));
    } catch (...) {
        owner.signals().unlink(word);
        throw;
    }
    return word;
}

bool Executor::close_port(ProcessContext &caller, Word port) {
    const std::scoped_lock lock(mutex_);
    auto *open_port = open(port);
    if (!open_port) {
        return false;
    }
    const Running running(caller);
    close(*open_port, atom(caller, "normal"));
    drain();
    return true;
}

PortOutcome Executor::command_port(ProcessContext &caller, Word port, std::vector<std::byte> data) {
    const std::scoped_lock lock(mutex_);
    auto *open_port = open(port);
    if (!open_port || !open_port->options.output) {
        return PortOutcome::not_open;
    }
    const Running running(caller);
    const auto outcome = write(caller, *open_port, std::move(data));
    drain();
    return outcome;
}

PortOutcome Executor::write(ProcessContext &caller, Port &port, std::vector<std::byte> data) {
    const auto size = data.size();
    const auto output = framed(port.options, std::move(data));
    if (!output) {
        return PortOutcome::too_long;
    }
    if (const auto written = port.driver->write(*output); !written) {
        close(port, atom(caller, written.error().reason));
        return PortOutcome::done;
    }
    port.output += size;
    return PortOutcome::done;
}

bool Executor::connect_port(Word port, const Term &owner_pid) {
    const std::scoped_lock lock(mutex_);
    const auto pid = owner_pid.word();
    auto *open_port = open(port);
    auto *owner = process(pid);
    if (!open_port || !owner) {
        return false;
    }
    open_port->connected = pid;
    if (!std::ranges::contains(open_port->links, pid)) {
        open_port->links.push_back(pid);
    }
    owner->signals().link(port);
    return true;
}

void Executor::port_request(ProcessContext &sender, Word port, const PortRequest &request) {
    const std::scoped_lock lock(mutex_);
    auto *open_port = open(port);
    if (!open_port) {
        return;
    }
    const Running running(sender);
    if (!serve(sender, *open_port, request)) {
        badsig(sender, *open_port);
    }
    drain();
}

bool Executor::serve(ProcessContext &sender, Port &port, const PortRequest &request) {
    if (request.kind == PortRequest::Kind::malformed || request.from != port.connected) {
        return false;
    }
    if (request.kind == PortRequest::Kind::command) {
        return port.options.output && write(sender, port, request.data) == PortOutcome::done;
    }
    if (request.kind == PortRequest::Kind::connect && !process(request.owner)) {
        return false;
    }
    answer(sender, port, request);
    return true;
}

void Executor::answer(ProcessContext &sender, Port &port, const PortRequest &request) {
    // A close or connect answers the sender, which is the connected process, after acting.
    auto *connected = process(port.connected);
    const auto word = detail::port_word(port.number);
    const auto close_request = request.kind == PortRequest::Kind::close;
    if (close_request) {
        close(port, atom(sender, "normal"));
    } else {
        port.connected = request.owner;
    }
    if (connected) {
        post(*connected,
             PortEvent::message(close_request ? PortEvent::Kind::closed : PortEvent::Kind::connected, word));
    }
}

void Executor::badsig(ProcessContext &sender, Port &port) {
    if (auto *connected = process(port.connected)) {
        post(*connected, PortEvent::exit(detail::port_word(port.number), atom(sender, "badsig").word()));
    }
}

std::optional<PortInfo> Executor::port_info(Word port) const {
    const std::scoped_lock lock(mutex_);
    const auto *open_port = open(port);
    if (!open_port) {
        return std::nullopt;
    }
    PortInfo info{.name = open_port->spelling,
                  .links = open_port->links,
                  .id = open_port->number,
                  .connected = open_port->connected,
                  .input = open_port->input,
                  .output = open_port->output,
                  .os_pid = open_port->driver->os_pid(),
                  .monitored_by = {},
                  .registered = open_port->name};
    for (const auto &[reference, watcher] : open_port->watchers) {
        info.monitored_by.push_back(watcher.pid);
    }
    return info;
}

std::vector<Word> Executor::ports() const {
    const std::scoped_lock lock(mutex_);
    std::vector<Word> words;
    words.reserve(ports_.size());
    for (const auto &[number, port] : ports_) {
        words.push_back(detail::port_word(number));
    }
    return words;
}

bool Executor::controllable(Word port) const {
    const std::scoped_lock lock(mutex_);
    const auto *open_port = open(port);
    return open_port && open_port->driver->controllable();
}

void Executor::close(Port &port, const Term &reason) {
    // The port leaves the table first; its record and driver live until the signals went out.
    auto node = ports_.extract(port.number);
    const auto word = detail::port_word(port.number);
    if (port.name != 0) {
        names_.erase(port.name);
    }
    for (const auto pid : port.links) {
        if (auto *target = process(pid)) {
            target->signals().unlink(word);
            link_closed(*target, word, reason);
        }
    }
    for (const auto &[reference, watcher] : port.watchers) {
        if (auto *target = process(watcher.pid)) {
            target->signals().demonitor(reference);
            monitor_closed(*target, word, reference, watcher.name, reason);
        }
    }
}

void Executor::link_closed(ProcessContext &target, Word port, const Term &reason) {
    if (busy(&target)) {
        post(target, PortEvent::exit(port, reason.word()));
    } else {
        signal(target, port, reason, SignalKind::link);
    }
}

void Executor::monitor_closed(ProcessContext &target, Word port, const ReferenceIdentity &reference, Word name,
                              const Term &reason) {
    if (busy(&target)) {
        post(target, PortEvent::down(port, reason.word(), reference, name));
    } else {
        deliver(target, down_message(target, reference, {.pid = port, .name = name}, reason));
    }
}

void Executor::port_exit(Port &port, Word from, const Term &reason, SignalKind kind) {
    if (kind == SignalKind::link && is_atom(reason, "normal") && from != port.connected) {
        std::erase(port.links, from);
        return;
    }
    if (kind != SignalKind::link && is_atom(reason, "kill")) {
        auto killed = runtime_.atom_storage.intern("killed");
        if (!killed) {
            throw std::bad_alloc();
        }
        close(port, *killed);
        return;
    }
    close(port, reason);
}

ProcessContext *Executor::busy_peer(const Port &port) noexcept {
    for (const auto pid : port.links) {
        if (auto *peer = process(pid); peer && busy(peer)) {
            return peer;
        }
    }
    for (const auto &[reference, watcher] : port.watchers) {
        if (auto *peer = process(watcher.pid); peer && busy(peer)) {
            return peer;
        }
    }
    return nullptr;
}

void Executor::post(ProcessContext &target, PortEvent event) {
    if (busy(&target)) {
        schedule(target).events.push_back(event);
    } else {
        apply(target, event);
    }
}

void Executor::apply(ProcessContext &target, const PortEvent &event) {
    if (target.generated_calls().failure()) {
        // The target has already ended.
        return;
    }
    const auto reason = Term::from_word(event.reason, target);
    switch (event.kind) {
    case PortEvent::Kind::exit:
        signal(target, event.port, reason.value(), SignalKind::link);
        break;
    case PortEvent::Kind::down:
        deliver(target,
                down_message(target, *event.reference, {.pid = event.port, .name = event.name}, reason.value()));
        break;
    case PortEvent::Kind::closed:
        deliver(target, port_message(target, event.port, "closed"));
        break;
    case PortEvent::Kind::connected:
        deliver(target, port_message(target, event.port, "connected"));
        break;
    }
}

bool Executor::link_port(ProcessContext &process, Word port) {
    auto *open_port = open(port);
    if (!open_port) {
        if (!process.signals().trap_exit()) {
            return false;
        }
        const Running running(process);
        deliver(process, exit_message(process, port, atom(process, "noproc")));
        return true;
    }
    const auto self = pid_of(process);
    if (!std::ranges::contains(open_port->links, self)) {
        open_port->links.push_back(self);
    }
    process.signals().link(port);
    return true;
}

void Executor::unlink_port(ProcessContext &process, Word port) noexcept {
    process.signals().unlink(port);
    if (auto *open_port = open(port)) {
        std::erase(open_port->links, pid_of(process));
    }
}

Term Executor::monitor_port(ProcessContext &watcher, Word port, Word name, const Term &reference) {
    const auto identity = reference.reference_value().value();
    auto *open_port = open(port);
    if (!open_port) {
        const Running running(watcher);
        // A 'DOWN' of a port names the port, or {Name, Node}; one that names nothing is still a port monitor.
        const auto item = Signals::Monitor{.pid = port != 0 ? port : detail::port_word(0), .name = name};
        deliver(watcher, down_message(watcher, identity, item, atom(watcher, "noproc")));
        return reference;
    }
    watcher.signals().monitor(identity, port);
    open_port->watchers.emplace(identity, Signals::Monitor{.pid = pid_of(watcher), .name = name});
    return reference;
}

void Executor::exit_port(ProcessContext &sender, Word port, const Term &reason) {
    auto *open_port = open(port);
    if (!open_port) {
        return;
    }
    // A reason in the sender's heap can reach only processes that do not run elsewhere.
    if (!reason.is_atom()) {
        if (auto *peer = busy_peer(*open_port)) {
            block(sender, *peer);
        }
    }
    const Running running(sender);
    port_exit(*open_port, pid_of(sender), reason, SignalKind::exit);
    drain();
}

void Executor::notify_ports(ProcessContext &process, const Term &reason, const std::vector<Word> &links) {
    const auto from = pid_of(process);
    for (const auto word : links) {
        if (auto *port = open(word)) {
            std::erase(port->links, from);
            port_exit(*port, from, reason, SignalKind::link);
        }
    }
}
} // namespace erlang_aot::runtime::detail
