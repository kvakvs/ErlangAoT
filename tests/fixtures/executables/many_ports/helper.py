"""Helper program of the many_ports case: writes back every input line until its input ends."""
import sys

for line in sys.stdin.buffer:
    sys.stdout.buffer.write(line)
    sys.stdout.buffer.flush()
