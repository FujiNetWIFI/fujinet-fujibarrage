#!/usr/bin/env python3
"""pyseat.py HOST:PORT ROOM NAME DELAY SECONDS -- a person that sits in ROOM
after DELAY seconds, keeps the link (credits, pings) for SECONDS, then BYE.
(A rig helper: more people in a waiting room, e.g. the Arcadia's
tools/arcsim/scenes/room3.txt.)"""
import socket, sys, time

def fletcher(b):
    s1 = s2 = 0
    for x in b:
        s1 = (s1 + x) % 255
        s2 = (s2 + s1) % 255
    return bytes([s1, s2])

def cobs(raw):
    out, run = bytearray(), bytearray()
    for x in raw:
        if x == 0:
            out += bytes([len(run) + 1]) + run; run = bytearray()
        else:
            run.append(x)
    out += bytes([len(run) + 1]) + run
    return bytes(out) + b"\0"

def frame(t, payload=b""):
    raw = bytes([t]) + payload
    return cobs(raw + fletcher(raw))

hp, room, name, delay, secs = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4]), float(sys.argv[5])
time.sleep(delay)
h, p = hp.split(":")
s = socket.create_connection((h, int(p)))
hello = bytes([1, 0]) + room.encode().ljust(8, b"\0") + name.encode().ljust(8, b"\0")
hello += bytes([160, 0, 56, 54, 60, 7, 4, 240, 0, 0, 0, 0])
s.sendall(frame(ord("H"), hello))
s.settimeout(0.5)
taken = cred = 0
t0 = last = time.time()
while time.time() - t0 < secs:
    try:
        d = s.recv(4096)
        if not d:
            break
        taken = (taken + len(d)) & 255
        if (taken - cred) & 255 >= 48:
            s.sendall(frame(ord("C"), bytes([taken]))); cred = taken
    except socket.timeout:
        pass
    if time.time() - last > 4:
        s.sendall(frame(ord("C"), bytes([taken])) + frame(ord("P"))); cred = taken; last = time.time()
s.sendall(frame(ord("B")))
print(name, "left", flush=True)
