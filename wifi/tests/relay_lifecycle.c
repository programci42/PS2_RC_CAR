#define SEDA_DRIVER_HOST_TEST
#define _start relay_module_start
#include "../iop/relay/seda_relay_irx.c"
#include <assert.h>
static UsbDeviceDescriptor dev={0x178e,0x07d9};
static unsigned char config[]={9,2,25,0,1,1,0,128,50,9,4,0,0,1,3,0,0,0,7,5,1,3,8,0,1};
static unsigned trace[32],trace_n;
static int attached,registrations,unregistrations,timeout_mode,submit_fail,transfers;
static void(*late)(int,int,void*);static void*cookie;
static void*sceUsbdScanStaticDescriptor(int id,void*p,int type){(void)p;if(!attached||id!=1)return 0;return type==1?(void*)&dev:config;}
static int sceUsbdOpenPipe(int id,void*p){(void)p;return id;}
static int sceUsbdClosePipe(int id){(void)id;return 0;}
static int sceUsbdRegisterLdd(sceUsbdLddOps*d){registrations++;if(attached&&d->probe(1))d->connect(1);return 0;}
static int sceUsbdUnregisterLdd(sceUsbdLddOps*d){(void)d;unregistrations++;return 0;}
static int sceUsbdControlTransfer(int ep,int type,int req,int value,int index,int n,void*p,void(*cb)(int,int,void*),void*a){(void)ep;(void)type;(void)req;(void)value;(void)index;(void)p;if(submit_fail)return -7;if(n){if(trace_n<32&&p)trace[trace_n++]=((u8*)p)[3];transfers++;if(timeout_mode){late=cb;cookie=a;return 0;}}cb(0,n,a);return 0;}
static int sceUsbdInterruptTransfer(int ep,void*p,int n,void(*cb)(int,int,void*),void*a){return sceUsbdControlTransfer(ep,0,0,0,0,n,p,cb,a);}
int main(void){SedaRelayRpcPacket p;unsigned tests=0;u8 owned[65];void*oldcookie;void(*oldcb)(int,int,void*);int before;
 assert(relay_module_start(0,0)==0&&relay_dev.dev_id==-1);tests++;
 memset(&p,0,sizeof(p));rpc_handler(4,&p,sizeof(p));assert(!p.result&&unregistrations==1);tests++;
 attached=1;rpc_handler(4,&p,sizeof(p));assert(relay_dev.status==2&&relay_dev.dev_id==1);tests++;
 p.mode=9;trace_n=0;rpc_handler(5,&p,sizeof(p));assert(p.result&&!live_mask&&trace_n==0);tests++;
 p.command=1;p.mode=9;rpc_handler(1,&p,sizeof(p));assert(!p.result&&live_mask==1);before=unregistrations;rpc_handler(4,&p,sizeof(p));assert(p.result==-1&&unregistrations==before);tests++;
 p.command=17;rpc_handler(1,&p,sizeof(p));assert(!live_mask);tests++;
 timeout_mode=1;p.command=2;rpc_handler(1,&p,sizeof(p));assert(p.result&&transfer_pending);memcpy(owned,relay_dev.report,65);oldcookie=cookie;oldcb=late;before=transfers;
 p.command=3;rpc_handler(1,&p,sizeof(p));assert(p.result&&transfers==before&&!memcmp(owned,relay_dev.report,65));tests++;
 seda_disconnect(1);assert(!memcmp(owned,relay_dev.report,65)&&!seda_probe(1));tests++;
 timeout_mode=0;rpc_handler(4,&p,sizeof(p));assert(relay_dev.status==2&&!transfer_pending);oldcb(0,8,oldcookie);assert(relay_dev.status==2&&!transfer_pending);watchdog_step();assert(!live_mask&&!relay_fault);tests++;
 seda_disconnect(1);submit_fail=1;rpc_handler(4,&p,sizeof(p));assert(relay_dev.status==1&&!configuration_pending);tests++;
 submit_fail=0;rpc_handler(4,&p,sizeof(p));assert(relay_dev.status==2);tests++;
 seda_disconnect(1);attached=0;rebinds=missing_polls=0;before=unregistrations;
 for(int i=0;i<300;i++){rpc_handler(2,&p,sizeof(p));}assert(rebinds==3&&unregistrations==before+3);tests++;
 attached=1;rpc_handler(4,&p,sizeof(p));p.command=7;p.mode=9;rpc_handler(1,&p,sizeof(p));assert(!p.result&&live_mask==64);p.command=8;before=transfers;rpc_handler(1,&p,sizeof(p));assert(p.result&&transfers==before&&live_mask==64);tests++;
 submit_fail=1;p.command=23;rpc_handler(1,&p,sizeof(p));assert(p.result&&live_mask==64&&relay_fault);rpc_handler(2,&p,sizeof(p));assert(p.status==1&&watchdog_ticks==0);tests++;
 submit_fail=0;watchdog_step();assert(!live_mask&&!relay_fault);tests++;
 p.command=8;rpc_handler(1,&p,sizeof(p));assert(live_mask==128);for(int i=0;i<20;i++)watchdog_step();assert(!live_mask);tests++;
 printf("RELAY_LIFECYCLE_TEST PASS %u cases actual-IOP-driver rebind-no-wifi-reset outputs-on-blocked timeout-buffer-owned stale-callback-rejected config-retry failed-OFF-keeps-mask status-polls-cannot-defeat-fault-watchdog IOP-opposite-interlock no-startup-pulse\n",tests);return 0;}
