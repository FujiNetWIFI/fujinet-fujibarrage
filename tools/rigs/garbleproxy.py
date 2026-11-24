#!/usr/bin/env python3
"""garbleproxy.py LISTEN SERVER AT -- a TCP proxy that, AT seconds after it
starts, flips one byte (a non-zero one, kept non-zero) of the server's next
chunk to the client, once: the client must see a bad frame, RESYNC, and
carry on. (A rig helper: tools/arcsim/scenes/garble.txt.)"""
import asyncio, sys, time
t0 = time.time()
done = False

async def pipe(r, w, garble):
    global done
    try:
        while data := await r.read(4096):
            if garble and not done and time.time() - t0 > float(sys.argv[3]):
                b = bytearray(data)
                for i in range(len(b) // 2, len(b)):
                    if b[i] not in (0, 0x55):
                        b[i] ^= 0x55
                        done = True
                        print("garbled a byte at", round(time.time() - t0, 1), flush=True)
                        break
                data = bytes(b)
            w.write(data)
            await w.drain()
    except ConnectionError:
        pass
    finally:
        w.close()

async def handle(cr, cw):
    sr, sw = await asyncio.open_connection("127.0.0.1", int(sys.argv[2]))
    await asyncio.gather(pipe(cr, sw, False), pipe(sr, cw, True))

async def main():
    srv = await asyncio.start_server(handle, "127.0.0.1", int(sys.argv[1]))
    async with srv:
        await srv.serve_forever()

asyncio.run(main())
