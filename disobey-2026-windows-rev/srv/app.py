from __future__ import annotations

import os

from flask import Flask, jsonify, request

app = Flask(__name__)

FLAG = os.environ.get("FLAG", "FLAG{dev}")

MBA_CHECK = b"0CAF3"


def stage5(key: str) -> bool:
    buf = 0
    for i, ch in enumerate(key):
        if ord(ch) & (1 << 2):
            buf |= 1 << i
    return buf == 0x230E29


def stage4(key: str) -> bool:
    key_slice = key[18:23]
    if len(key_slice) != 5:
        return False

    if key_slice.encode("latin-1").lower() != MBA_CHECK.lower():
        return False

    return stage5(key)


def stage3(key: str) -> bool:
    key_slice = key[12:17]
    if len(key_slice) != 5:
        return False

    total = sum(ord(ch) for ch in key_slice)
    if total != 257:
        return False

    return stage4(key)


def stage2(key: str) -> bool:
    if len(key) <= 6:
        return False

    if key[6] != "9":
        return False

    return stage3(key)


def stage1(key: str) -> bool:
    if len(key) != 23:
        return False

    for i, ch in enumerate(key):
        is_delim = i in (5, 11, 17)
        if is_delim:
            if ch != "-":
                return False
        else:
            if not (ch.isascii() and ch.isalnum()):
                return False

    if key[0:3].lower() != "dis":
        return False

    return stage2(key)


def check_key(key: str) -> bool:
    return stage1(key)


@app.post("/check")
def check() -> tuple:
    data = request.get_json(silent=True) or {}
    key = data.get("key")
    if not isinstance(key, str):
        return jsonify({"ok": False, "error": "key must be a string"}), 400

    if check_key(key):
        return jsonify({"ok": True, "flag": FLAG})

    return jsonify({"ok": False}), 403


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000)
