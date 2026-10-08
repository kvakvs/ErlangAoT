"""Helper program of the busy_ports case: waits a second, then reads 4-byte-length packets until one says end and
answers with the number of other bytes it read."""
import sys
import time

inp = sys.stdin.buffer
time.sleep(1)
total = 0
while True:
    head = inp.read(4)
    if len(head) < 4:
        break
    body = inp.read(int.from_bytes(head, 'big'))
    if body == b'end':
        break
    total += len(body)
answer = str(total).encode()
sys.stdout.buffer.write(len(answer).to_bytes(4, 'big') + answer)
sys.stdout.buffer.flush()
sys.stdin.buffer.read()
