/* Real USB/RF harness. Windows supplies a fresh private PMK/nonce in a
 * disposable test ELF. No credential is stored in source or Release. */
#define main diagnostic_main
#include "probe.c"
#undef main
#include "rtl_wire.h"
#include "wifi_app.h"
#include "rtl_wpa_crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <timer.h>
volatile unsigned char physical_private_input[128]="RTL_PHYSICAL_JOIN_PRIVATE_SLOT";
static WifiApp app;
static void wipe(void*p,size_t n){volatile uint8_t*v=p;while(n--)*v++=0;}
static int private_nonce(const char*path,uint8_t n[32]){
    static uint32_t count;uint8_t seed[32],ctr[32]={0},ptk[64],mac[6]={0};unsigned i;int e;(void)path;
    for(i=0;i<32;i++)seed[i]=physical_private_input[48+i];
    count++;for(i=0;i<4;i++)ctr[i]=count>>(8*i);
    e=rtl_wpa_ptk(seed,mac,mac,ctr,ctr,ptk);if(!e)memcpy(n,ptk,32);
    wipe(seed,32);wipe(ptk,64);return e;
}
#define rtl_nonce_next private_nonce
#define RTL_WIFI_PACKET_TRACE
#include "wifi_physical.h"
#undef rtl_nonce_next
#include "robot_usb.h"
static union {RtlReport usb;RtlBootReport boot;RtlRadioReport radio;RtlWire wire;} response __attribute__((aligned(64)));
static uint32_t now_ms(void){return (uint32_t)(GetTimerSystemTime()/147456u);}
static int call(int cmd,int size){int tick;
 if(SifCallRpc(&client,cmd,SIF_RPC_M_NOWAIT,&response,size,&response,size,0,0)<0)return -1;
 for(tick=0;tick<120000&&SifCheckStatRpc(&client);tick++)DelayThread(1000);
 return tick==120000?-1:0;
}
int main(void){unsigned i,last=999,selected=0,online=0;uint32_t now,start,online_at=0;uint8_t pmk[32];
 if(!physical_private_input[80]||physical_private_input[80]>32){printf("PHYSICAL_JOIN_TEST FAIL private-input-missing\n");return 1;}
 SifInitRpc(0);SifLoadFileInit();SifInitIopHeap();sbv_patch_enable_lmb();sbv_patch_disable_prefix_check();
 if(load(usbd_irx,size_usbd_irx)||load(rtl_probe_irx,size_rtl_probe_irx))return 1;
 for(i=0;i<100;i++){if(SifBindRpc(&client,RTL_RPC_ID,SIF_RPC_M_NOWAIT)<0||wait_rpc())return 1;if(client.server)break;DelayThread(10000);}
 if(!client.server)return 1;
 robot_usb_init(0);
 DelayThread(100000);
 if(call(RTL_RPC_BOOT,sizeof(RtlBootReport))||response.boot.result){printf("PHYSICAL_JOIN_TEST FAIL boot=%ld\n",(long)response.boot.result);return 1;}
 if(call(RTL_RPC_RADIO,sizeof(RtlRadioReport))||response.radio.result){printf("PHYSICAL_JOIN_TEST FAIL radio=%ld\n",(long)response.radio.result);return 1;}
 memcpy(physical_mac,response.radio.mac,6);physical_radio_ready=1;
 wifi_app_init(&app);wifi_app_driver(&app,0);wifi_app_take_actions(&app);start=now_ms();physical_scan_start(start);
 printf("PHYSICAL_JOIN using actual EE backend; MC provision excluded from this test\n");
 while((now=now_ms()),physical_private_input[113]==2||(uint32_t)(now-start)<(physical_private_input[113]?150000u:55000u)){
  if(app.state==WIFI_LIST){
   if(selected){printf("PHYSICAL_JOIN_TEST FAIL %s\n",app.message);return 1;}
   for(i=0;i<app.scan.count;i++)if(app.scan.ap[i].ssid_length==physical_private_input[80]){
    unsigned j;int equal=1;for(j=0;j<app.scan.ap[i].ssid_length;j++)if(app.scan.ap[i].ssid[j]!=physical_private_input[81+j])equal=0;
    if(equal)break;
   }
   if(i==app.scan.count){printf("PHYSICAL_JOIN_TEST FAIL target-not-in-scan\n");return 1;}
   selected=1;app.selected=i;wifi_app_input(&app,WIFI_X,now);wifi_app_input(&app,0,now+1);
   if(app.state!=WIFI_PASSWORD){printf("PHYSICAL_JOIN_TEST FAIL unsupported-target\n");return 1;}
   for(i=0;i<32;i++)pmk[i]=physical_private_input[16+i];
   app.state=WIFI_DERIVING;wifi_app_key(&app,app.ticket,pmk,0);wipe(pmk,32);wifi_app_take_actions(&app);physical_join_start(now);
  }
  robot_usb_tick(0,0,app.state==WIFI_ONLINE,now);physical_tick(now);
  if(last!=physical_phase){last=physical_phase;printf("PHYSICAL_JOIN phase=%u %s\n",last,app.message);}
  if(app.state==WIFI_ONLINE&&!online){online=1;online_at=now;printf("PHYSICAL_JOIN_IP=%u.%u.%u.%u\n",app.ip[0],app.ip[1],app.ip[2],app.ip[3]);fflush(stdout);}
  if(physical_private_input[113]!=2&&app.ping_rx>=3&&app.ping_tx>=3&&http.responses>=1&&(!physical_private_input[113]||(uint32_t)(now-online_at)>90000)){printf("PHYSICAL_JOIN_TEST PASS real-USB real-WPA2 real-DHCP real-ping real-HTTP rx=%lu tx=%lu http=%lu\n",(unsigned long)app.ping_rx,(unsigned long)app.ping_tx,(unsigned long)http.responses);return 0;}
  if(queued.length){unsigned kind=queued_kind;response.wire=queued;queued.length=queued_kind=0;
   if(call(RTL_RPC_TX,sizeof(RtlWire))){printf("PHYSICAL_JOIN_TEST FAIL TX RPC timeout phase=%u\n",physical_phase);return 1;}
   physical_tx_bytes=response.wire.length;physical_sent(response.wire.result,kind,now_ms());
  }else if(physical_phase==PH_SCAN_SET||physical_phase==PH_AUTH_SET){
   memset(&response,0,sizeof(response));response.wire.channel=physical_channel;
   if(physical_phase==PH_AUTH_SET){response.wire.length=6;memcpy(response.wire.bytes,app.scan.ap[app.selected].bssid,6);}
   if(call(RTL_RPC_CHANNEL,sizeof(RtlWire))||response.wire.result){printf("PHYSICAL_JOIN_TEST FAIL channel result=%ld\n",(long)response.wire.result);return 1;}
   if(physical_phase==PH_SCAN_SET){physical_phase=PH_SCAN;physical_deadline=now_ms()+350;}
   else physical_auth(now_ms());
  }else{
   if(call(RTL_RPC_RX,sizeof(RtlWire))||response.wire.result){printf("PHYSICAL_JOIN_TEST FAIL RX\n");return 1;}
   if(response.wire.length)physical_packet(response.wire.bytes,response.wire.length,now_ms());
  }
  DelayThread(5000);
 }
 printf("PHYSICAL_JOIN_TEST FAIL timeout phase=%u %s\n",physical_phase,app.message);return 1;
}
