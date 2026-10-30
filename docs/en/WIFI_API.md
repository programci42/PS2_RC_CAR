# HTTP API

Use `http://<PS2-IP>/` on a trusted LAN. No TLS or full user authentication is implemented. Compatibility command values retain their established spelling; the interface supports Turkish (default) and English. The shared choice is persisted in dil.json.

| GET request | Result |
|---|---|
| `/` / `/tools` | Control panel / USB and source-code tools |
| `/?ls=devices` | Device inventory, hub ports and available metadata |
| `/devices/refresh` | Asynchronous descriptor refresh |
| `/?usb=1` | Raw descriptors/interfaces/endpoints for device ID 1 |
| `/network` | Link/radio state, auto USB checks and hub diagnostics |
| `/language` | Shared TV/web language (`tr` or `en`) |
| `/relay/diagnostics` | USB result/status, output uncertainty and reset/release state |
| `/display` | Version, TV state and pad thread/DMA/init diagnostics |
| `/?display=1` / `0` | Enable / disable TV drawing |
| `/status` / `/telemetry` | Relay state / network, log, heap and battery estimates |
| `/?ls=mc0` | Recursive file tree; `mc1` and `mass` also work |
| `/?download=mc0:/Wifi/example.txt` | Download; HEAD returns size without file content |
| `/?view=mc0:/Wifi/example.txt` | Text, binary hex view or media element |
| `/?raw=mass:/picture.jpg` | Raw media content |
| `/?del=mc0:/Wifi/example.txt` | Delete one non-protected file; no directories/bulk deletion |
| `/?git=ileri5` / `geri3` | Forward five / reverse three seconds, then stop |
| `/?git=sol2` / `sag5` | Left two / right five seconds, then stop |
| `/?shutdown=-r-t4` / `-s-t1` | Restart in four / shut down in one second |
| `/?shutdown=-r` / `-s` | Immediate restart / shutdown request |
| `/?reload=1` | Reopen ELF; compatibility feature, not used by deployment tests |
| `/exec/status` | Native job state, result, output and compiler location |

SELECT and gamepad takeover interrupt timed movement. Held web buttons use asynchronous requests and heartbeats instead of full page navigation. Release, focus loss and stale-command expiry stop motion.

| POST endpoint | Purpose / required header |
|---|---|
| `/drive/forward`, `/back`, `/left`, `/right`, `/stop` (each under `/drive/`) | Motion/heartbeat; `X-PS2-Control: 1` |
| `/language` | Two-byte text body `tr` or `en`; `X-PS2-Control: 1`; save while stopped |
| `/settings`, `/battery`, `/clock` | JSON button settings / battery parameters / UTC; `X-PS2-Control: 1` |
| `/files/upload/begin?path=...&size=...` | Stage upload; `X-PS2-Files: 1` |
| `/files/upload/chunk?offset=...` | Ordered binary chunk; `X-PS2-Files: 1` |
| `/files/upload/commit?crc32=...&pcbackup=...` | Validate and replace; PC backup proof when required |
| `/files/upload/abort` | Cancel upload while preserving original |
| `/exec/source?lang=cpp` / `asm` | Compile/run source **on PS2**; `X-PS2-Exec: 1` |
| `/exec/cancel` | Cancel worker; `X-PS2-Exec: 1` |
| `/relay/recover` | Rebind unavailable relay driver while idle; `X-PS2-Exec: 1` |
| `/hub/power` | Explicit power request for eligible unpowered ports; `X-PS2-Exec: 1` |

Headers and path protections prevent some accidental operations, not hostile users on the LAN. System/boot/seed/profile files and the running ELF have additional file protections. Native code can still crash the console.

`/status` also includes `usbStatus`, `usbResult`, `usbControlResult`, `usbInterruptResult`, `resetPending`, `outputUncertain` and `releaseRequired`. In interrupt mode an unused control-path result of -1 is normal when the overall result and interrupt result are zero. OFF acknowledgement describes USB command delivery, not electrical contact continuity.
