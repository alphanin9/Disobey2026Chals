# /// script
# requires-python = ">=3.14"
# dependencies = [
#     "pefile>=2024.8.26",
#     "pycryptodome>=3.23.0",
# ]
# ///

import struct
import sys

from pefile import PE, SectionStructure
from Crypto.Cipher import AES

pe = PE("./build/windows/x64/release/runtime.dll")
pe.write("./build/windows/x64/release/runtime.bak.dll")
pe.full_load()

for i in pe.sections:
    if i.Name.startswith(b"MVM") and not i.Name.endswith(b"\x01"):
        print(i.Name)
        aligned_size = i.Misc_VirtualSize + (-i.Misc_VirtualSize & 15)
        i.Misc_VirtualSize = aligned_size # We will dump a little more padding in, doesn't matter...

        aes_key = struct.pack("IIII", (~i.VirtualAddress) & 0xFFFFFFFF, (i.Misc_VirtualSize ^ 0x67676767) & 0xFFFFFFFF, i.SizeOfRawData, ((~i.Characteristics) ^ 0x10101010) & 0xFFFFFFFF)
        print(f"AES key: {aes_key.hex()}")
        nonce = b"\x43" * 16
        print(f"Nonce: {nonce.hex()}")
        print(f"First bytes: {pe.get_dword_at_rva(i.VirtualAddress):08x}")

        cipher = AES.new(aes_key, AES.MODE_CBC, nonce)

        encrypted = cipher.encrypt(pe.get_data(i.VirtualAddress, i.Misc_VirtualSize))
        pe.set_bytes_at_rva(i.VirtualAddress, encrypted)
        i.Name = b"MVM" + b"\x00" * 4 + b"\x01"
        pe.write("./build/windows/x64/release/runtime.dll")
        sys.exit(0)
