"""Helper program of the slow_owner case: writes the numbers 1 to N as lines as fast as it can, then an end line,
then waits for its input to end."""
import sys

count = int(sys.argv[1])
out = sys.stdout.buffer
for start in range(1, count + 1, 1000):
    out.write(b''.join(b'%d\n' % n for n in range(start, min(start + 1000, count + 1))))
out.write(b'end\n')
out.flush()
sys.stdin.buffer.read()
