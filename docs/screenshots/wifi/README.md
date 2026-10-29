# English Wi-Fi application gallery

Version **0.4.30**. Console images are real Play! window captures of a separate tour ELF using the production renderer. The tour values are synthetic. Browser images use the production English HTML/JavaScript with a localhost fixture server and a visible preview notice.

None of these screenshots is presented as a new physical Wi-Fi, relay or motor test. The PS2 was off when publication images were created.

| Screen | Image |
|---|---|
| Drive mode — console renderer | [View](console-drive-preview.jpg) |
| Network selection — console renderer | [View](console-networks-preview.jpg) |
| Password keyboard — console renderer | [View](console-keyboard-preview.jpg) |
| Connected/IP screen — console renderer | [View](console-network-status-preview.jpg) |
| Robot Control — browser fixture | [View](web-robot-preview.png) |
| File Upload & Download — browser fixture | [View](web-files-preview.png) |
| Run Command — browser fixture | [View](web-commands-preview.png) |
| Settings / battery / telemetry — browser fixture | [View](web-settings-preview.png) |
| USB inventory / on-console source editor — browser fixture | [View](web-tools-preview.png) |

Console reproduction: `make wifi-tour reports/ui-tour/WIFI_5.ELF`, then load each `reports/ui-tour/WIFI_*.ELF` in Play!. Browser reproduction: `node tools/web-preview.cjs --capture` with Playwright installed. These are documentation fixtures, never the production vehicle ELF.

[Promotional banner](../../../images/ps2-robot-banner.png) and [poster](../../../images/ps2-robot-poster.png) are separate AI-generated artwork based on the real vehicle. They are not screenshots or wiring diagrams. Generated with the built-in image tool; prompts requested a neat MDF/PS2 Slim/RTL8192CU/eight-relay/XL4016 vehicle composition without hands or a gamepad. The poster's regulator caption was corrected to 8.6 V.
