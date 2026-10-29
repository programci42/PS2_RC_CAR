/* SPDX-License-Identifier: MIT -- real production write transport, mock USBD. */
#define RTL_DRIVER_HOST_TEST
#define RTL_PHYSICAL_BOOT
#define _start rtl_module_start
#include "../iop/rtl_probe.c"
#include <assert.h>
unsigned char rtl_firmware_bin[33];unsigned int size_rtl_firmware_bin=33;
static UsbDeviceDescriptor fake_device;
static unsigned char config[]={9,2,39,0,1,1,0,0x80,250,9,4,0,0,3,255,255,255,0,
    7,5,129,2,64,0,0,7,5,2,2,64,0,0,7,5,3,2,64,0,0};
static int calls,pending,short_write,submit_error,completion_error,unplug;
static unsigned char captured[256];
static void (*callback)(int,int,void*);static void *cookie;static int count;
static void *sceUsbdScanStaticDescriptor(int id,void*p,int type){(void)id;(void)p;return type==1?(void*)&fake_device:config;}
static int sceUsbdOpenPipe(int id,void*p){(void)id;(void)p;return 5;}
static int sceUsbdClosePipe(int id){(void)id;return 0;}
static int sceUsbdControlTransfer(int ep,int type,int req,int val,int idx,int n,void*data,void(*cb)(int,int,void*),void*arg){
    (void)ep;(void)val;assert(idx==0);
    if(req==9){cb(0,0,arg);return 0;}
    calls++;assert(req==5&&n>0&&n<=196);
    if(submit_error)return submit_error;
    if(type==0x40)memcpy(captured,data,n);else{assert(type==0xc0&&n<=4);memset(data,0x12,n);}
    if(pending){callback=cb;cookie=arg;count=n;}
    else cb(completion_error,short_write?n-1:n,arg);
    return 0;
}
static void DelayThread(int us){(void)us;if(unplug){unplug=0;disconnect(device);}}
static void finish(void){assert(callback);callback(0,count,cookie);callback=0;}
static void reset(void){
    memset(&report,0,sizeof(report));report.magic=RTL_RPC_MAGIC;report.version=1;
    device=pipe=-1;config_pending=transfer_pending=0;boot_generation=0;
    calls=pending=short_write=submit_error=completion_error=unplug=0;callback=0;
    fake_device.idVendor=RTL_VID;fake_device.idProduct=RTL_PID;
    assert(connect(1)==0&&report.state==RTL_READY);
}
int main(void){
    unsigned char data[196],saved[196];RtlRequest q={0x40,5,0x1000,0,196};int i,cases=0;
    for(i=0;i<196;i++)data[i]=(unsigned char)i;
    memcpy(saved,data,196);
    reset();assert(control(0,&q,data)==196&&!memcmp(captured,data,196));cases++;
    q.value=0x30;q.length=4;assert(control(0,&q,data)==RTL_E_ARGUMENT&&calls==1);q.value=0xcf;q.length=1;assert(control(0,&q,data)==RTL_E_ARGUMENT&&calls==1);cases++;
    q.value=0x1fff;q.length=2;assert(control(0,&q,data)==RTL_E_ARGUMENT&&calls==1);q.value=0x1000;q.length=197;assert(control(0,&q,data)==RTL_E_ARGUMENT&&calls==1);q.length=196;cases++;
    reset();pending=1;assert(control(0,&q,data)==RTL_E_TIMEOUT&&transfer_pending);
    memset(data,0xaa,196);assert(!memcmp(transfer_buffer,saved,196));assert(control(0,&q,data)==RTL_E_BUSY&&calls==1);finish();pending=0;assert(control(0,&q,data)==196);cases++;
    reset();short_write=1;assert(control(0,&q,data)==RTL_E_SHORT&&!transfer_pending);cases++;
    reset();submit_error=7;assert(control(0,&q,data)==RTL_E_IO&&!transfer_pending);cases++;
    reset();completion_error=7;assert(control(0,&q,data)==RTL_E_IO&&!transfer_pending);cases++;
    reset();pending=1;unplug=1;assert(control(0,&q,data)==RTL_E_DISCONNECTED&&transfer_pending&&!probe(2));finish();pending=0;assert(probe(2)&&connect(2)==0);assert(control(0,&q,data)==196);cases++;
    reset();disconnect(1);assert(boot_run()->result==RTL_E_DISCONNECTED&&calls==0);cases++;
    printf("IOP boot write lifecycle scenarios=%d failures=0 (actual production transport, mock USB)\n",cases);return 0;
}
