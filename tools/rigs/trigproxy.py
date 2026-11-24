#!/usr/bin/env python3
"""trigproxy.py LISTEN_PORT SERVER_PORT FILE -- a TCP proxy whose faults a
scenario times: each time FILE appears (arcsim's `trigger FILE TEXT`) it is
read, removed and acted on, so a fault lands at a chosen moment (the items
line open, a tank talking, a shot under way) rather than at a clock time:
    cut             hang up on every client (CONNECTION LOST, a redial)
    garble          flip a byte of the server's next chunk (a bad frame,
                    RESYNC)
    stall SECONDS   hold what the clients send for SECONDS, then pass it on
                    late (a held D: the server gives up on the shot)
Every action is printed with its time."""
import asyncio
import os
import sys
import time

FILE = sys.argv[3]
t0 = time.time()
conns = []
state = {"garble": False, "stall_until": 0.0}


def say(*a):
    print("%.1f" % (time.time() - t0), *a, flush=True)


async def pipe(r, w, down):
    held = bytearray()
    try:
        while True:
            try:
                data = await asyncio.wait_for(r.read(4096), 0.05)
            except asyncio.TimeoutError:
                data = None
            if data == b"":
                break
            if data and down and state["garble"]:
                b = bytearray(data)
                for i in range(len(b) // 2, len(b)):
                    if b[i] not in (0, 0x55):
                        b[i] ^= 0x55
                        state["garble"] = False
                        say("garbled a byte")
                        break
                data = bytes(b)
            if not down and time.time() < state["stall_until"]:
                if data:
                    held += data
                continue
            if held:
                w.write(bytes(held))
                held.clear()
                say("stall over: the held bytes sent")
            if data:
                w.write(data)
            await w.drain()
    except (ConnectionError, asyncio.CancelledError):
        pass
    finally:
        w.close()


async def handle(cr, cw):
    sr, sw = await asyncio.open_connection("127.0.0.1", int(sys.argv[2]))
    conns.append((cw, sw))
    say("a client")
    await asyncio.gather(pipe(cr, sw, False), pipe(sr, cw, True))


async def watch():
    while True:
        await asyncio.sleep(0.05)
        if not os.path.exists(FILE):
            continue
        what = open(FILE).read().split()
        os.remove(FILE)
        say("trigger:", " ".join(what))
        if what[0] == "cut":
            for cw, sw in conns:
                cw.close()
                sw.close()
            conns.clear()
            say("cut")
        elif what[0] == "garble":
            state["garble"] = True
        elif what[0] == "stall":
            state["stall_until"] = time.time() + float(what[1])


async def main():
    srv = await asyncio.start_server(handle, "127.0.0.1", int(sys.argv[1]))
    async with srv:
        await asyncio.gather(srv.serve_forever(), watch())


asyncio.run(main())
