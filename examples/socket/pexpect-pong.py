#!/usr/bin/python3
# In MSYS2, use /mingw64/bin/python3
#
# wait for a "Ping" and Send a "Pong"
# Repeat this process.
#

import pexpect
from pexpect import popen_spawn

child = pexpect.popen_spawn.PopenSpawn("nc -UN /tmp/tio-socket1")

cnt = 0
while True:
    try:
        child.expect(r'Ping \d+[\r\n]+', timeout = 10)
        child.sendline(f"Pong {cnt:d}")
        cnt += 1
    except Exception as e:
        print(type(e))
        break

