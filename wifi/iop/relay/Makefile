IOP_BIN = seda_relay.irx
IOP_OBJS = seda_relay_irx.o imports.o
IOP_INCS = -I../../include
all: $(IOP_BIN)
obj/imports.o: irx_imports.h ../irx_imports.h
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
