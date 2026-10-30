EE_BIN = rtl8192cu-probe.elf
.DEFAULT_GOAL := all
rtl8192cu-wifi.elf rtl8192cu-physical-protocol-test.elf reports/physical-join-template.elf: src/wifi_files.o
rtl8192cu-wifi.elf: ee/wifi_files_native.o
src/wifi_files.o: include/wifi_files.h
ee/wifi_files_native.o: include/wifi_files.h ee/wifi_files_native.h ee/wifi_seed.h
src/rtl_http.o ee/wifi.o ee/physical_join_test.o tests/physical_flow.o: include/wifi_files.h
rtl8192cu-wifi.elf rtl8192cu-physical-protocol-test.elf reports/physical-join-template.elf: src/robot_control.o
src/robot_control.o: include/robot_control.h
src/rtl_http.o: include/robot_web_asset.h
ee/wifi.o ee/physical_join_test.o: ee/robot_usb.h include/robot_control.h include/seda_relay_rpc.h
rtl8192cu-wifi.elf reports/physical-join-template.elf: seda_relay_irx.o
iop/relay/seda_relay.irx: iop/relay/seda_relay_irx.c iop/relay/imports.lst include/seda_relay_rpc.h
	$(MAKE) -C iop/relay
seda_relay_irx.c: iop/relay/seda_relay.irx
	bin2c $< $@ seda_relay_irx
EE_OBJS = ee/probe.o src/rtl_core.o usbd_irx.o rtl_probe_irx.o
EE_INCS = -Iinclude
EE_LIBS = -ldebug -lpatches -lc
rtl8192cu-wifi.elf rtl8192cu-physical-protocol-test.elf reports/physical-join-template.elf: src/rtl_http.o
rtl8192cu-physical-protocol-test.elf: tests/http.o
rtl8192cu-physical-protocol-test.elf: tests/robot.o
tests/http.o: tests/http.c include/rtl_http.h
src/rtl_http.o: include/rtl_http.h include/wifi_version.h
ee/wifi.o ee/physical_join_test.o tests/physical_flow.o: include/rtl_http.h
WPA_LIB_NAMES = aes ccm cipher cipher_wrap md sha1 pkcs5 nist_kw constant_time platform platform_util
WPA_LIB_OBJS = $(addprefix build-crypto/,$(addsuffix .o,$(WPA_LIB_NAMES)))
.PHONY: all
all: $(EE_BIN) rtl8192cu-selftest.elf rtl8192cu-usbnet-test.elf rtl8192cu-bringup.elf rtl8192cu-boot-model-test.elf rtl8192cu-wpa-crypto-test.elf rtl8192cu-scan-parser-test.elf
all: rtl8192cu-wifi.elf rtl8192cu-wifi-ui-preview.elf rtl8192cu-wifi-flow-test.elf
all: rtl8192cu-physical-protocol-test.elf
reports/physical-radio-test.elf: ee/physical_radio_test.o src/rtl_scan.o src/rtl_core.o usbd_irx.o rtl_wifi_irx.o
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) $(EE_LIBS)
reports/physical-join-template.elf: ee/physical_join_test.o src/wifi_app.o src/rtl_scan.o src/rtl_core.o src/rtl_supplicant.o src/rtl_tkip.o src/rtl_tkip_guard.o src/rtl_dhcp.o src/rtl_echo.o build-crypto/rtl_nonce.o build-crypto/rtl_wpa_crypto.o $(WPA_LIB_OBJS) usbd_irx.o rtl_wifi_irx.o
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) $(EE_LIBS)
ee/physical_join_test.o: ee/wifi_physical.h include/rtl_supplicant.h include/rtl_nonce.h include/wifi_app.h
ee/physical_protocol_test.o: tests/physical_protocol.c tests/physical_fixtures.h include/rtl_supplicant.h
tests/physical_flow.o: tests/physical_fixtures.h tests/tkip_fixtures.h ee/wifi_physical.h include/wifi_app.h include/rtl_supplicant.h include/rtl_tkip_guard.h
src/rtl_supplicant.o: include/rtl_supplicant.h include/rtl_wpa_crypto.h include/rtl_tkip.h
src/rtl_tkip.o: include/rtl_tkip.h
src/rtl_tkip_guard.o: include/rtl_tkip_guard.h
rtl8192cu-physical-protocol-test.elf rtl8192cu-wifi.elf: src/rtl_tkip.o src/rtl_tkip_guard.o
rtl8192cu-physical-protocol-test.elf: build-crypto/rtl_nonce.o
rtl8192cu-physical-protocol-test.elf: tests/nonce_setup.o
rtl8192cu-physical-protocol-test.elf: src/wifi_known.o tests/wifi_known.o
tests/wifi_known.o: tests/wifi_known.c tests/physical_fixtures.h include/wifi_known.h
tests/nonce_setup.o: tests/nonce_setup.c include/rtl_nonce.h
src/rtl_dhcp.o: include/rtl_dhcp.h
src/rtl_radio.o: include/rtl_radio.h src/rtl_radio_netbsd.inc
src/rtl_wire.o: include/rtl_wire.h
rtl8192cu-physical-protocol-test.elf: ee/physical_protocol_test.o tests/physical_flow.o src/wifi_app.o src/rtl_scan.o src/rtl_echo.o src/rtl_supplicant.o src/rtl_dhcp.o src/rtl_radio.o src/rtl_wire.o src/rtl_core.o build-crypto/rtl_wpa_crypto.o $(WPA_LIB_OBJS)
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) -lc

src/wifi_app.o: include/wifi_app.h include/rtl_scan.h
src/wifi_store.o: include/wifi_store.h include/wifi_app.h
src/rtl_echo.o: include/rtl_echo.h
ee/wifi_render.o: ee/wifi_render.h include/wifi_app.h include/wifi_version.h include/rtl_scan.h
ee/wifi.o: ee/wifi.c ee/probe.c ee/wifi_physical.h ee/wifi_render.h include/wifi_app.h include/wifi_store.h include/rtl_bringup.h
ee/wifi_preview.o: ee/wifi.c ee/probe.c ee/wifi_render.h include/wifi_app.h tests/echo_fixtures.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -DRTL_WIFI_UI_PREVIEW -c $< -o $@
rtl8192cu-wifi.elf: ee/wifi.o ee/wifi_render.o src/wifi_app.o src/wifi_store.o src/rtl_scan.o src/rtl_core.o src/rtl_supplicant.o src/rtl_dhcp.o src/rtl_echo.o build-crypto/rtl_nonce.o usbd_irx.o rtl_wifi_irx.o build-crypto/rtl_wpa_crypto.o $(WPA_LIB_OBJS)
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) -lpad -lmc -lfileXio $(EE_LIBS)
rtl8192cu-wifi.elf: ee/wifi_seed.o iomanx_irx.o filexio_irx.o bdm_irx.o fatfs_irx.o usbmass_irx.o
rtl8192cu-wifi.elf: src/wifi_known.o
src/wifi_known.o: include/wifi_known.h include/wifi_app.h
ee/wifi_seed.o: ee/wifi_seed.c ee/wifi_seed.h include/rtl_nonce.h
iomanx_irx.c: $(PS2SDK)/iop/irx/iomanX.irx
	bin2c $< $@ iomanx_irx
filexio_irx.c: $(PS2SDK)/iop/irx/fileXio.irx
	bin2c $< $@ filexio_irx
bdm_irx.c: $(PS2SDK)/iop/irx/bdm.irx
	bin2c $< $@ bdm_irx
fatfs_irx.c: $(PS2SDK)/iop/irx/bdmfs_fatfs.irx
	bin2c $< $@ fatfs_irx
usbmass_irx.c: $(PS2SDK)/iop/irx/usbmass_bd.irx
	bin2c $< $@ usbmass_irx
rtl8192cu-wifi-ui-preview.elf: ee/wifi_preview.o ee/wifi_render.o src/wifi_app.o src/rtl_echo.o src/rtl_scan.o src/rtl_core.o usbd_irx.o rtl_boot_irx.o build-crypto/rtl_wpa_crypto.o $(WPA_LIB_OBJS)
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) -lpad $(EE_LIBS)
tests/wifi_app_ee.o: tests/wifi_app.c include/wifi_app.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -Dmain=wifi_app_selftest -c $< -o $@
tests/echo_ee.o: tests/echo.c tests/echo_fixtures.h include/rtl_echo.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -Itests -Dmain=echo_packet_selftest -c $< -o $@
rtl8192cu-wifi-flow-test.elf: ee/wifi_app_test.o tests/wifi_app_ee.o tests/echo_ee.o src/wifi_app.o src/rtl_echo.o src/rtl_scan.o
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) -lc
.PHONY: wifi-tour
wifi-tour: reports/ui-tour/WIFI_1.ELF reports/ui-tour/WIFI_2.ELF reports/ui-tour/WIFI_3.ELF reports/ui-tour/WIFI_4.ELF
ee/wifi_tour_%.o: ee/wifi_ui_tour.c include/wifi_app.h ee/wifi_render.h tests/echo_fixtures.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -Iee -Itests -DWIFI_TOUR_SCREEN=$* -c $< -o $@
reports/ui-tour/WIFI_%.ELF: ee/wifi_tour_%.o ee/wifi_render.o src/wifi_app.o src/rtl_echo.o src/rtl_scan.o
	@mkdir -p reports/ui-tour
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) -ldebug -lc
ee/wifi_trace.o: ee/wifi.c ee/probe.c ee/wifi_render.h include/wifi_app.h tests/echo_fixtures.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -DRTL_WIFI_UI_PREVIEW -DRTL_WIFI_TRACE -c $< -o $@
reports/wifi-loop-trace.elf: ee/wifi_trace.o ee/wifi_render.o src/wifi_app.o src/rtl_echo.o src/rtl_scan.o src/rtl_core.o usbd_irx.o rtl_boot_irx.o build-crypto/rtl_wpa_crypto.o $(WPA_LIB_OBJS)
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) -lpad $(EE_LIBS)
ee/wifi_pad_test.o: ee/wifi.c ee/probe.c ee/wifi_render.h include/wifi_app.h tests/echo_fixtures.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -DRTL_WIFI_UI_PREVIEW -DRTL_WIFI_PAD_TEST -DRTL_WIFI_TRACE -c $< -o $@
reports/wifi-pad-test.elf: ee/wifi_pad_test.o ee/wifi_render.o src/wifi_app.o src/rtl_echo.o src/rtl_scan.o src/rtl_core.o usbd_irx.o rtl_boot_irx.o build-crypto/rtl_wpa_crypto.o $(WPA_LIB_OBJS)
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) -lpad $(EE_LIBS)
ee/wifi_pad_reject_test.o: ee/wifi.c ee/probe.c ee/wifi_render.h include/wifi_app.h tests/echo_fixtures.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -DRTL_WIFI_UI_PREVIEW -DRTL_WIFI_PAD_TEST -DRTL_WIFI_PAD_REJECT_TEST -DRTL_WIFI_TRACE -c $< -o $@
reports/wifi-pad-reject-test.elf: ee/wifi_pad_reject_test.o ee/wifi_render.o src/wifi_app.o src/rtl_echo.o src/rtl_scan.o src/rtl_core.o usbd_irx.o rtl_boot_irx.o build-crypto/rtl_wpa_crypto.o $(WPA_LIB_OBJS)
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) -lpad $(EE_LIBS)

ee/scan_parser_test.o: tests/scan.c include/rtl_scan.h
src/rtl_scan.o: include/rtl_scan.h
rtl8192cu-scan-parser-test.elf: ee/scan_parser_test.o src/rtl_scan.o
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) -lc

WPA_INCS = -Iinclude -Itests -Ithird_party/mbedtls/include -Ithird_party/mbedtls/library -DMBEDTLS_CONFIG_FILE='"rtl_mbedtls_config.h"'
build-crypto/rtl_nonce.o: src/rtl_nonce.c include/rtl_nonce.h
	@mkdir -p build-crypto
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) $(WPA_INCS) -c $< -o $@
iop/rtl8192cu_wifi.irx: iop/rtl_wifi.c iop/rtl_probe.c iop/rtl_wifi_impl.h src/rtl_radio.c src/rtl_radio_netbsd.inc src/rtl_wire.c src/rtl_bringup.c
	$(MAKE) -C iop -f Makefile.wifi
rtl_wifi_irx.c: iop/rtl8192cu_wifi.irx
	bin2c $< $@ rtl_probe_irx
ee/wifi.o: ee/wifi_physical.h ee/wifi_seed.h include/rtl_nonce.h include/rtl_wire.h include/rtl_supplicant.h include/rtl_dhcp.h include/rtl_tkip_guard.h
build-crypto/%.o: third_party/mbedtls/library/%.c include/rtl_mbedtls_config.h
	@mkdir -p build-crypto
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) $(WPA_INCS) -c $< -o $@
build-crypto/rtl_wpa_crypto.o: src/rtl_wpa_crypto.c include/rtl_wpa_crypto.h include/rtl_mbedtls_config.h
	@mkdir -p build-crypto
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) $(WPA_INCS) -c $< -o $@
ee/wpa_crypto_test.o: ee/wpa_crypto_test.c tests/wpa_crypto.c tests/wpa_fixtures.h include/rtl_wpa_crypto.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) $(WPA_INCS) -c $< -o $@
rtl8192cu-wpa-crypto-test.elf: ee/wpa_crypto_test.o build-crypto/rtl_wpa_crypto.o $(WPA_LIB_OBJS)
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) -ldebug -lc

iop/rtl8192cu_boot.irx: iop/rtl_boot.c iop/rtl_probe.c src/rtl_bringup.c include/rtl_bringup.h include/rtl_rpc.h iop/Makefile.boot third_party/realtek-firmware/rtl8192cfw.bin
	$(MAKE) -C iop -f Makefile.boot
rtl_boot_irx.c: iop/rtl8192cu_boot.irx
	bin2c $< $@ rtl_probe_irx
rtl8192cu-bringup.elf: ee/bringup.o src/rtl_core.o usbd_irx.o rtl_boot_irx.o
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) $(EE_LIBS)
ee/bringup.o: ee/probe.c include/rtl_bringup.h include/rtl_rpc.h
rtl8192cu-boot-model-test.elf: ee/boot_model_test.o src/rtl_core.o usbd_irx.o rtl_boot_irx.o
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) $(EE_LIBS)
ee/boot_model_test.o: ee/probe.c include/rtl_bringup.h include/rtl_rpc.h
iop/rtl8192cu_net.irx: iop/rtl_net.c iop/rtl_probe.c iop/rtl_net_impl.h src/rtl_net_packet.c include/rtl_rpc.h include/rtl_net_packet.h iop/Makefile.net
	$(MAKE) -C iop -f Makefile.net
rtl_net_irx.c: iop/rtl8192cu_net.irx
	bin2c $< $@ rtl_probe_irx
rtl8192cu-usbnet-test.elf: ee/usbnet_test.o src/rtl_core.o usbd_irx.o rtl_net_irx.o
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) $(EE_LIBS)
ee/usbnet_test.o: ee/probe.c include/rtl_rpc.h
iop/rtl8192cu_probe.irx: iop/rtl_probe.c src/rtl_core.c include/rtl_core.h include/rtl_rpc.h iop/imports.lst iop/irx_imports.h iop/Makefile
	$(MAKE) -C iop
third_party/ps2sdk-usbd/usbd.irx: $(wildcard third_party/ps2sdk-usbd/*.c third_party/ps2sdk-usbd/*.h third_party/ps2sdk-usbd/*.lst third_party/ps2sdk-usbd/*.tab) third_party/ps2sdk-usbd/Makefile
	$(MAKE) -C third_party/ps2sdk-usbd
usbd_irx.c: third_party/ps2sdk-usbd/usbd.irx
	bin2c $< $@ usbd_irx
rtl_probe_irx.c: iop/rtl8192cu_probe.irx
	bin2c $< $@ rtl_probe_irx
ee/probe.o: include/rtl_core.h include/rtl_rpc.h
src/rtl_core.o: include/rtl_core.h
tests/selftest.o ee/selftest_main.o: include/selftest.h include/rtl_core.h
rtl8192cu-selftest.elf: ee/selftest_main.o src/rtl_core.o tests/selftest.o
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $@ $^ $(EE_LDFLAGS) -ldebug -lc
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

rtl8192cu-physical-protocol-test.elf: tests/files.o
tests/files.o: tests/files.c include/wifi_files.h include/rtl_http.h

rtl8192cu-wifi.elf rtl8192cu-physical-protocol-test.elf reports/physical-join-template.elf: src/rtl_acd.o
rtl8192cu-wifi.elf rtl8192cu-physical-protocol-test.elf: src/wifi_controls.o
src/wifi_controls.o: include/wifi_controls.h include/wifi_app.h
src/rtl_acd.o: include/rtl_acd.h
src/wifi_known.o: include/wifi_files.h
ee/wifi.o: include/wifi_controls.h

rtl8192cu-physical-protocol-test.elf: tests/network_ui.o
tests/network_ui.o: tests/network_ui.c include/wifi_app.h include/rtl_acd.h include/wifi_controls.h

rtl8192cu-wifi.elf rtl8192cu-physical-protocol-test.elf reports/physical-join-template.elf: src/robot_program.o src/wifi_runtime.o
src/robot_program.o: include/robot_program.h
src/wifi_runtime.o: include/wifi_runtime.h include/robot_program.h include/robot_control.h
ee/wifi.o: ee/wifi_clock.h include/wifi_runtime.h include/robot_program.h
src/robot_control.o tests/robot.o tests/http.o: include/robot_control.h

rtl8192cu-physical-protocol-test.elf: tests/program.o
tests/program.o: tests/program.c include/robot_program.h include/rtl_dhcp.h tests/physical_fixtures.h

rtl8192cu-wifi.elf: poweroff_irx.o
rtl8192cu-wifi.elf: EE_LIBS += -lpoweroff
poweroff_irx.c: $(PS2SDK)/iop/irx/poweroff.irx
	bin2c $< $@ poweroff_irx
tests/files.o: include/wifi_runtime.h
ee/wifi.o ee/physical_join_test.o tests/physical_flow.o: include/rtl_wire.h
src/wifi_runtime.o: include/wifi_files.h
src/wifi_runtime.o: include/wifi_version.h
ee/wifi.o ee/wifi_preview.o ee/wifi_pad_test.o ee/wifi_pad_reject_test.o: include/wifi_video.h

ee/wifi.o: ee/background_job.h ee/device_inventory.h include/ps2_job_api.h include/usb_inventory.h
iop/rtl8192cu_wifi.irx: iop/usb_inventory_impl.h include/usb_inventory.h

src/rtl_http.o: include/ps2_tools_asset.h

rtl8192cu-wifi.elf: src/ps2_compile.o
src/ps2_compile.o: include/ps2_compile.h
ee/wifi.o: include/ps2_compile.h

$(wildcard ee/*.o src/*.o tests/*.o): include/rtl_scan.h

$(wildcard ee/*.o src/*.o tests/*.o): include/wifi_app.h

include/robot_web_asset.h: web/controls.html tools/build-web-assets.cjs
	node tools/build-web-assets.cjs
include/ps2_tools_asset.h: web/tools.html tools/build-web-assets.cjs
	node tools/build-web-assets.cjs

include/wifi_i18n_asset.h: web/i18n-runtime.js web/translations.json web/controls.html tools/build-web-assets.cjs
	node tools/build-web-assets.cjs
src/rtl_http.o: include/wifi_i18n_asset.h include/wifi_i18n.h
src/wifi_app.o src/wifi_runtime.o ee/wifi.o ee/wifi_render.o: include/wifi_i18n.h
include/wifi_i18n.h: web/translations.json tools/build-web-assets.cjs
	node tools/build-web-assets.cjs
