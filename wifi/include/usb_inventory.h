#ifndef USB_INVENTORY_H
#define USB_INVENTORY_H
#define RTL_RPC_DEVICE 10
#define RTL_RPC_HUB_POWER 11
#define RTL_RPC_USB_CHECK 12
#define USB_INVENTORY_MAX 32
#define USB_CONFIG_MAX 2048
/* Fixed-width, pointer-free EE/IOP RPC wire format. */
typedef struct {
 unsigned id,present,config_length,config_total,driver_flags;int location_result,string_result[3];
 unsigned char location[8],descriptor[20],config[USB_CONFIG_MAX];
 char manufacturer[96],product[96],serial[96];
 unsigned hub_length,hub_port_count;int hub_result,hub_port_result[8];
 unsigned char hub_descriptor[64],hub_ports[8][4];
 unsigned dma_padding[5];
} UsbInventoryDevice;
typedef char UsbInventory_rpc_cacheline_size[(sizeof(UsbInventoryDevice)%64)==0?1:-1];
#endif
