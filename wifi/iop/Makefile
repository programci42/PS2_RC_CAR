IOP_BIN = rtl8192cu_probe.irx
IOP_OBJS = rtl_probe.o rtl_core.o imports.o
IOP_INCS = -I../include
all: $(IOP_BIN)
obj/rtl_probe.o: ../include/rtl_core.h ../include/rtl_rpc.h
obj/imports.o: irx_imports.h
obj/rtl_core.o: ../src/rtl_core.c ../include/rtl_core.h
	$(DIR_GUARD)
	$(IOP_CC) $(IOP_CFLAGS) $(IOP_INCS) -c $< -o $@
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
