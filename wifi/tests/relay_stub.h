#include <stdio.h>
#include <string.h>
#include <stdint.h>
typedef uint8_t u8;
#define IRX_ID(a,b,c)
#define USB_RC_OK 0
#define USB_DT_DEVICE 1
#define USB_DT_CONFIG 2
#define USB_DT_INTERFACE 4
#define USB_DT_ENDPOINT 5
#define USB_CLASS_HID 3
#define USB_ENDPOINT_XFERTYPE_MASK 3
#define USB_ENDPOINT_XFER_INT 3
#define USB_ENDPOINT_DIR_MASK 128
#define USB_DIR_OUT 0
#define USB_TYPE_CLASS 32
#define USB_RECIP_INTERFACE 1
#define USB_RECIP_DEVICE 0
#define USB_REQ_SET_REPORT 9
#define TH_C 0
#define MODULE_NO_RESIDENT_END 1
#define MODULE_RESIDENT_END 0
typedef struct {unsigned short idVendor,idProduct;} UsbDeviceDescriptor;
typedef struct {u8 bLength,bDescriptorType;unsigned short wTotalLength;u8 bNumInterfaces,bConfigurationValue,iConfiguration,bmAttributes,bMaxPower;} UsbConfigDescriptor;
typedef struct {u8 bLength,bDescriptorType,bInterfaceNumber,bAlternateSetting,bNumEndpoints,bInterfaceClass,bInterfaceSubClass,bInterfaceProtocol,iInterface;} UsbInterfaceDescriptor;
typedef struct {u8 bLength,bDescriptorType,bEndpointAddress,bmAttributes,wMaxPacketSizeLB,wMaxPacketSizeHB,bInterval;} UsbEndpointDescriptor;
typedef struct {void *next,*prev;char *name;int(*probe)(int),(*connect)(int),(*disconnect)(int);unsigned r1,r2,r3,r4,r5;void *gp;} sceUsbdLddOps;
typedef struct {int attr,option,initial,max;} iop_sema_t;
typedef struct {int attr;void(*thread)(void*);int priority,stacksize,option;} iop_thread_t;
typedef struct {int dummy;} SifRpcDataQueue_t;
typedef struct {int dummy;} SifRpcServerData_t;
static int CreateSema(iop_sema_t*s){(void)s;return 1;}
static int WaitSema(int s){(void)s;return 0;}
static int done_flag;
static int SignalSema(int s){(void)s;done_flag=1;return 0;}
static int PollSema(int s){(void)s;if(done_flag){done_flag=0;return 0;}return -1;}
static int CreateThread(iop_thread_t*t){(void)t;return 1;}
static int StartThread(int n,void*p){(void)n;(void)p;return 0;}
static int GetThreadId(void){return 1;}
static unsigned signal_waits;static void DelayThread(int n){if(n==100000)signal_waits++;}
static void sceSifInitRpc(int n){(void)n;}
static void sceSifSetRpcQueue(void*a,int n){(void)a;(void)n;}
static void sceSifRegisterRpc(void*a,int n,void*(*f)(int,void*,int),void*b,void*c,void*d,void*e){(void)a;(void)n;(void)f;(void)b;(void)c;(void)d;(void)e;}
static void sceSifRpcLoop(void*a){(void)a;}
static int sceUsbdSetPrivateData(int id,void*p){(void)id;(void)p;return 0;}
static void*sceUsbdScanStaticDescriptor(int,void*,int);
static int sceUsbdOpenPipe(int,void*);
static int sceUsbdClosePipe(int);
static int sceUsbdRegisterLdd(sceUsbdLddOps*);
static int sceUsbdUnregisterLdd(sceUsbdLddOps*);
static int sceUsbdControlTransfer(int,int,int,int,int,int,void*,void(*)(int,int,void*),void*);
static int sceUsbdInterruptTransfer(int,void*,int,void(*)(int,int,void*),void*);
#define sceUsbdSetConfiguration(ep,c,cb,arg) sceUsbdControlTransfer(ep,0,9,c,0,0,0,cb,arg)
