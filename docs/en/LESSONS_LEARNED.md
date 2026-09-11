# Problems Solved and Lessons Learned

This file records the failures that shaped the working PS2 robot vehicle so they are not repeated.

## Power wiring: thin cable melted and reset the console

The PS2 Slim and relay board appeared to work over thin wire while the motors were disconnected. Once the RS-540 motors were connected, their start-up current caused a large voltage drop. The cable insulation began to melt and the PS2 reset immediately. The working correction was thicker, correctly sized cable with firm joints. The final design also calls for branch fusing and keeping motor-current paths short. A motor that starts is a much harder load than an idle relay board.

The 11.1 V 3S LiPo is divided into two branches. Branch 1 passes through an XL4015 buck converter adjusted to 8.50 V for the externally powered PS2 Slim SCPH-70004. Branch 2 remains at battery voltage and feeds NC1–NC4 from battery negative and NO1–NO4 from battery positive. COM1 and COM2 feed the parallel left motors; COM3 and COM4 feed the parallel right motors. The relay board uses both PS2 USB ports: one cable for 5 V power and one for HID data.

## Direction conflicts and relay contact stress

Simultaneous opposite commands could request unsafe or contradictory motor states. The software now checks opposing directions and commands all movement relays OFF when a conflict exists. Direction changes use an OFF-before-ON sequence and a configurable dead time, 100 ms by default, to reduce contact arcing. Controller disconnect, SELECT emergency stop, and program exit each send individual OFF commands and verify their results.

## Undocumented USB HID behavior

The Seda relay board did not provide a reliable public protocol specification. A retained Windows tester served as the reference. The custom IOP driver matches VID `0x178E` and PID `0x07D9`, discovers a HID interface and interrupt OUT endpoint, and supports 12 transfer variants. AUTO mode first sends the working eight-byte payload through interrupt OUT, then diagnostic modes can try alternative report length, type, recipient, and command-byte positions.

Examples: relay ON commands are `1`, `2`, `3`, `4`; corresponding OFF commands are `17`, `18`, `19`, `20`. The normal payload is `01 00 00 CMD 00 00 00 00`. The EE application sends the byte over SIF RPC; the IOP module performs the USB transfer and returns result, status, control, and interrupt diagnostics.

## Why the ELF shrank from 1.44 MB to 248 KB

`relay.elf` is the developer build and is 1,509,884 bytes, about 1.44 MiB. Most of the extra size is DWARF debugging data: source-line maps, symbol names, type descriptions, and variable information. The PS2 does not execute that map; it loads the executable LOAD segments. `build.ps1` copies the developer ELF to `role.elf` and runs `mips64r5900el-ps2-elf-strip --strip-debug`. The distributable is 254,224 bytes, 248.3 KiB in Windows, while retaining the embedded `usbd.irx`, custom relay IOP module, SIF RPC protocol, settings, memory-card support, and executable data.

## Booting automatically on the real PS2

The target console is a PS2 Slim SCPH-70004 with an external 8.5 V supply input. Copy `role.elf` to a dedicated path on the Free McBoot memory card with uLaunchELF. Open Free McBoot Configurator, enter **Configure E1 launch keys**, assign **AUTO** to that ELF path, save the CNF to MC0, and reboot. This makes the vehicle application start automatically. Keep a tested escape key or alternate launcher entry before enabling AUTO.

A lesson preserved in `C:\ps2dev\projeler\PS2OS_MLM\COZULEN_SORUNLAR.md` also applies here: an ELF launched through uLaunchELF may inherit a prepared IOP environment, while Free McBoot AUTO can start it in a cleaner state. A self-contained ELF therefore performs its own IOP reset, `SifLoadFileInit`, IOP heap initialization, and SBV patches before loading embedded IRX modules. The vehicle application follows this discipline in `main.c`.

## Motion programs and diagnosis

The application records up to 128 one-second movement or wait steps. L1 saves `mc0:SYS-CONF/ROLEPRG.TXT`, L2 loads it, and START replays it. R1 opens settings for VID, PID, USB send mode, automatic USB loading, and dead time. On-screen result/status/control/interrupt values remain essential because PCSX2 can validate menus and controller logic but cannot reliably reproduce the real relay USB path.

## Road-test videos

- [PS2 robot vehicle short 1](https://www.youtube.com/shorts/x-vaeMoXdgk)
- [PS2 robot vehicle short 2](https://www.youtube.com/shorts/5uW2evH4y8U)
