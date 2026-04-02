# Challenge name

> Vibe Driving

# Category

Pwn

# Flag(s)

`DISOBEY[wh0_n33ds_arb1trary_wr1t3_wh3n_y0uRe_0n_rootfs?]`

# Rundown

Bad kernel module gives arbitrary read + gifts some pointers (task struct and a kernel function), use those to break ASLR and traverse kernel file system structs, use rootfs storing files in page cache to read flag from memory

# Folder structure
- ./container: Docker compose stuff for remote
- ./public: Public info
- ./vulnerable-module: Source code of bad kernel module

# For hosted challenges

## Installation (and reset) instructions

```sh
cd container
docker-compose up -d
```

Note: this doesn't use KVM for virtualization acceleration because I'm unsure of how to set that up
Currently emulated speed is good enough

## Open ports

6666

# Testing instructions / walkthrough

Compile exploit.c with static link (`gcc -O2 -static -flto -fno-ident -fno-unwind-tables -s exploit.c -o exploit`) or use precompiled exploit (`./exploit`), test `solve.py` solvescript (requires Pwntools)

I don't know Linux enough to check for unintended solves

# Hints

- How does the Linux file system work data structure-wise?
- Where does rootfs keep files?

# Your contact information.

<REDACTED>

`alphanine` on Discord
