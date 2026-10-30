IOP_BIN = usbd.irx
.DEFAULT_GOAL := all
IOP_OBJS = hub.o mem.o timer.o endpoint.o io_request.o hub_resets.o td_queue.o hcd.o usbd_sys.o usbd_api.o usbd_main.o report_descriptor_init.o device_driver.o device.o imports.o exports.o
IOP_OBJS_DIR = obj/
$(addprefix $(IOP_OBJS_DIR),$(IOP_OBJS)): usbdpriv.h
$(IOP_OBJS_DIR)hub.o: hub_ports.h hub_runtime.h
$(IOP_OBJS_DIR)device.o: enum_start.h
all: $(IOP_BIN)
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
