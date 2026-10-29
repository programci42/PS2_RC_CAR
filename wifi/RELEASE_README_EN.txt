PS2 Wi-Fi Robot — English release

Wifi.elf is the single current production executable. It embeds the USB host,
RTL8192CU Wi-Fi, relay and storage modules. Do not install the test/tour ELFs.

For an existing installation, replace only mc0:/Wifi/WIFI.ELF. Preserve your
SEED.DAT, saved profiles, bilinenaglar.csv and ayarlar.json. Keep backups on PC.

First installation: build.ps1 creates a private random Release/Wifi.seed if it
does not already exist. Copy your private seed beside Wifi.elf on your setup
medium. Existing or invalid memory-card seeds are never silently overwritten.
Never publish or share seeds, saved profiles or network passwords.

Known networks: create bilinenaglar.csv beside the ELF. UTF-8 rows:
SSID;Password
ExampleWiFi;replace-with-your-password
The application can append the last IP and static-address preference.

Boot defaults to drive mode with the TV display off. L2+R2 toggles the display.
START switches drive/network screens; SELECT stops motors without disconnecting
Wi-Fi. R2 in drive mode reconnects to known networks when offline. SELECT+START
held for one second requests a system restart.

Open http://<PS2-IP>/ for Robot Control, File Upload & Download, Run Command and
Settings. /tools exposes USB inventory and the on-console C++ statement subset /
MIPS assembler. It is not a full C++ standard-library compiler.

USB Wi-Fi presence is checked every 30 seconds while offline. Connected sessions
are not scanned/restarted by that timer; active WPA/DHCP exchanges are preserved.
The gamepad loop has higher priority than networking and file transfers.

Use only on a trusted local network. HTTP file/command/native-code tools are not
an Internet-facing security boundary. Battery values are estimates, not sensor
measurements. Never energize all motor relays at once. Startup may briefly pulse
only unused R1 (100 ms); this is skipped when R1 belongs to a motor group.

See the repository README, architecture, API and validation documents for the
hardware compatibility limits, source credits and known limitations.
