/* SPDX-License-Identifier: MIT -- this ELF uses USBD, not SMAP/Ethernet APIs. */
#define main diagnostic_main
#include "probe.c"
#undef main
#include <stdio.h>
static RtlNetReport net_report __attribute__((aligned(64)));
int main(void){
 int i,tick,result;
 init_scr();scr_setCursor(0);scr_printf("VIRTUAL RTL8192CU USB NETWORK TEST\nNo Ethernet driver is loaded in this ELF.\n");
 SifInitRpc(0);SifLoadFileInit();SifInitIopHeap();sbv_patch_enable_lmb();sbv_patch_disable_prefix_check();
 if(load(usbd_irx,size_usbd_irx)<0||load(rtl_probe_irx,size_rtl_probe_irx)<0){printf("USB_NET_TEST FAIL module load\n");return 1;}
 memset(&client,0,sizeof(client));
 for(i=0;i<100;i++){
  if(SifBindRpc(&client,RTL_RPC_ID,SIF_RPC_M_NOWAIT)<0||wait_rpc()<0){printf("USB_NET_TEST FAIL RPC bind\n");return 1;}
  if(client.server)break;
  DelayThread(10000);
 }
 if(!client.server){printf("USB_NET_TEST FAIL no server\n");return 1;}
 /* Allow asynchronous SET_CONFIGURATION callback before the first request. */
 DelayThread(100000);
 for(i=0;i<5;i++){
  memset(&net_report,0,sizeof(net_report));
  result=SifCallRpc(&client,RTL_RPC_NET_DNS,SIF_RPC_M_NOWAIT,&net_report,sizeof(net_report),&net_report,sizeof(net_report),0,0);
  if(result<0){printf("USB_NET_TEST FAIL RPC start\n");return 1;}
  for(tick=0;tick<6000&&SifCheckStatRpc(&client);tick++)DelayThread(1000);
  if(tick==6000){printf("USB_NET_TEST FAIL RPC timeout\n");SleepThread();return 1;}
  printf("USB_DNS sequence=%u result=%d tx_usb=%u rx_usb=%u id=%04X answers=%u\n",net_report.sequence,net_report.result,net_report.tx_bytes,net_report.rx_bytes,net_report.dns_id,net_report.dns_answers);
  scr_printf("Round %d: result=%d TX=%u RX=%u answers=%u\n",i+1,net_report.result,net_report.tx_bytes,net_report.rx_bytes,net_report.dns_answers);
  if(net_report.magic!=RTL_NET_MAGIC||net_report.result){printf("USB_NET_TEST FAIL round=%d\n",i+1);return 1;}
 }
 printf("USB_NET_TEST PASS rounds=5 path=EE-IOP-USBD-RTL8192CU-Ethernet-DNS\n");
 scr_printf("USB data path passed. This is a restricted\nvirtual profile, not a physical WiFi test.\n");
 return 0;
}
