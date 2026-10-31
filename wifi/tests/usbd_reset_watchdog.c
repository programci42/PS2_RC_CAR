#include "usbd_watchdog_stub.h"
#include <assert.h>
FakePool pool,*memPool=&pool;static FakeOhci ohci;
static UsbdDevice_t root,hub,bad,wifi;static unsigned resets,polls,addresses,called;
void hubResetDevicePort(UsbdDevice_t*d){(void)d;resets++;}
void hubRestartStatusPoll(UsbdDevice_t*d){(void)d;polls++;}
void*openDeviceEndpoint(UsbdDevice_t*d,void*p,unsigned a){(void)d;(void)p;(void)a;return &root;}
int hubTimedSetFuncAddress(UsbdDevice_t*d){(void)d;addresses++;return 0;}
void flushPort(UsbdDevice_t*d){if(d->m_timer.m_isActive)cancelTimerCallback(&d->m_timer);d->m_deviceStatus=DEVICE_NOTCONNECTED;if(d->m_resetFlag)checkDelayedResets(d);}
#define USBD_RESET_HOST_TEST
#include "../third_party/ps2sdk-usbd/timer.c"
#include "../third_party/ps2sdk-usbd/hub_resets.c"
#include "../third_party/ps2sdk-usbd/hub_ports.h"
static void callback(void*a){(void)a;called++;}
static void ticks(unsigned n){while(n--)handleTimerList();}
static void reset(void){memset(&pool,0,sizeof(pool));memset(&ohci,0,sizeof(ohci));memset(&root,0,sizeof(root));memset(&hub,0,sizeof(hub));memset(&bad,0,sizeof(bad));memset(&wifi,0,sizeof(wifi));pool.m_ohciRegs=&ohci;pool.m_deviceTreeRoot=&root;resets=polls=addresses=called=0;}
int main(void){UsbdTimerCbStruct_t a={0},b={0};UsbdDevice_t ports[8];unsigned i,rotation,port;reset();
 memset(ports,0,sizeof(ports));hub.m_childListStart=ports;
 for(rotation=0;rotation<8;rotation++){
  for(i=0;i<8;i++){ports[i].m_attachedToPortNo=(i+rotation)%8+1;ports[i].m_next=i<7?&ports[i+1]:0;}
  for(port=1;port<=8;port++)assert(hubPortDevice(&hub,port)==&ports[(port-1+8-rotation)%8]);
 }
 assert(!hubPortDevice(&hub,0)&&!hubPortDevice(&hub,9)&&!hubPortDevice(&hub,-1));
 assert(hubStatusBytes(4)==1&&hubStatusBytes(7)==1&&hubStatusBytes(8)==2&&hubStatusBytes(63)==8);
 reset();
 addTimerCallback(&a,callback,0,1001);addTimerCallback(&b,callback,0,2001);ticks(100);cancelTimerCallback(&a);ticks(1899);assert(!called);ticks(1);assert(called==1);
 reset();hub.m_parent=&root;hub.m_deviceStatus=DEVICE_READY;hub.m_attachedToPortNo=1;root.m_childListStart=&hub;hub.m_next=&wifi;hub.m_childListStart=&bad;bad.m_parent=&hub;bad.m_deviceStatus=DEVICE_CONNECTED;bad.m_attachedToPortNo=2;wifi.m_parent=&root;wifi.m_deviceStatus=DEVICE_RESETDELAYED;wifi.m_attachedToPortNo=2;
 hubResetDevice(&bad);assert(pool.m_delayResets&&bad.m_resetFlag&&bad.m_deviceStatus==DEVICE_RESETPENDING&&resets==1);
 ticks(2000);assert(bad.m_deviceStatus==DEVICE_CONNECTED&&!bad.m_resetFlag&&polls==1&&bad.m_timer.m_isActive);assert(wifi.m_deviceStatus==DEVICE_RESETPENDING&&wifi.m_resetFlag&&pool.m_delayResets);
 ohci.HcRhPortStatus[0]=BIT(PORT_CONNECTION)|BIT(PORT_ENABLE)|256;ohci.HcRhPortStatus[1]=BIT(PORT_CONNECTION)|BIT(PORT_ENABLE)|256;handleRhsc();assert(wifi.m_deviceStatus==DEVICE_RESETCOMPLETE&&!wifi.m_timer.m_isActive&&addresses==1);checkDelayedResets(&wifi);wifi.m_deviceStatus=DEVICE_READY;
 ticks(999);assert(resets==1);ticks(1);assert(resets==2&&bad.m_deviceStatus==DEVICE_RESETPENDING);
 flushPort(&bad);bad.m_parent=0;ticks(3000);assert(resets==2&&!pool.m_delayResets&&!bad.m_timer.m_isActive);
 /* Observed live: connected root port, ENABLE lost, stale READY LDD.
    Re-enumerate only that port and leave the healthy Wi-Fi root untouched. */
 reset();root.m_childListStart=&bad;bad.m_parent=&root;bad.m_next=&wifi;bad.m_attachedToPortNo=1;bad.m_deviceStatus=DEVICE_READY;
 wifi.m_parent=&root;wifi.m_attachedToPortNo=2;wifi.m_deviceStatus=DEVICE_READY;
 ohci.HcRhPortStatus[0]=257;ohci.HcRhPortStatus[1]=259;handleRhsc();
 assert(bad.m_deviceStatus==DEVICE_CONNECTED&&bad.m_timer.m_isActive&&wifi.m_deviceStatus==DEVICE_READY&&!wifi.m_timer.m_isActive);
 ticks(500);assert(bad.m_deviceStatus==DEVICE_RESETPENDING&&pool.m_delayResets);
 ohci.HcRhPortStatus[0]=257;ohci.HcRhPortStatus[1]=259;handleRhsc();assert(!addresses&&bad.m_deviceStatus==DEVICE_RESETPENDING);
 ohci.HcRhPortStatus[0]=259;ohci.HcRhPortStatus[1]=259;handleRhsc();assert(addresses==1&&bad.m_deviceStatus==DEVICE_RESETCOMPLETE);
 checkDelayedResets(&bad);bad.m_deviceStatus=DEVICE_READY;
 for(i=0;i<2;i++){ohci.HcRhPortStatus[0]=257|BIT(i?PORT_RESET:PORT_OVER_CURRENT);ohci.HcRhPortStatus[1]=259;handleRhsc();assert(bad.m_deviceStatus==DEVICE_READY&&!bad.m_timer.m_isActive);}
 printf("USBD_RESET_WATCHDOG_TEST PASS actual vendor timer/reset code: root-disabled recovery, healthy peer preserved, no addressing disabled root, reset/overcurrent guards, 8 ports x 8 enumeration orders, timer watchdog\n");return 0;}
