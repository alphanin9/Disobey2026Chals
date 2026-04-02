# Challenge name

> Exceptional

# Category

Rev

# Rundown

Utilizes SEH to obfuscate key checking functions, functions implementing key checking are in separate section that is decrypted at runtime

After local check key is submitted to remote, if key is proper you get flag

# Folder structure
- ./dist: Challenge source code
- ./public: Distributed files
- ./srv: Remote key checker, contains Dockerfile and docker-compose

# Flag(s)

`DISOBEY[tr1v1@11y_3xc3pt10n3l_c0d3]`

## Installation (and reset) instructions

```
cd srv
docker-compose up -d
```

## Custom domain

`exceptional.dissi.fi`

The custom domain is used for the key validation server in `./srv` that sends the flag to the user after validating the key is correct (just in case the player patches the binary to always query the flag from the server). The domain name is hardcoded in the executable.

If the domain name needs to be changed, you need to change it in dist/src/Main.cpp (line 19) and recompile the executable.

## Open ports

Port 8000 for key validation server in `./srv`


# Testing instructions / walkthrough

Solvescript (courtesy of ChatGPT):
```python
#!/usr/bin/env python3
import random
import string

MASK_TARGET = 0x230E29
DASH_POS = {5, 11, 17}
LENGTH = 23

ALNUM = string.ascii_lowercase + string.digits

def bit_needed(i: int) -> int:
    return (MASK_TARGET >> i) & 1

def mask23(s: str) -> int:
    m = 0
    for i, ch in enumerate(s):
        if ord(ch) & 0x04:
            m |= (1 << i)
    return m

def pick_alnum_for_bit2(need_set: int) -> str:
    cand = [c for c in ALNUM if ((ord(c) & 0x04) != 0) == bool(need_set)]
    if not cand:
        raise RuntimeError(f"No candidate satisfies bit2={need_set}")
    return random.choice(cand)

def gen_group3_sum257() -> str:
    pool = string.ascii_lowercase + string.digits
    while True:
        s = "".join(random.choice(pool) for _ in range(5))
        if sum(map(ord, s)) == 0x101:
            return s

def verify(key: str) -> bool:
    if len(key) != LENGTH:
        return False

    for i, ch in enumerate(key):
        if i in DASH_POS:
            if ch != "-":
                return False
        else:
            if not ch.isalnum():
                return False

    # corrected prefix
    if key[:3].lower() != "dis":
        return False

    # privileged gate
    if key[6] != "9":
        return False

    # group3 sum
    if sum(ord(c) for c in key[12:17]) != 0x101:
        return False

    # constant-return check => little-endian "0CAF3"
    if key[18:23].lower() != "0caf3":
        return False

    return mask23(key) == MASK_TARGET

def gen_key() -> str:
    out = ["?"] * LENGTH

    for p in DASH_POS:
        out[p] = "-"

    # corrected prefix
    out[0], out[1], out[2] = "d", "i", "s"

    # privileged gate
    out[6] = "9"

    # group3 sum constraint
    out[12:17] = list(gen_group3_sum257())

    # last group fixed by constant
    out[18:23] = list("0caf3")

    # fill remaining positions to satisfy global bit2 mask
    for i in range(LENGTH):
        if out[i] != "?":
            need = bit_needed(i)
            have = 1 if (ord(out[i]) & 0x04) else 0
            if have != need:
                raise AssertionError(f"Fixed char {out[i]!r} at {i} violates bitmask")
            continue
        out[i] = pick_alnum_for_bit2(bit_needed(i))

    key = "".join(out)
    if not verify(key):
        raise AssertionError("Generated key failed verification (logic mismatch).")
    return key

if __name__ == "__main__":
    for _ in range(5):
        try:
            print(gen_key())
        except:
            pass
```

# Hints (optional)

I don't know how Ghidra/Binja handle SEH, but look at more than the C pseudocode?

# Your contact information.

<REDACTED>

`alphanine` on Discord
