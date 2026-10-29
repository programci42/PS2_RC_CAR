/* Host USB/scheduler stubs for testing the unmodified production callback code. */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define IRX_ID(a,b,c)
#define USB_DT_DEVICE 1
#define USB_DT_CONFIG 2
#define USB_RC_OK 0
#define TH_C 0
#define MODULE_NO_RESIDENT_END 1
#define MODULE_RESIDENT_END 0
typedef struct {unsigned short idVendor,idProduct;} UsbDeviceDescriptor;
typedef struct {unsigned char bLength,bDescriptorType;unsigned short wTotalLength;} UsbConfigDescriptor;
typedef struct {int dummy;} SifRpcDataQueue_t;
typedef struct {int dummy;} SifRpcServerData_t;
typedef struct {int attr;void (*thread)(void*);int priority,stacksize;} iop_thread_t;
typedef struct {void *next,*prev;char *name;int(*probe)(int),(*connect)(int),(*disconnect)(int);unsigned int r1,r2,r3,r4,r5;void *gp;} sceUsbdLddOps;
static void *sceUsbdScanStaticDescriptor(int,void*,int);
static int sceUsbdOpenPipe(int,void*);
static int sceUsbdClosePipe(int);
static int sceUsbdControlTransfer(int,int,int,int,int,int,void*,void(*)(int,int,void*),void*);
#define sceUsbdSetConfiguration(ep,c,cb,arg) sceUsbdControlTransfer(ep,0,9,c,0,0,0,cb,arg)
static void DelayThread(int);
static int CpuSuspendIntr(int *old){*old=0;return 0;}
static int CpuResumeIntr(int old){(void)old;return 0;}
static void sceSifInitRpc(int x){(void)x;}
static int GetThreadId(void){return 1;}
static void sceSifSetRpcQueue(void*a,int b){(void)a;(void)b;}
static void sceSifRegisterRpc(void*a,int b,void*(*c)(int,void*,int),void*d,void*e,void*f,void*g){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;}
static void sceSifRpcLoop(void*a){(void)a;}
static int CreateThread(iop_thread_t*t){(void)t;return 1;}
static int DeleteThread(int n){(void)n;return 0;}
static int StartThread(int n,void*a){(void)n;(void)a;return 0;}
static int register_calls,unregister_calls,reattach_id;
static int sceUsbdRegisterLdd(sceUsbdLddOps*d){register_calls++;if(reattach_id&&d->probe(reattach_id))d->connect(reattach_id);return 0;}
static int sceUsbdUnregisterLdd(sceUsbdLddOps*d){(void)d;unregister_calls++;return 0;}
