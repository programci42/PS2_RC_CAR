EE_BIN = relay.elf
EE_OBJS = main.o usbd_irx.o seda_relay_irx.o
EE_LIBS = -lpad -lmc -ldebug -lpatches -lc

USBD_IRX = $(PS2SDK)/iop/irx/usbd.irx
SEDA_IRX = iop/seda_relay.irx

all: $(EE_BIN)

iop/seda_relay.irx:
	$(MAKE) -C iop

usbd_irx.c: $(USBD_IRX)
	bin2c $(USBD_IRX) $@ usbd_irx

seda_relay_irx.c: $(SEDA_IRX)
	bin2c $(SEDA_IRX) $@ seda_relay_irx

clean:
	$(MAKE) -C iop clean
	rm -f $(EE_BIN) $(EE_OBJS) usbd_irx.c seda_relay_irx.c

run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
