# AGENTS.md: debugging and iterating on this OPNpool fork

This is zackify's fork of cvonk/OPNpool (ESPHome external component for Pentair pool controllers over RS-485).
Branch `zs/salt-cell-sensors` is based on `zs/no-pump` (the pump-override work) and adds the salt cell support.
The device config is NOT in this repo: it is `pool/opnpool-1.yaml` on the same-named branch of `zackify/pool`
(that YAML loads this repo as `external_components: path: OPNpool/components`).

The controller: ESP32-S3 `opnpool-1`, static IP 192.168.0.233, Wi-Fi `knox`, ESPHome API (unencrypted) on 6053,
OTA password in `pool/secrets.yaml` of the `pool` repo. Its pool controller is a SunTouch (bus address 0x10); our
packets go out with source 0x20 (EasyTouch address), exactly like the remote does.

## What this branch adds (salt cell)
- Sensors: Chlorinator Salt (ppm), Level (%), Status (text: OK / Low flow / Low salt / Very low salt / Clean cell / Cold),
  Error Byte (debug, raw), Name.
- Binary sensors, one per error bit: low flow 0x01, low salt 0x02, very low salt 0x04, clean cell 0x10, cold 0x40.
  NOTE: bit meanings are NOT all verified. Upstream labelled 0x04 "high salt", but the real cell read 0x84 at 2550 ppm
  (low, not high), so it is now "Very low salt" (matches other Pentair protocol notes, still unconfirmed). 0x80 is
  probably "check PCB" but is still treated as OK. Verify by
  triggering states and reading the log, then fix the mapping in `core/poolstate.h`
  (`poolstate_chlor_status_typ_t`) and `core/poolstate_rx.cpp` (`_get_chlor_status_from_error`).
- Receive-filter fix: the cell's replies are addressed to 0x00; `network_rx_msg` used to drop them.
- Number `chlorinator_level_set` (0-100 %): sends the same A5 command as the remote (see protocol below). The controller
  stores the setpoint, so ONE command per change is enough; nothing needs to be re-sent.

## Protocol notes (captured from the real bus)
Controller <-> cell, IC protocol (`10 02 ... 10 03`):
- controller -> cell: dst 0x50, typ 0x11 LEVEL_SET, 1 byte = output % (e.g. 0x1A = 26). Repeats every few seconds.
- cell -> controller: dst 0x00, typ 0x12 LEVEL_RESP, 2 bytes = [salt/50, error bits]. e.g. `33 84` = 2550 ppm, 0x84.
- cell -> controller: dst 0x00, typ 0x01 MODEL_RESP-ish, `00 00`.
Controller bus, A5 protocol:
- remote -> controller: src 0x20 dst 0x10 typ 0x99 (CHLOR_SET), 4 bytes `[spa byte, pool %, 00, 00]`.
  The controller acks with typ 0x01 (SET_ACK) data `99`.
- remote -> controller: typ 0xD9 (CHLOR_REQ), 1 byte `00`; controller answers broadcast typ 0x19 (CHLOR_RESP), 22 bytes:
  `[spa byte, pool %, 0x81, salt/50, status, 00, 16-char name "Intellichlor--40"]`.
- Observed pairs (pool % -> first byte): 10 -> 01, 26 -> 03, 40 -> 05, 50 -> 05. The first byte is the spa setpoint,
  which the remote changes together with the pool value. We never invent it: the number entity echoes the last value
  the controller reported, so only the pool % changes.
- Safety: the number refuses to send until a CHLOR_RESP readback has been seen (a CHLOR_REQ is sent periodically).

## Build (no secrets in git)
Build in a scratch dir that has a copy of the YAML, the secrets, and a symlink to this repo:
```bash
S=$(mktemp -d); mkdir $S/pool && cd $S/pool
cp ~/work/pool/pool/opnpool-1.yaml ~/work/pool/pool/secrets.yaml .   # real secrets are in the private pool repo
ln -s ~/work/OPNpool OPNpool                                        # the YAML says path: OPNpool/components
esphome compile opnpool-1.yaml        # ~1-2 min; "Successfully compiled program" = OK
```
For a compile-only check use a dummy secrets.yaml instead (wifi_ssid, wifi_password, domain_name, ota_password,
api_encryption_key, ap_password). Never commit a dummy secrets file.
Gotchas:
- `components/opnpool/__init__.py` REWRITES the id enums in `core/opnpool_ids.h` at config time from its CONF_* lists.
  Edit the Python lists, keep the header in sync, and commit whatever the build regenerated.
- Files compile with `-Wall -Wextra -Wunused-parameter` as errors: mark unused params `(void)x;`.
- Every new A5 message needs: an enum value in `datalink_pkt.h`, a struct + union member + `X(...)` row in
  `network_msg.h`, and a `case` in the big switch in `core/poolstate_rx.cpp` (else the build fails or it is ignored).
- New HA entity kinds also need a setter in `core/opnpool.{h,cpp}` (`set_<key>_<kind>`) and a CONF_* entry.

## Flash (OTA) and roll back
```bash
cd $S/pool && esphome upload opnpool-1.yaml --device 192.168.0.233     # ~30 s; device is back ~10 s later
```
- Fallback if Wi-Fi breaks: the device opens hotspot "opnpool-1 Fallback" (password = `ap_password` in the pool repo's
  `pool/secrets.yaml`); join it and open 192.168.4.1 to re-enter Wi-Fi. USB is the last resort.
- Rollback image (pre-salt-cell firmware: `zs/no-pump` + `main` YAML) was saved to
  `~/work/opnpool-firmware-backup/opnpool-1-ROLLBACK-zs-no-pump-main.ota.bin` (OTA) and `.factory.bin` (USB, offset 0).
  OTA it back with `esphome upload opnpool-1.yaml --device 192.168.0.233 --file <that .ota.bin>`.
  To rebuild instead: check out `zs/no-pump` here and `main` in `zackify/pool`, then compile and upload.

## Debug logging
The YAML sets `logger: level: DEBUG, initial_level: INFO` with two DEBUG tags:
- `opnpool_chlor`: every salt cell (IC) packet in hex, each decoded LEVEL_RESP with all error bits, CHLOR_SET/RESP.
- `opnpool_a5`: every A5 packet except the routine STATE_BCAST (src/dst/typ/payload hex).
Stream them (works without a USB cable, no secrets needed):
```bash
uvx --from aioesphomeapi python tools/esphome_logs.py 300 /tmp/capture.log        # 5 min to a file
uvx --from aioesphomeapi python tools/esphome_logs.py 60 - opnpool_chlor           # one tag to stdout
```
or `esphome logs opnpool-1.yaml --device 192.168.0.233`.

Workflow to learn an unknown command: capture, press the button on the remote, find the new `typ=0x..` packets,
change one value at a time (e.g. 10, 26, 40, 50 %), diff the payloads, then add the message type (see Gotchas).

## Test checklist after a flash
1. Device answers ping and the API; `Chlorinator Salt/Level/Status/Name` have values within ~1 minute.
2. Salt cell error bits: trigger the flow switch (turn the pump off briefly) and watch `error=0x..`, the Low Flow
   binary sensor and the status text. Record any bit that doesn't match its label.
3. Number: change "Chlorinator Level Set" in HA, check the log shows `CHLOR_SET`, the controller acks (`typ=0x01 data=[99]`),
   the next `CHLOR_RESP` shows the new %, the cell's `typ=0x11` byte follows (hex of the %), and the remote shows it.
   Put it back to the original value afterwards (26 % at time of writing).
