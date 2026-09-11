IOP_BIN = seda_relay.irx
IOP_OBJS = seda_relay_irx.o imports.o
IOP_INCS = -I..

all: $(IOP_BIN)

clean:
	rm -rf obj $(IOP_BIN) $(IOP_BIN:.irx=.notiopmod.elf) $(IOP_BIN:.irx=.notiopmod.stripped.elf)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
