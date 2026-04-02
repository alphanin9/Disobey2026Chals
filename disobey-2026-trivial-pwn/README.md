# Challenge name

Blindness

# Category

Pwn

# Folder structure
- ./public: Distributed files - note that all binaries should be extracted from container for exploit

# Flag(s)

`DISOBEY[s0m3t!m3s_pwn3r_does_not_n33d_3y3s_2_see]`

## Installation (and reset) instructions

`docker-compose up -d`, extract the libc/ld/built binary from container instance for distributed files

## Open ports

Port 4008 is exposed to the world

# Testing instructions / walkthrough

Note you can break libc ASLR due to you being able to add stuff

Note you can write 8 bytes to exitcode (enough for /bin/sh)

Add delta to system()/execve() to some GOT entry

Call it via ROP with exitcode being RDI

Solvescript provided - note that you need to be fast once you get shell

# Your contact information.

<REDACTED>
`alphanine` @ Discord
