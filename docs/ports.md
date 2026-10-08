# Ports

Decision of plan 11 step 57A (2026-10-08). It replaces the step-53 decision
(no ports, [processes](processes.md#ports)): programs get ports as OTP
defines them, and external I/O goes through them. Steps 57B–57F implement
it; each section names its step. This contract settles the representation,
the driver model and the I/O thread before any source can open a port.

Implemented: identities, the port table, the port builtins and messages,
links, monitors, names and exit signals of ports, and output-only `fd` ports
(step 57B, `runtime/src/scheduler/ports.cpp`, `runtime/src/builtins/ports.cpp`,
`runtime/src/ports/`; OTP golden `executables_port_identities`).

## Identity

- A port is an immediate word: tag `0x7` (`TermKind2::port` under the
  `see_termkind2` primary tag, beside pids' `0x3`), payload a port number
  from one process-wide sequence that is never reused, like pid numbers
  ([pids](terms.md#pids-and-references)). A runtime admits only the numbers
  it issued; forged and foreign port words are rejected.
- Printing: `#Port<0.N>`; `port_to_list/1` gives that text and
  `list_to_port/1` parses it (badarg for anything else).
- Order: numbers < atoms < references < funs < ports < pids < tuples, as in
  OTP; ports compare by number.
- A closed port's identity stays valid: `is_port/1` stays true,
  `port_info/1,2` answer `undefined`, sends to it are dropped.
- Ports are immediates, so copying, collection and messages need nothing new.

## Port table and ownership

- Each runtime keeps a port table (number → port) in its executor, guarded
  by the executor mutex like the processes' links and names
  ([workers](processes.md#workers)).
- A port records its driver, its connected process (the opener, changed by
  `port_connect/2` or `{Pid, {connect, New}}`), its links, the monitors held
  on it, an optional registered name, its options and its input/output byte
  counters.
- `open_port/2` links the new port to its opener. When the connected process
  ends, its port closes; a port that closes sends exit signals to its links
  and `'DOWN'` messages to its monitors with its reason (`normal` for a close
  that is not an error, a POSIX atom such as `epipe` otherwise).
- An exit signal reaching a port closes it with its reason (`kill` from
  `exit/2` as `killed`), except `normal` through a link from a process other
  than the connected one, which only removes the link. `exit(Port, normal)`
  closes the port. A connected process that unlinked leaves its port open
  when it ends.
- A request message from a process other than the connected one, or a
  malformed message, sends the connected process an exit signal `badsig`
  from the port; the port stays open.
- A port's exit signals and messages to a process running on another worker
  wait for that process's time slice to end (they hold only atoms, pids and
  ports); an `exit(Port, Reason)` with a reason term in the sender's heap
  makes the builtin run again until no linked or monitoring process runs
  elsewhere, as for process signals ([workers](processes.md#workers)).
- Ports take registered names (`register/2`) and `monitor(port, Port)`;
  `link/1` and `unlink/1` accept them.

## Builtins and port messages

| Builtin or message | Step |
| --- | --- |
| `is_port/1` true for ports, `port_to_list/1`, `list_to_port/1`, `ports/0` | 57B |
| `port_info/1,2` (`name`, `links`, `id`, `connected`, `input`, `output`, `os_pid`, `monitors`, `monitored_by`, `registered_name`) | 57B |
| `port_close/1`, `port_connect/2`, `port_command/2,3` | 57B |
| `Port ! {Pid, {command, Data}}`, `{Pid, close}`, `{Pid, {connect, New}}` | 57B |
| `link/1`, `unlink/1`, `monitor(port, P)`, `exit/2` on ports; `register/2` of a port | 57B |
| `port_control/3`, `port_call/3` | 57B (badarg for drivers without control); used by the 57E/57F drivers |
| `open_port({fd, In, Out}, Opts)` | 57B output; input from 57C |
| `open_port({spawn, Command} \| {spawn_executable, File}, Opts)` | 57D |
| Internal drivers of the project library (`file`, sockets) | 57E, 57F |

The builtins replace the step-53 `[ports] notimpl` diagnostics; feature
`ports` becomes implemented in 57B. Errors follow OTP: `badarg` for a closed
or invalid port and bad arguments, the POSIX reason (`enoent`, `eacces`) as
an `error` for an open that fails.

Messages from a port go to its connected process:
`{Port, {data, Data}}`, `{Port, eof}` (option `eof`),
`{Port, {exit_status, Status}}` (option `exit_status`), `{Port, closed}`
(after `{Pid, close}`), `{Port, connected}` (to the old owner after a
connect).

## Data modes and options

| Option | Effect |
| --- | --- |
| `stream` (default), `{packet, N}` (N = 1, 2, 4) | Bytes as they arrive, or messages framed by an N-byte big-endian length that output also gets |
| `{line, L}` | `{eol, Line}` per line, `{noeol, Part}` for parts longer than `L` or an unterminated end |
| `binary` | Data as binaries instead of byte lists |
| `eof` | `{Port, eof}` at end of input; the port stays open until closed |
| `exit_status` | `{Port, {exit_status, S}}` when the program exits (spawn ports) |
| `use_stdio` (default), `nouse_stdio`, `stderr_to_stdout`, `in`, `out`, `hide` | As in OTP (`hide` has no effect) |
| `{args, List}`, `{arg0, A}`, `{env, Env}`, `{cd, Dir}` | Spawn ports (57D) |

Without `eof`, end of input closes the port with reason `normal`, after the
`exit_status` message when one was asked for. Unknown options are `badarg`.

## Drivers

A driver is a C++ object behind one port (`runtime/src/ports/`): it opens the
resource, accepts output (`port_command`), answers `port_control/3` when it
supports control, reports input and errors as events and closes. Drivers:

- `fd`: existing descriptors, output written synchronously (57B), input
  through the I/O thread (57C).
- `spawn`: a child program with its stdin and stdout as pipes (57D).
- `file`: an open file of the project library's `file` module; its operations
  are synchronous `port_control/3` calls that may block the worker running
  the caller for the duration of the disk I/O, as OTP's dirty I/O schedulers
  do (57E).
- `tcp`, `udp`: sockets of the project library's `gen_tcp`, `gen_udp` and
  `inet`; connects, accepts and receives are asynchronous (57F).

The internal drivers of `file` and the sockets are opened with
`{spawn_driver, Name}` under ErlangAoT names, and their `port_control/3`
operations are an ErlangAoT protocol: programs use the library modules, not
OTP's `prim_inet`/`efile` protocols.

## I/O thread

Each runtime that opens a port starts one I/O thread; it waits for every
port's input, output completion and child exit at once:

- Windows: an I/O completion port with overlapped I/O (named pipes for
  child programs, sockets); console standard input, which cannot be
  overlapped, gets a reader thread that posts its reads to the completion
  port.
- Linux and macOS: `poll()` over nonblocking descriptors plus a wakeup pipe.
  `epoll`/`kqueue` are a later optimization behind the same interface.

An event becomes a message to the connected process under the executor
mutex: at once when that process does not run on a worker (waking it like
any message), else when its time slice ends, so a running process's heap is
never touched by another thread. Output is queued on the port and written by
the I/O thread; `port_command` returns at once and never suspends the caller
(OTP may suspend a caller on a busy port; ErlangAoT queues without a cap).
The prototype `tests/prototypes/poller/` (`run.py --wsl`) shows the wakeup
on Windows (completion port) and WSL Linux (`poll()`): an idle scheduler
thread wakes 9–91 µs after input, and shutdown stops the I/O thread without
input.

At program end every port is closed (child programs see end of input; they
are not killed, as in OTP) and the I/O thread is stopped and joined.

## Standard I/O and files (57E)

- `io:format`, `io:put_chars` and `erlang:display` keep writing standard
  output directly ([io](io.md)); their output is unchanged. Standard input is
  read through an `{fd, 0, 1}` port owned by a library server process:
  `io:get_line/1,2` and `io:get_chars/2,3` ask it and wait for the answer.
- `file` is a project-library module: `open/2` returns an I/O server pid
  that owns a `file` port; `read/2`, `write/2`, `read_line/1`,
  `position/2`, `close/1` talk to it. `read_file/1`, `write_file/2`,
  `delete/1`, `rename/2`, `list_dir/1` use a port directly. Results and
  errors (`{error, enoent}`, ...) follow OTP for this subset.

## Sockets (57F)

A socket is a port, as with OTP's `inet_drv` backend: `is_port(Socket)` is
true and active-mode messages are `{tcp, Socket, Data}`,
`{tcp_closed, Socket}`, `{tcp_error, Socket, Reason}`,
`{udp, Socket, Address, Port, Data}`. The subset: `gen_tcp:connect/3,4`,
`listen/2`, `accept/1,2`, `send/2`, `recv/2,3`, `close/1`,
`controlling_process/2`, `shutdown/2`; `gen_udp:open/1,2`, `send/4`,
`recv/2,3`, `close/1`; `inet:setopts/2`, `inet:port/1`, `inet:peername/1`,
`inet:sockname/1`; modes `{active, true | false | once}`, `binary`/`list`,
`{packet, 0 | 1 | 2 | 4}`, `{reuseaddr, Bool}`; IPv4 and IPv6 addresses
as tuples and `localhost`/`loopback`.

## Not provided

Linked-in drivers and NIFs, `erlang:open_port({spawn_driver, Name})` for OTP
driver names, distribution ports, `port_call/3` on the provided drivers
other than the library's own protocol, `busy` port suspension, and the
`overlapped_io`, `parallelism` and `busy_limits_*` options.
