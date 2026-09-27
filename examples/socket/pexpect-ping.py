#!/usr/bin/python3
# In MSYS2, use /mingw64/bin/python3
#
# Send a "Ping" and wait for a "Pong".
# Repeat this process.
#

import pexpect
from pexpect import popen_spawn

child = pexpect.popen_spawn.PopenSpawn("nc -UN /tmp/tio-socket0")

cnt = 0
while True:
    try:
        child.sendline(f"Ping {cnt:d}")
        cnt += 1
        child.expect(r'Pong \d+[\r\n]+', timeout = 10)
    except Exception as e:
        print(type(e))
        break

