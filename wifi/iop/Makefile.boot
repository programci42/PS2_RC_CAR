IOP_BIN = rtl8192cu_boot.irx
IOP_OBJS = rtl_boot.o rtl_core.o rtl_bringup.o rtl_firmware.o imports.o
IOP_OBJS_DIR = obj-boot/
IOP_INCS = -I../include
all: $(IOP_BIN)
obj-boot/rtl_boot.o: rtl_probe.c ../include/rtl_rpc.h ../include/rtl_core.h ../include/rtl_bringup.h
obj-boot/imports.o: imports.lst irx_imports.h
rtl_firmware.c: ../third_party/realtek-firmware/rtl8192cfw.bin
	bin2c $< $@ rtl_firmware_bin
obj-boot/rtl_firmware.o: rtl_firmware.c
obj-boot/rtl_core.o: ../src/rtl_core.c ../include/rtl_core.h
	$(DIR_GUARD)
	$(IOP_CC) $(IOP_CFLAGS) $(IOP_INCS) -c $< -o $@
obj-boot/rtl_bringup.o: ../src/rtl_bringup.c ../include/rtl_bringup.h
	$(DIR_GUARD)
	$(IOP_CC) $(IOP_CFLAGS) $(IOP_INCS) -c $< -o $@
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
