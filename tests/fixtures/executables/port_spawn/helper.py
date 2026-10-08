"""Helper program of the port_spawn case, run as `python helper.py MODE ARGS...` by spawned ports."""
import os
import sys

out = sys.stdout.buffer
err = sys.stderr.buffer
inp = sys.stdin.buffer


def echo():
    """Write back every input line as it arrives."""
    for line in inp:
        out.write(line)
        out.flush()


def lines():
    """Write two lines and an unterminated part, then exit."""
    out.write(b'first\nsecond line\nunterminated')


def packet():
    """Answer each 2-byte-length packet with the same packet in upper case."""
    while True:
        head = inp.read(2)
        if len(head) < 2:
            return
        body = inp.read(int.from_bytes(head, 'big')).upper()
        out.write(len(body).to_bytes(2, 'big') + body)
        out.flush()


def streams():
    """Write one line to standard error, then one to standard output."""
    err.write(b'to stderr\n')
    err.flush()
    out.write(b'to stdout\n')


def info():
    """Write the arguments and two environment variables."""
    out.write(('|'.join(sys.argv[2:]) + '\n').encode())
    out.write(f"{os.environ.get('ERLANG_AOT_PROBE')} {os.environ.get('ERLANG_AOT_UNSET')}\n".encode())


def count():
    """Read the number of bytes the argument names, then write how many arrived."""
    expected = int(sys.argv[2])
    total = 0
    while total < expected:
        chunk = inp.read(min(65536, expected - total))
        if not chunk:
            break
        total += len(chunk)
    out.write(f'{total}\n'.encode())


MODES = {'echo': echo, 'lines': lines, 'packet': packet, 'streams': streams, 'info': info, 'count': count}

if __name__ == '__main__':
    if sys.argv[1] == 'exit':
        sys.exit(int(sys.argv[2]))
    MODES[sys.argv[1]]()
    out.flush()
