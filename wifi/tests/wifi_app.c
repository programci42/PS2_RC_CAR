/* SPDX-License-Identifier: MIT -- UI state/credentials, no simulated RF claim. */
#include "wifi_app.h"
#include <stdio.h>
#include <string.h>
static unsigned int checks;
#define CHECK(x) do{checks++;if(!(x)){printf("WIFI_APP_TEST FAIL line=%d\n",__LINE__);return 1;}}while(0)
static void tap(WifiApp *a,unsigned int button){wifi_app_input(a,button,1000);wifi_app_input(a,0,1010);}
static void list_ready(WifiApp *a){
    RtlScanList list;unsigned int i;
    wifi_app_init(a);wifi_app_driver(a,0);wifi_app_take_actions(a);memset(&list,0,sizeof(list));list.count=16;
    for(i=0;i<list.count;i++){list.ap[i].ssid_length=6;memcpy(list.ap[i].ssid,"Lab AP",6);list.ap[i].ssid[5]=(uint8_t)('A'+i);list.ap[i].channel=6;list.ap[i].wpa2_ccmp_psk=1;list.ap[i].bssid[0]=2;list.ap[i].bssid[5]=(uint8_t)i;}
    wifi_app_scan(a,a->ticket,&list,0);
}
static void password8(WifiApp *a){unsigned int i;for(i=0;i<8;i++)tap(a,WIFI_X);tap(a,WIFI_R2);}
int main(void){
    WifiApp a;WifiProfile p,decoded;uint8_t encoded[WIFI_PROFILE_BYTES],bad[WIFI_PROFILE_BYTES],pmk[32],ip[4]={192,0,2,25},mask[4]={255,255,255,0},gw[4]={192,0,2,1};
    unsigned int i,j,k;uint32_t ticket;char key[12];int seen[127]={0};
    memset(pmk,0x42,32);list_ready(&a);CHECK(a.state==WIFI_LIST&&a.scan.count==16&&a.selected==0);
    tap(&a,WIFI_UP);CHECK(a.selected==15);tap(&a,WIFI_DOWN);CHECK(a.selected==0);
    wifi_app_input(&a,WIFI_DOWN,100);CHECK(a.selected==1);wifi_app_input(&a,WIFI_DOWN,200);CHECK(a.selected==1);
    wifi_app_input(&a,WIFI_DOWN,480);CHECK(a.selected==2);wifi_app_input(&a,WIFI_DOWN,500);CHECK(a.selected==2);wifi_app_input(&a,0,510);
    tap(&a,WIFI_X);CHECK(a.state==WIFI_PASSWORD&&a.password_length==0);tap(&a,WIFI_X);CHECK(a.password_length==1&&a.password[0]=='1');
    wifi_app_input(&a,WIFI_X,2000);wifi_app_input(&a,WIFI_X,2500);CHECK(a.password_length==2);wifi_app_input(&a,0,2510);
    tap(&a,WIFI_O);CHECK(a.password_length==1);tap(&a,WIFI_L2);tap(&a,WIFI_X);CHECK(a.password[1]=='!');
    tap(&a,WIFI_TRIANGLE);CHECK(a.password[2]==' ');tap(&a,WIFI_SQUARE);CHECK(a.reveal);tap(&a,WIFI_R2);CHECK(a.state==WIFI_PASSWORD&&wifi_app_take_actions(&a)==0);
    for(i=0;i<65;i++)tap(&a,WIFI_X);
    CHECK(a.password_length==63&&a.password[63]==0);
    tap(&a,WIFI_SELECT);CHECK(a.state==WIFI_LIST&&a.password_length==0&&!a.password[0]&&!a.reveal);
    wifi_app_take_actions(&a);tap(&a,WIFI_X);password8(&a);ticket=a.ticket;
    CHECK(a.state==WIFI_DERIVING&&wifi_app_take_actions(&a)==WIFI_DO_DERIVE);
    wifi_app_bound(&a,ticket,ip,mask,gw);CHECK(a.state==WIFI_DERIVING&&!a.has_profile);
    wifi_app_key(&a,ticket,pmk,0);CHECK(a.state==WIFI_JOINING&&!a.password_length&&!a.password[0]);CHECK(wifi_app_take_actions(&a)==WIFI_DO_JOIN);
    wifi_app_bound(&a,ticket,ip,mask,gw);CHECK(a.state==WIFI_JOINING&&!a.has_profile);
    wifi_app_authenticated(&a,ticket+1);CHECK(a.state==WIFI_JOINING);
    wifi_app_authenticated(&a,ticket);CHECK(a.state==WIFI_DHCP&&a.authenticated);
    wifi_app_bound(&a,ticket+1,ip,mask,gw);CHECK(a.state==WIFI_DHCP);
    wifi_app_bound(&a,ticket,ip,mask,gw);CHECK(a.state==WIFI_ONLINE&&!memcmp(a.ip,ip,4)&&a.has_profile&&a.profile.generation==1);
    CHECK(wifi_app_take_actions(&a)==WIFI_DO_SAVE&&!memcmp(a.profile.pmk,pmk,32));
    CHECK(a.ping_rx==0&&a.ping_tx==0);wifi_app_echo_rx(&a,ticket,gw,7,1);CHECK(a.ping_rx==1&&a.ping_tx==0);
    wifi_app_echo_tx(&a,ticket,-1);CHECK(a.ping_tx==0&&a.ping_failed==1);wifi_app_echo_tx(&a,ticket,0);CHECK(a.ping_tx==0);
    wifi_app_echo_rx(&a,ticket,gw,7,2);wifi_app_echo_tx(&a,ticket,0);CHECK(a.ping_tx==1);
    wifi_app_echo_rx(&a,ticket+1,gw,7,3);CHECK(a.ping_rx==2);wifi_app_saved(&a,-1);CHECK(strstr(a.message,"kaydedilemedi")!=NULL&&a.state==WIFI_ONLINE);
    p=a.profile;CHECK(wifi_profile_encode(&p,encoded)==0);CHECK(wifi_profile_decode(encoded,sizeof(encoded),&decoded)==0&&!memcmp(&p,&decoded,sizeof(p)));
    for(i=0;i<sizeof(encoded);i++){memcpy(bad,encoded,sizeof(bad));bad[i]^=1;CHECK(wifi_profile_decode(bad,sizeof(bad),&decoded)!=0);}
    for(i=0;i<sizeof(encoded);i++)CHECK(wifi_profile_decode(encoded,i,&decoded)!=0);
    CHECK(wifi_profile_newer(0,0xffffffffu)&&!wifi_profile_newer(0xffffffffu,0)&&!wifi_profile_newer(1,1));
    ticket=a.ticket;tap(&a,WIFI_SELECT);CHECK(a.state==WIFI_LIST&&a.linked&&a.ip[0]==192&&a.ticket==ticket&&!wifi_app_take_actions(&a));
    wifi_app_failure(&a,a.ticket,"fixture reconnect");wifi_app_take_actions(&a);tap(&a,WIFI_X);CHECK(a.use_saved);
    tap(&a,WIFI_R2);CHECK(a.state==WIFI_JOINING&&wifi_app_take_actions(&a)==WIFI_DO_JOIN&&!memcmp(a.pending_pmk,pmk,32));
    wifi_app_failure(&a,a.ticket,"Sifre dogrulanamadi.");CHECK(a.state==WIFI_LIST&&a.profile.generation==1);CHECK(!(wifi_app_take_actions(&a)&WIFI_DO_SAVE));
    wifi_app_authenticated(&a,ticket);wifi_app_bound(&a,ticket,ip,mask,gw);CHECK(a.state==WIFI_LIST&&!a.ip[0]);
    tap(&a,WIFI_X);tap(&a,WIFI_X);CHECK(!a.use_saved&&a.password_length==1);tap(&a,WIFI_SELECT);
    tap(&a,WIFI_X);ticket=a.ticket;tap(&a,WIFI_SELECT);wifi_app_key(&a,ticket,pmk,0);CHECK(a.state==WIFI_LIST&&!a.pending_pmk[0]);
    wifi_app_disconnected(&a,"USB cikarildi");CHECK(a.state==WIFI_BOOT&&a.scan.count==0&&!a.ip[0]&&a.has_profile);
    wifi_app_driver(&a,0);wifi_app_scan(&a,a.ticket,NULL,-30);CHECK(a.state==WIFI_LIST&&!a.scan.count);tap(&a,WIFI_X);CHECK(a.state==WIFI_LIST);
    /* Every printable ASCII character must be reachable; 32 via space. */
    seen[32]=1;for(i=0;i<2;i++)for(j=0;j<4;j++)for(k=0;k<wifi_keyboard_columns(j);k++){const char *s=wifi_keyboard_label(j,k,i,key);CHECK(s[0]>=33&&s[0]<=126);seen[(unsigned char)s[0]]=1;}
    for(i=32;i<=126;i++)CHECK(seen[i]);
    /* Monotonic repeat arithmetic also works across the 32-bit clock wrap. */
    list_ready(&a);wifi_app_input(&a,WIFI_DOWN,0xffffff00u);CHECK(a.selected==1);wifi_app_input(&a,WIFI_DOWN,0x20);CHECK(a.selected==1);wifi_app_input(&a,WIFI_DOWN,0x7c);CHECK(a.selected==2);
    printf("WIFI_APP_TEST PASS checks=%u RF=NO credentials=synthetic\n",checks);return 0;
}
