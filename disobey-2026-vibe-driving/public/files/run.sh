#!/bin/bash
set -eu

timeout --foreground 120 qemu-system-x86_64 \
  -m 256M \
  -cpu qemu64,+smep,+smap \
  -kernel bzImage \
  -initrd rootfs.cpio.gz \
  -nographic \
  -append "console=ttyS0 oops=panic panic=1 kaslr" \
  -no-reboot \
  -monitor none
