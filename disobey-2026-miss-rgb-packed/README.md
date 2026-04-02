# Challenge name

> Miss RGB

# Category

Pwn

# Rundown

Vulnerable driver provides arbitrary physmem RW via adding linear mapping of physmem to process page tables

Figure out how to make IOCTL succeed, use gifted CR3 and EPROCESS to get to target process, read flag from process memory or make target process write flag to message box

# Folder structure:
- ./public: Distributed files (vulnerable driver, target binary, ntoskrnl used on target machine)
- ./not_distributed: Files that are not distributed to public (compiled anticheat driver + precompiled exploit binary)
- ./client: Source code of example exploit
- ./anticheat: Source code of "anticheat" driver
- ./vulnerable: Source code of vulnerable driver
- ./target_binary: Source code of target process

# Flag(s)

`DISOBEY[w3_w1ll_add_cr3_shuffl1ng_t0_our_ant1ch3at_n3xt]`

Flag is hidden in "anticheat" driver binary (this is not to be distributed publically) and is injected into process address space via process notify callback + work item

## Installation (and reset) instructions

Installation handled by TSRY, chal is running on laptop provided by Esinko, exploit is submitted via USB stick/other physical measure

Reset is not really planned, competitors should be on their very best behavior and not try to wreck the machine for other participants

Note: ntoskrnl.exe provided in public files MUST be the same version as is running on exploited machine (reason: proper exploit will depend on process struct offsets that like to change around with Windows updates)

# Testing instructions / walkthrough

FAQ:
> Yes, flag is fake on challenge binary on pwn client too, this is intentional (cuz flag is written into process by driver)
> 
> You should really use statically linked binary because challenge machine probably doesn't have any deps (VC++ redist ETC)
> 
> One binary per exploit

An example exploit is provided along with the challenge.

Note that the "anticheat" is fairly primitive and provides for a lot of ways to get unintended solve :(

# Hints (optional)

IDK, take a look at Unknowncheats? This is fairly trivial driver exploitation chal given the absurdly broad physmem R/W primitive

# Your contact information.

<REDACTED>

`alphanine` on Discord
