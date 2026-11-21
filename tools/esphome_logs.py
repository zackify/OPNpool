#!/usr/bin/env python3
"""Stream (or save) the opnpool-1 ESPHome logs over the native API, no secrets needed (API is unencrypted on the LAN).

  uvx --from aioesphomeapi python tools/esphome_logs.py [seconds] [outfile] [tag-substring ...]

Defaults: 300 s, print to stdout, only lines containing "opnpool_chlor" or "opnpool_a5".
"""
import asyncio, re, sys, time
import aioesphomeapi

HOST = "192.168.0.233"

async def main():
    secs = float(sys.argv[1]) if len(sys.argv) > 1 else 300
    out = open(sys.argv[2], "a") if len(sys.argv) > 2 and sys.argv[2] != "-" else sys.stdout
    tags = sys.argv[3:] or ["opnpool_chlor", "opnpool_a5"]
    c = aioesphomeapi.APIClient(HOST, 6053, None)
    await c.connect(login=True)
    def on_log(l):
        m = l.message.decode() if isinstance(l.message, bytes) else l.message
        m = re.sub(r"\x1b\[[0-9;]*m", "", m)
        if any(t in m for t in tags):
            out.write(time.strftime("%H:%M:%S ") + m + "\n"); out.flush()
    c.subscribe_logs(on_log, log_level=aioesphomeapi.LogLevel.LOG_LEVEL_DEBUG)
    await asyncio.sleep(secs)

asyncio.run(main())
