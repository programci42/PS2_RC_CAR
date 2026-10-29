# Validation record — 0.4.30

Different kinds of evidence are deliberately separated below. A UI preview, emulated USB device or passing host test is not proof of physical RF or motor operation.

| Layer | Result / scope |
|---|---|
| Host protocol suite | 552,928 checks passed: packet framing, WPA2/CCMP and TKIP group paths, DHCP, ICMP, HTTP/TCP, file service and related state |
| Renderer | 3,000 checks passed against the production renderer, including known-network markers, button labels, release/conflict/disconnect states and password non-disclosure |
| Native EE/IOP harness | Actual MIPS execution, PS2-local statement compiler/assembler, worker timeout, pad/video priority, USB driver recovery and 30-second offline policy passed in the project Play! harness |
| USB inventory | 13 production-reader cases passed, including moved device IDs, unplug, timeout/DMA ownership and cached presence checks without USB control transfers |
| Hub mapping/reset | Eight ports × eight child-list orders, port-eight bitmap, reset-token release, retries, cancellation and timer deadlines passed |
| Relay lifecycle | 12 driver lifecycle cases passed, including rebind, pending-transfer ownership and stale callbacks; startup signal commands target only R1 |
| Browser | Five English pages captured from the actual HTML/JS with clearly labeled fixture data; zero page JavaScript errors |
| Physical PS2 Wi-Fi | Owner tested real RTL8192CU association, WPA2, DHCP/IP and network access during development |
| Physical hub/relay | Owner confirmed relay operation through the four-port USB hub after the 0.4.29 driver changes |
| Recorded remote reboot | 0.4.28 → 0.4.29 returned over HTTP in 44.143 seconds; version and pad-tick reset were observed |
| English 0.4.30 | Built and tested locally. Not installed/retested on the powered-off physical console during publication |

The owner cancelled further restart testing because the PS2 was powered off. Therefore this release does **not** claim a completed repeated-reboot endurance campaign or physical tests of every hub socket/permutation.

## Reproduce

From `wifi/`, run `build.ps1`, `node tests/physical_protocol.cjs`, the inventory/reset-watchdog host tests in the main README, and `node tests/web_drive_hold.cjs`. These tests use public synthetic vectors, not a real network password.

Native integration additionally needs the Play! changes in `wifi/emulator-patches/`, a built `Rtl8192CuNetTest` executable and its normal runtime dependencies. Set `PS2_TEST_EMULATOR` to that executable and run `node tools/test-native-headless.cjs`. No BIOS is distributed or required by the Play! HLE harness. Emulator-only tour ELFs must never be installed as the vehicle release.

For web previews, install Playwright in your development environment and run:

```powershell
node tools/web-preview.cjs --capture
```

The preview server binds only to localhost, simulates API responses and sends no USB, Wi-Fi or movement commands. Without `--capture`, it prints a local preview address. Captures go under `reports/web-screenshots` unless `SCREENSHOT_DIR` is set.

## Remaining limits and issues

Field logs have contained intermittent DHCP timeouts and access-point disconnects. The 30-second offline scan and automatic recovery address recovery behavior; they do not prove that every source of radio loss is eliminated. Keep useful recent diagnostics when investigating these events.

Gamepad readiness and real button detection are separate observations. During development, a controller required physical unplug/replug before real L2+R2 events resumed. The software now checks packet validity/DMA progress and isolates pad RPC initialization from normal control/network threads. Do not describe that as a universal fix for all physical controller faults.

The Wi-Fi driver is not a generic class driver for arbitrary chipsets. The hub source supports descriptor-based routing, but compatibility/power behavior still depends on actual hardware. Performance is bounded by the PS2 USB host, the current radio mode, software protocol processing and memory-card writes.
