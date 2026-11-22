#!/usr/bin/env python3
"""tapproxy.py LISTEN_PORT SERVER_PORT LOG -- a TCP proxy that writes every
frame the server sends (COBS-decoded, its sums checked) to LOG, a line
each: its type's letter and its payload in hex. For checking a client
against what it was sent (e.g. the field its TERRAIN frames make)."""
import asyncio
import sys


def cobs(b):
    out, i = bytearray(), 0
    while i < len(b):
        n = b[i]
        if n == 0:
            return None
        out += b[i + 1:i + n]
        i += n
        if n < 255 and i < len(b):
            out.append(0)
    return bytes(out)


def fletcher(b):
    s1 = s2 = 0
    for x in b:
        s1 = (s1 + x) % 255
        s2 = (s2 + s1) % 255
    return s1, s2


async def pipe(r, w, log):
    buf = bytearray()
    try:
        while data := await r.read(4096):
            w.write(data)
            await w.drain()
            if log is None:
                continue
            for x in data:
                if x:
                    buf.append(x)
                    continue
                f = cobs(bytes(buf))
                buf.clear()
                if f and len(f) >= 3 and fletcher(f[:-2]) == (f[-2], f[-1]):
                    log.write("%s %s\n" % (chr(f[0]), f[1:-2].hex()))
                    log.flush()
    finally:
        w.close()


async def handle(cr, cw):
    sr, sw = await asyncio.open_connection("127.0.0.1", int(sys.argv[2]))
    log = open(sys.argv[3], "a")
    await asyncio.gather(pipe(cr, sw, None), pipe(sr, cw, log), return_exceptions=True)


async def main():
    srv = await asyncio.start_server(handle, "127.0.0.1", int(sys.argv[1]))
    async with srv:
        await srv.serve_forever()


asyncio.run(main())
