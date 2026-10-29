#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "usb_inventory.h"
typedef struct {unsigned char raw[18];} UsbDeviceDescriptor;
typedef struct {unsigned char a,b;unsigned short wTotalLength;unsigned char raw[2044];} UsbConfigDescriptor;
#define USB_DT_DEVICE 1
#define USB_DT_CONFIG 2
static UsbDeviceDescriptor dev;static UsbConfigDescriptor config;static unsigned fake_id=3;
static int time_out,late_count,requests,last_index;static unsigned powered=15,overcurrent,power_writes,configuration=1,config_writes;static void(*late)(int,int,void*);
static void*sceUsbdScanStaticDescriptor(int id,void*p,int type){(void)p;if((unsigned)id!=fake_id)return 0;return type==1?(void*)&dev:&config;}
static int sceUsbdGetDeviceLocation(int id,unsigned char*p){(void)id;p[0]=2;p[1]=3;return 0;}
static int sceUsbdOpenPipe(int id,void*d){(void)d;return id;}
static int sceUsbdClosePipe(int id){(void)id;return 0;}
static void DelayThread(int n){(void)n;}
static int sceUsbdControlTransfer(int ep,int type,int req,int value,int index,int len,void*data,void(*cb)(int,int,void*),void*a){unsigned char*p=data;int n=0;(void)ep;(void)type;(void)index;(void)len;requests++;last_index=index;
 if(time_out){late=cb;late_count=4;return 0;}
 if(req==8){p[0]=configuration;n=1;}
 else if(req==9&&type==0){configuration=value;config_writes++;n=0;}
 else if(req==3&&type==0x23&&value==8){assert(index>=1&&index<=4);powered|=1u<<(index-1);power_writes++;n=0;}
 else if(req==0){p[0]=3|((overcurrent&(1u<<(index-1)))?8:0);p[1]=(powered&(1u<<(index-1)))?1:0;p[2]=p[3]=0;n=4;}
 else if(value==0x2900){p[0]=9;p[1]=0x29;p[2]=4;n=9;}
 else if((value&255)==0){p[0]=4;p[1]=3;p[2]=9;p[3]=4;n=4;}
 else{p[0]=8;p[1]=3;p[2]='P';p[3]=0;p[4]='S';p[5]=0;p[6]='2';p[7]=0;n=8;}
 cb(0,n,a);return 0;}
#include "../iop/usb_inventory_impl.h"
int main(void){UsbInventoryDevice r;unsigned cases=0;memset(&dev,0,sizeof(dev));dev.raw[0]=18;dev.raw[1]=1;dev.raw[4]=9;dev.raw[14]=dev.raw[15]=dev.raw[16]=1;config.wTotalLength=2048;
 memset(&r,0,sizeof(r));r.id=3;inventory_device(&r);assert(r.present&&r.config_length==2048&&!strcmp(r.product,"PS2")&&r.location[1]==3);cases++;
 assert(r.hub_port_count==4&&r.hub_ports[3][0]==3&&last_index==4);cases++;
 r.id=4;inventory_device(&r);assert(!r.present&&!r.product[0]);cases++;
 r.id=3;time_out=1;inventory_device(&r);assert(r.string_result[0]==-1001&&inventory_pending);cases++;
 {int before=requests;r.id=3;inventory_device(&r);assert(requests==before);cases++;}
 late(0,late_count,0);time_out=0;r.id=3;inventory_device(&r);assert(!inventory_pending&&!strcmp(r.product,"PS2"));cases++;
 dev.raw[14]=dev.raw[15]=dev.raw[16]=0;r.id=3;inventory_device(&r);assert(r.string_result[0]==1&&!r.manufacturer[0]);cases++;
 configuration=0;((unsigned char*)&config)[5]=1;powered=0;r.id=1;r.driver_flags=0x80000000u;inventory_hub_power(&r);assert(r.id==3&&r.present&&r.driver_flags==15&&power_writes==4&&config_writes==1&&configuration==1);cases++;
 r.id=1;inventory_hub_power(&r);assert(!r.driver_flags&&power_writes==4);cases++;powered=0;overcurrent=15;r.id=1;inventory_hub_power(&r);assert(!r.driver_flags&&power_writes==4);cases++;
 {int before=requests;dev.raw[8]=0xda;dev.raw[9]=0x0b;dev.raw[10]=0x78;dev.raw[11]=0x81;
  inventory_presence(&r,0x0bda,0x8178);assert(r.present&&r.id==3&&r.config_total==1&&requests==before);cases++;
  fake_id=7;inventory_presence(&r,0x0bda,0x8178);assert(r.present&&r.id==7&&requests==before);cases++;
  fake_id=0;inventory_presence(&r,0x0bda,0x8178);assert(!r.present&&!r.config_total&&requests==before);cases++;}
 printf("USB_INVENTORY_TEST PASS cases=%u actual-production-reader descriptor-strings-hub-ports-unplug-timeout-DMA-ownership-presence-any-id-no-control-transfer\n",cases);return 0;}
