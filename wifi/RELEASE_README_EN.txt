PS2 Wi-Fi Robot — Turkish / English release 0.4.33

Turkce varsayilandir. Web panelinde AYARLAR > Dil ile Turkce / English secin.
Secim ELF klasorundeki dil.json dosyasina kaydedilir; TV ve web ayni dili kullanir.
Turkish is the default. Settings > Language selects Turkish / English and saves
the choice in dil.json beside the ELF. TV Turkish uses the debug font's ASCII
character set. API keys, filenames and user source code remain compatible.

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
measurements. Never energize all motor relays at once. Startup does not pulse
any relay. Group changes remap existing gamepad relay actions as well as web
movement. After a USB fault, release the controls before resuming. A failed USB
OFF is not treated as confirmed OFF; the IOP retries stopping independently.
Software cannot detect welded relay contacts or repair damaged hardware.

0.4.33 retries failed hub status monitoring with bounded backoff, re-enumerates
a connected disabled child port and removes the three-attempt relay recovery
limit. These are recovery fixes, not proof that an unpowered hub can supply the
relay board reliably. Physical hub/motor-load validation is still required.

See the repository README, architecture, API and validation documents for the
hardware compatibility limits, source credits and known limitations.

Relay Test: individual R1-R8 toggles with acknowledged USB state, opposite-pair interlock, gamepad priority and expiring web lease. Green is not contact feedback. USB failure can prevent OFF reaching the board.
