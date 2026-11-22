#!/usr/bin/env python3
"""stallproxy.py LISTEN_PORT SERVER_PORT AT SECONDS -- a TCP proxy that holds
what each client sends, once, from AT seconds after it connects for SECONDS
(then passes it on, late). Held over a shot, the client's D for a segment
comes too late: the server gives up on it (the script's length and 4 s)
and goes on, so the client gets the next state and the field where it
waited for a segment. It must drop the shot and resync (R)."""
import asyncio
import sys
import time


async def pipe(r, w, hold_from=None, hold_for=0):
    held = bytearray()
    try:
        while True:
            try:
                data = await asyncio.wait_for(r.read(4096), 0.05)
            except asyncio.TimeoutError:
                data = None
            now = time.monotonic()
            holding = hold_from is not None and hold_from <= now < hold_from + hold_for
            if data == b"":
                break
            if data:
                held += data
            if held and not holding:
                if hold_from is not None and now >= hold_from + hold_for and hold_for:
                    print("stallproxy: released %d bytes" % len(held), flush=True)
                    hold_for = 0
                w.write(bytes(held))
                held.clear()
                await w.drain()
    except (ConnectionError, asyncio.CancelledError):
        pass
    finally:
        w.close()


async def handle(cr, cw):
    sr, sw = await asyncio.open_connection("127.0.0.1", int(sys.argv[2]))
    at = time.monotonic() + float(sys.argv[3])
    print("stallproxy: a client; holding its bytes in %s s for %s s" % (sys.argv[3], sys.argv[4]), flush=True)
    await asyncio.gather(pipe(cr, sw, at, float(sys.argv[4])), pipe(sr, cw))


async def main():
    srv = await asyncio.start_server(handle, "127.0.0.1", int(sys.argv[1]))
    async with srv:
        await srv.serve_forever()


asyncio.run(main())
