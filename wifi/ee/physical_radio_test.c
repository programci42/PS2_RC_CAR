/* Offline ELF harness for the REAL USB passthrough, no synthetic SSIDs. */
#define main diagnostic_main
#include "probe.c"
#undef main
#include "rtl_wire.h"
#include "rtl_scan.h"
#include <stdio.h>
static union {RtlReport usb;RtlBootReport boot;RtlRadioReport radio;RtlWire wire;} reply __attribute__((aligned(64)));
static int call(int cmd,int size){int tick;
 if(SifCallRpc(&client,cmd,SIF_RPC_M_NOWAIT,&reply,size,&reply,size,0,0)<0)return -1;
 for(tick=0;tick<120000&&SifCheckStatRpc(&client);tick++)DelayThread(1000);
 return tick==120000?-1:0;
}
int main(void){int i,j;RtlScanList list={0};RtlAccessPoint ap;char name[129];
 SifInitRpc(0);SifLoadFileInit();SifInitIopHeap();sbv_patch_enable_lmb();sbv_patch_disable_prefix_check();
 if(load(usbd_irx,size_usbd_irx)||load(rtl_probe_irx,size_rtl_probe_irx)){printf("PHYSICAL_RADIO_TEST FAIL modules\n");return 1;}
 for(i=0;i<100;i++){if(SifBindRpc(&client,RTL_RPC_ID,SIF_RPC_M_NOWAIT)<0||wait_rpc())return 1;if(client.server)break;DelayThread(10000);}
 if(!client.server)return 1;
 DelayThread(100000);
 if(call(RTL_RPC_BOOT,sizeof(RtlBootReport)))return 1;
 printf("PHYSICAL_BOOT result=%ld stage=%lu uploaded=%lu\n",(long)reply.boot.result,(unsigned long)reply.boot.stage,(unsigned long)reply.boot.uploaded_bytes);
 if(reply.boot.result)return 1;
 if(call(RTL_RPC_RADIO,sizeof(RtlRadioReport)))return 1;
 printf("PHYSICAL_RADIO result=%ld stage=%lu reg=%04lX ROM=%02X%02X\n",(long)reply.radio.result,(unsigned long)reply.radio.stage,(unsigned long)reply.radio.last_address,reply.radio.rom[1],reply.radio.rom[0]);
 if(reply.radio.result)return 1;
 for(i=1;i<=13;i++){
  memset(&reply,0,sizeof(reply));reply.wire.channel=i;
  if(call(RTL_RPC_CHANNEL,sizeof(RtlWire))||reply.wire.result)return 1;
  for(j=0;j<40;j++){
   if(call(RTL_RPC_RX,sizeof(RtlWire))||reply.wire.result)return 1;
   if(reply.wire.length&&!rtl_scan_parse(reply.wire.bytes,reply.wire.length,i,0,&ap))rtl_scan_add(&list,&ap);
   DelayThread(10000);
  }
  printf("PHYSICAL_SCAN channel=%d networks=%u\n",i,list.count);
 }
 for(i=0;i<(int)list.count;i++){rtl_scan_label(&list.ap[i],name,sizeof(name));printf("PHYSICAL_SSID channel=%u security=%s SSID=%s\n",list.ap[i].channel,rtl_scan_security_label(&list.ap[i]),name);}
 if(!list.count){printf("PHYSICAL_RADIO_TEST FAIL no-beacons\n");return 1;}
 printf("PHYSICAL_RADIO_TEST PASS real-USB real-RF networks=%u\n",list.count);return 0;
}
