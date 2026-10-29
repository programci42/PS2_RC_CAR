# Integrated PS2 Wi-Fi application

See the [project overview](../README.md), [quick start](RELEASE_README_EN.txt), [architecture](../docs/en/WIFI_ARCHITECTURE.md), [HTTP API](../docs/en/WIFI_API.md) and [validation record](../docs/en/WIFI_VALIDATION.md).

Production executable: **[Release/Wifi.elf](Release/Wifi.elf)**. TV and web UI are English in 0.4.30. Tests, emulator tours, private seed/profile/CSV files and logs do not belong in the public release.

Build from this directory with `powershell -ExecutionPolicy Bypass -File build.ps1`. The build preserves existing private seeds and creates a fresh random source only when one is absent. Never commit a generated seed or real network password.
