#include "spawn.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cwctype>
#include <map>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// Spawned programs on Windows (docs/ports.md#drivers): CreateProcessW with pipes for stdin and stdout; the child
// inherits exactly its three standard handles.
namespace clause::runtime::detail {
namespace {
// A handle closed when it goes out of scope unless released.
class Handle final {
  public:
    Handle() noexcept = default;

    explicit Handle(HANDLE handle) noexcept : handle_(handle) {}

    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;

    Handle(Handle &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    Handle &operator=(Handle &&other) noexcept {
        std::swap(handle_, other.handle_);
        return *this;
    }

    ~Handle() {
        if (handle_ && handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
    }

    HANDLE get() const noexcept { return handle_; }

    // Give up ownership.
    HANDLE release() noexcept { return std::exchange(handle_, nullptr); }

  private:
    HANDLE handle_ = nullptr;
};

// UTF-16 text of UTF-8 `text`.
std::wstring wide(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const auto size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

// Quote one argument so the program's runtime (CommandLineToArgvW rules) reads it back unchanged.
std::wstring quoted(const std::wstring &argument) {
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return argument;
    }
    std::wstring result = L"\"";
    std::size_t backslashes = 0;
    for (const auto character : argument) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }
        // Backslashes before a quote are doubled, and the quote escaped.
        result.append(character == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        result.push_back(character);
        backslashes = 0;
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}

// The command line: {spawn, Command} as given, {spawn_executable, File} its argv[0] then its quoted arguments.
std::wstring command_line(const SpawnRequest &request, const PortOptions &options) {
    if (!request.executable) {
        return wide(request.command);
    }
    auto line = quoted(wide(options.arg0.value_or(request.command)));
    for (const auto &argument : options.args) {
        line += L" " + quoted(wide(argument));
    }
    return line;
}

// Order environment names as Windows does, without case.
struct NoCase {
    bool operator()(const std::wstring &left, const std::wstring &right) const {
        return std::lexicographical_compare(left.begin(), left.end(), right.begin(), right.end(),
                                            [](wchar_t a, wchar_t b) { return std::towupper(a) < std::towupper(b); });
    }
};

// The environment block with the {env, ...} changes applied; empty when there are none.
std::wstring environment(const PortOptions &options) {
    if (options.env.empty()) {
        return {};
    }
    std::map<std::wstring, std::wstring, NoCase> variables;
    auto *block = GetEnvironmentStringsW();
    for (const auto *entry = block; *entry != L'\0'; entry += std::wcslen(entry) + 1) {
        const std::wstring text(entry);
        // Names may start with '=' (per-drive directories), so the separator is searched from the second character.
        const auto separator = text.find(L'=', 1);
        variables[text.substr(0, separator)] = separator == std::wstring::npos ? L"" : text.substr(separator + 1);
    }
    FreeEnvironmentStringsW(block);
    for (const auto &[name, value] : options.env) {
        if (value) {
            variables[wide(name)] = wide(*value);
        } else {
            variables.erase(wide(name));
        }
    }
    std::wstring result;
    for (const auto &[name, value] : variables) {
        result.append(name).append(L"=").append(value).push_back(L'\0');
    }
    return result + L'\0';
}

// The POSIX reason of a failed CreateProcessW.
std::string reason(DWORD error) {
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_NAME:
    case ERROR_DIRECTORY:
        return "enoent";
    case ERROR_ACCESS_DENIED:
        return "eacces";
    case ERROR_BAD_EXE_FORMAT:
        return "enoexec";
    default:
        return "einval";
    }
}

// A pipe whose child end is an inheritable synchronous handle and whose parent end the I/O thread serves with
// overlapped I/O; it is a named pipe, as anonymous pipes cannot be overlapped.
struct Pipe {
    Handle child;
    Handle parent;
};

// The unique name of a new pipe of this program.
std::wstring pipe_name() {
    static std::atomic<std::uint64_t> serial{0};
    return L"\\\\.\\pipe\\clause-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++serial);
}

// A pipe for the child's stdin (`to_child`) or stdout.
std::optional<Pipe> make_pipe(bool to_child) {
    constexpr DWORD BUFFER_BYTES = 64 * 1024;
    const auto name = pipe_name();
    const DWORD direction = to_child ? PIPE_ACCESS_OUTBOUND : PIPE_ACCESS_INBOUND;
    Handle parent(CreateNamedPipeW(name.c_str(), direction | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1,
                                   BUFFER_BYTES, BUFFER_BYTES, 0, nullptr));
    if (parent.get() == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }
    SECURITY_ATTRIBUTES security{
        .nLength = sizeof(SECURITY_ATTRIBUTES), .lpSecurityDescriptor = nullptr, .bInheritHandle = TRUE};
    // The child may change its end's pipe state, so it gets the attributes right of the other direction.
    const DWORD access = to_child ? GENERIC_READ | FILE_WRITE_ATTRIBUTES : GENERIC_WRITE | FILE_READ_ATTRIBUTES;
    Handle child(CreateFileW(name.c_str(), access, 0, &security, OPEN_EXISTING, 0, nullptr));
    if (child.get() == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }
    return Pipe{std::move(child), std::move(parent)};
}

// An inheritable handle of the null device, for a direction the port does not use.
Handle null_device() {
    SECURITY_ATTRIBUTES security{
        .nLength = sizeof(SECURITY_ATTRIBUTES), .lpSecurityDescriptor = nullptr, .bInheritHandle = TRUE};
    return Handle(CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                              OPEN_EXISTING, 0, nullptr));
}

// An inheritable duplicate of this program's stderr, or the null device when it has none.
Handle error_output() {
    HANDLE duplicate = nullptr;
    const auto own = GetStdHandle(STD_ERROR_HANDLE);
    if (own == nullptr || own == INVALID_HANDLE_VALUE ||
        !DuplicateHandle(GetCurrentProcess(), own, GetCurrentProcess(), &duplicate, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
        return null_device();
    }
    return Handle(duplicate);
}

// The child's three standard handles and the parent's ends of the pipes.
struct Standard {
    std::optional<Pipe> input;
    std::optional<Pipe> output;
    Handle null_input;
    Handle null_output;
    Handle error;

    HANDLE child_input() const noexcept { return input ? input->child.get() : null_input.get(); }

    HANDLE child_output() const noexcept { return output ? output->child.get() : null_output.get(); }
};

// Make the pipes the port's directions need and the null device or stderr for the others.
std::optional<Standard> standard_handles(const PortOptions &options) {
    Standard handles;
    handles.input = options.output ? make_pipe(true) : std::nullopt;
    handles.output = options.input ? make_pipe(false) : std::nullopt;
    if ((options.output && !handles.input) || (options.input && !handles.output)) {
        return std::nullopt;
    }
    handles.null_input = options.output ? Handle() : null_device();
    handles.null_output = options.input ? Handle() : null_device();
    handles.error = options.stderr_to_stdout ? Handle() : error_output();
    return handles;
}

// A process attribute list naming the only handles a child inherits; deleted with it.
class InheritedHandles final {
  public:
    explicit InheritedHandles(std::vector<HANDLE> handles) : handles_(std::move(handles)) {
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        storage_.resize(size);
        InitializeProcThreadAttributeList(list(), 1, 0, &size);
        UpdateProcThreadAttribute(list(), 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, static_cast<void *>(handles_.data()),
                                  handles_.size() * sizeof(HANDLE), nullptr, nullptr);
    }

    InheritedHandles(const InheritedHandles &) = delete;
    InheritedHandles &operator=(const InheritedHandles &) = delete;
    InheritedHandles(InheritedHandles &&) = delete;
    InheritedHandles &operator=(InheritedHandles &&) = delete;

    ~InheritedHandles() { DeleteProcThreadAttributeList(list()); }

    LPPROC_THREAD_ATTRIBUTE_LIST list() noexcept {
        return reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data());
    }

  private:
    // The handles the list points at, and the list's own storage.
    std::vector<HANDLE> handles_;
    std::vector<std::byte> storage_;
};

// The handle the child's stderr gets.
HANDLE child_error(const PortOptions &options, const Standard &handles) noexcept {
    return options.stderr_to_stdout ? handles.child_output() : handles.error.get();
}

// The handles a child inherits: its stdin, stdout and, when it is another one, stderr.
std::vector<HANDLE> inherited(const PortOptions &options, const Standard &handles) {
    std::vector<HANDLE> result{handles.child_input(), handles.child_output()};
    if (!options.stderr_to_stdout) {
        result.push_back(handles.error.get());
    }
    return result;
}

// Create the process with exactly `handles` inheritable through a handle list.
std::expected<PROCESS_INFORMATION, DriverError> create(const SpawnRequest &request, const PortOptions &options,
                                                       const Standard &handles) {
    const auto error = child_error(options, handles);
    InheritedHandles list(inherited(options, handles));
    auto *attributes = list.list();
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = handles.child_input();
    startup.StartupInfo.hStdOutput = handles.child_output();
    startup.StartupInfo.hStdError = error;
    startup.lpAttributeList = attributes;
    auto line = command_line(request, options);
    auto block = environment(options);
    const auto application = request.executable ? wide(request.command) : std::wstring();
    const auto directory = options.cd ? wide(*options.cd) : std::wstring();
    PROCESS_INFORMATION process{};
    const auto created = CreateProcessW(request.executable ? application.c_str() : nullptr, line.data(), nullptr,
                                        nullptr, TRUE, EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
                                        block.empty() ? nullptr : block.data(),
                                        options.cd ? directory.c_str() : nullptr, &startup.StartupInfo, &process);
    const auto failure = GetLastError();
    if (!created) {
        return std::unexpected(DriverError{reason(failure)});
    }
    CloseHandle(process.hThread);
    return process;
}

// The parent end of a pipe, now owned by the caller, or -1 without one.
NativeHandle parent_end(std::optional<Pipe> &pipe) noexcept {
    return pipe ? std::bit_cast<NativeHandle>(pipe->parent.release()) : -1;
}
} // namespace

std::expected<Spawned, DriverError> spawn_program(const SpawnRequest &request, const PortOptions &options) {
    auto handles = standard_handles(options);
    if (!handles) {
        return std::unexpected(DriverError{"emfile"});
    }
    const auto process = create(request, options, *handles);
    if (!process) {
        return std::unexpected(process.error());
    }
    return Spawned{.input = parent_end(handles->output),
                   .output = parent_end(handles->input),
                   .child = std::bit_cast<std::intptr_t>(process->hProcess),
                   .os_pid = process->dwProcessId};
}
} // namespace clause::runtime::detail
