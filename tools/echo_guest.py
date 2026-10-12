"""The smallest SeriousBridge guest: accepts the Serious Sam plugin's connection and shows what it sends.

    uv run -q --with websockets python tools/echo_guest.py [--port 25610] [--seconds 20] [--level-file]

Start it instead of the real guest (the Sam plugin connects to 127.0.0.1:25610 within a second), then play: it prints
the level Sam announces, how many messages of each type arrive per second, and the latest camera. A new bridge
starts here: replace the prints with the guest's side (docs/PROTOCOL.md). No frames are published (the shared memory
name it gives doesn't exist), so Sam draws only itself.
"""
import argparse
import asyncio
import collections
import json
import os
import struct
import time

import websockets

stats = collections.Counter()
latest = {}


def describe_tri(path: str) -> str:
    """The .tri header: triangle count and bounds (docs/PROTOCOL.md, Geometry files)."""
    try:
        with open(path, "rb") as f:
            head = f.read(40)
        magic, version, count, _flags = struct.unpack_from("<4siii", head)
        bbox = struct.unpack_from("<6f", head, 16)
        return f"{magic.decode()} v{version}, {count} triangles, bounds {tuple(round(v) for v in bbox)}"
    except (OSError, struct.error) as e:
        return f"unreadable ({e})"


async def serve(ws, show_level: bool) -> None:
    print("Sam connected")
    await ws.send(json.dumps({"t": "hello", "v": 1, "shm": "Local\\EchoGuestNoFrames", "pid": os.getpid()}))
    try:
        async for raw in ws:
            try:
                m = json.loads(raw)
            except json.JSONDecodeError:
                stats["(bad json)"] += 1
                continue
            t = m.get("t", "?")
            stats[t] += 1
            latest[t] = m
            if t == "hello":
                print("hello:", m)
            elif t == "level":
                print(f"level: {m.get('name')} ({m.get('tris')} triangles)" + (f": {describe_tri(m.get('file', ''))}" if show_level else ""))
            elif t == "reset":
                print("reset:", m.get("what"))
    except websockets.ConnectionClosed:
        pass
    print("Sam disconnected")


async def report(seconds: float) -> None:
    start = time.time()
    while seconds <= 0 or time.time() - start < seconds:
        await asyncio.sleep(2.0)
        rates = ", ".join(f"{t} {n / 2:g}/s" for t, n in sorted(stats.items()))
        cam = latest.get("cam")
        where = f"; camera at {[round(v, 1) for v in cam['p']]} heading {cam['r'][0]:.0f}" if cam else ""
        print(f"[{time.time() - start:5.1f}s] {rates or 'nothing yet'}{where}")
        stats.clear()


async def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=25610)
    ap.add_argument("--seconds", type=float, default=0.0, help="stop after this long (0 = run until Ctrl+C)")
    ap.add_argument("--level-file", action="store_true", help="read the announced .tri file's header")
    a = ap.parse_args()
    async with websockets.serve(lambda ws: serve(ws, a.level_file), "127.0.0.1", a.port, max_size=None):
        print(f"waiting for Sam on 127.0.0.1:{a.port}")
        await report(a.seconds)


if __name__ == "__main__":
    asyncio.run(main())
