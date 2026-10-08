"""Helper program of the port_fairness case: writes lines as fast as it can, then an end line, then waits for its
input to end."""
import sys

LINE = b'x' * 60 + b'\n'
try:
    for _ in range(2_000):
        sys.stdout.buffer.write(LINE * 100)
    sys.stdout.buffer.write(b'end\n')
    sys.stdout.buffer.flush()
    sys.stdin.buffer.read()
except (BrokenPipeError, OSError):
    pass
