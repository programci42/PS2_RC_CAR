/* Production recovery helper and actual USBD timer, with no USB hardware. */
#include "usbd_watchdog_stub.h"
#include <assert.h>
#define USB_RC_OK 0
#define bzero(p,n) memset(p,0,n)
typedef struct {unsigned m_busyFlag,m_resultCode;} TestRequest;
typedef struct {UsbdDevice_t*m_dev;TestRequest m_controlIoReq,m_statusIoReq;unsigned m_numChildDevices,m_hubStatusCounter,m_pad3[6],m_portStatusChange;unsigned char m_statusChangeInfo[8];UsbdTimerCbStruct_t m_runtimeRetryTimer;unsigned m_runtimeRetryCount;} UsbdUsbHub_t;
FakePool pool,*memPool=&pool;static FakeOhci ohci;
static unsigned polls,flushed,resets;
static void getHubStatusChange(UsbdUsbHub_t*h){assert(h->m_pad3[0]==1&&!h->m_hubStatusCounter&&!h->m_statusIoReq.m_resultCode);polls++;}
void hubResetDevice(UsbdDevice_t*d){(void)d;resets++;}
void flushPort(UsbdDevice_t*d){if(d->m_timer.m_isActive)cancelTimerCallback(&d->m_timer);d->m_deviceStatus=DEVICE_NOTCONNECTED;flushed++;}
#define USBD_RESET_HOST_TEST
#include "../third_party/ps2sdk-usbd/timer.c"
#include "../third_party/ps2sdk-usbd/hub_runtime.h"
static void ticks(unsigned n){while(n--)handleTimerList();}
int main(void){UsbdDevice_t owner={0},port={0};UsbdUsbHub_t h={0};unsigned i;
 pool.m_ohciRegs=&ohci;h.m_dev=&owner;owner.m_privDataField=&h;h.m_numChildDevices=4;
 h.m_statusIoReq.m_resultCode=5;memset(h.m_statusChangeInfo,255,8);
 hubRuntimeError(&h);assert(h.m_runtimeRetryTimer.m_isActive&&h.m_runtimeRetryCount==1);
 hubRuntimeError(&h);assert(h.m_runtimeRetryCount==1);ticks(250);assert(polls==1);
 for(i=0;i<8;i++)assert(!h.m_statusChangeInfo[i]);
 h.m_controlIoReq.m_busyFlag=1;hubRuntimeError(&h);ticks(500);assert(polls==1&&h.m_runtimeRetryTimer.m_isActive&&h.m_runtimeRetryCount==3);
 h.m_controlIoReq.m_busyFlag=0;ticks(1000);assert(polls==2);
 for(i=0;i<10;i++){hubRuntimeError(&h);ticks(4000);}assert(h.m_runtimeRetryCount==4&&polls==12);
 port.m_deviceStatus=DEVICE_READY;h.m_portStatusChange=BIT(PORT_CONNECTION);
 assert(hubRuntimePortDisabled(&h,&port)&&flushed==1&&port.m_deviceStatus==DEVICE_CONNECTED);ticks(501);assert(resets==1);
 port.m_deviceStatus=DEVICE_READY;h.m_portStatusChange|=BIT(PORT_OVER_CURRENT);assert(!hubRuntimePortDisabled(&h,&port)&&flushed==1);
 h.m_portStatusChange=BIT(PORT_CONNECTION)|BIT(PORT_ENABLE);assert(!hubRuntimePortDisabled(&h,&port));
 h.m_portStatusChange=BIT(PORT_CONNECTION)|BIT(PORT_RESET);assert(!hubRuntimePortDisabled(&h,&port));
 h.m_portStatusChange=0;assert(!hubRuntimePortDisabled(&h,&port));
 h.m_portStatusChange=BIT(PORT_CONNECTION);port.m_deviceStatus=DEVICE_RESETPENDING;assert(!hubRuntimePortDisabled(&h,&port));
 hubRuntimeError(&h);owner.m_privDataField=0;ticks(4000);assert(polls==12);
 owner.m_privDataField=&h;hubRuntimeError(&h);h.m_statusIoReq.m_busyFlag=1;hubRuntimeHealthy(&h);assert(!h.m_runtimeRetryTimer.m_isActive&&!h.m_runtimeRetryCount&&h.m_statusIoReq.m_busyFlag);h.m_dev=0;ticks(4000);assert(polls==12);
 puts("USBD_HUB_RUNTIME_TEST PASS actual recovery/timer code: failed-status restarts full port scan, backoff capped, busy DMA retained, disabled child reset, overcurrent/reset-in-progress protected, stale/unplug callbacks inert");return 0;}
