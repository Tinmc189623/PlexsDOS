#!/usr/bin/env python3
"""
最简 QEMU 测试: 串口接 stderr, 跑 5 秒, 看输出
"""
import subprocess
import time
import os

QEMU = r"C:\Program Files\qemu\qemu-system-i386.exe"
WORKDIR = r"J:\APP\OS\x86\PlexsDOS"
FLOPPY = os.path.join(WORKDIR, "build", "plexsdos.img")
DISK   = os.path.join(WORKDIR, "build", "disk.img")
LOG    = os.path.join(WORKDIR, "build", "boot_serial.log")

# 清空日志
if os.path.exists(LOG):
    os.remove(LOG)

# 用 -serial stdio + -display none 让 QEMU 把串口输出到 stdout
# 加 -no-reboot 让启动失败时不会重启
args = [
    QEMU,
    "-fda", FLOPPY,
    "-hda", DISK,
    "-boot", "a",
    "-m", "64M",
    "-display", "none",
    "-no-reboot",
    "-serial", "file:" + LOG,
]

print("Starting QEMU...", flush=True)
proc = subprocess.Popen(
    args,
    cwd=WORKDIR,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    stdin=subprocess.DEVNULL,
    text=True,
    errors="replace",
)
print(f"QEMU PID: {proc.pid}", flush=True)

# 等 5 秒
time.sleep(5)

# 杀 QEMU
print("Killing QEMU...", flush=True)
proc.kill()
try:
    proc.wait(timeout=3)
except Exception:
    pass

# 读串口日志
print("\n=== Serial log ===", flush=True)
if os.path.exists(LOG):
    sz = os.path.getsize(LOG)
    print(f"Size: {sz} bytes", flush=True)
    if sz > 0:
        with open(LOG, "r", errors="replace") as f:
            print(f.read()[:3000])
    else:
        print("(EMPTY)")
else:
    print("Log file not created")
