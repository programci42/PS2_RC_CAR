#include "wifi_controls.h"
#include "wifi_video.h"
#include "wifi_known.h"
#include "rtl_acd.h"
#include "rtl_dhcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define N(x) do{checks++;if(!(x)){printf("NETWORK_UI_TEST FAIL line=%d %s\n",__LINE__,#x);exit(1);}}while(0)
static void input(WifiControls*c,WifiApp*a,uint32_t b,uint32_t now){uint32_t menu=wifi_controls_input(c,a,b,1,now);if(!c->drive)wifi_app_input(a,menu,now);}
int network_ui_test(void){WifiApp a;WifiControls c={0};RtlAcd acd;RtlDhcp dhcp;uint8_t p[400],copy[400],last[4],manual[4],mac[6]={2,1,2,3,4,5},ip[4]={192,168,1,43},other[6]={2,6,7,8,9,10};unsigned i;uint32_t ticket;int n;uint8_t csv[4097];
 wifi_app_init(&a);a.state=WIFI_ONLINE;a.linked=1;a.authenticated=1;a.scan.count=2;a.scan.ap[0].ssid_length=4;memcpy(a.scan.ap[0].ssid,"IEEE",4);memcpy(a.ip,ip,4);ticket=a.ticket;
 N(wifi_video_pmode(1)==UINT64_C(0xff62)&&wifi_video_pmode(0)==UINT64_C(0xff60));N((wifi_video_pmode(1)&3)==2&&!(wifi_video_pmode(0)&3));
 {WifiControls headless;wifi_controls_init(&headless);N(headless.drive&&!headless.display);wifi_controls_input(&headless,&a,WIFI_L2|WIFI_R2,1,0);N(headless.display&&headless.display_changed&&headless.drive);wifi_controls_input(&headless,&a,WIFI_L2|WIFI_R2,1,1);N(headless.display&&!headless.display_changed);wifi_controls_input(&headless,&a,WIFI_R2,1,2);N(headless.display&&headless.display_latched);wifi_controls_input(&headless,&a,0,1,3);wifi_controls_input(&headless,&a,WIFI_L2|WIFI_R2,1,4);N(!headless.display&&headless.display_changed&&headless.drive);a.held_buttons=0;a.dirty=1;}
 {WifiControls retry;wifi_controls_init(&retry);a.linked=0;
  wifi_controls_input(&retry,&a,WIFI_R2,1,0);N(retry.reconnect&&retry.reconnect_latched);
  wifi_controls_input(&retry,&a,WIFI_R2,1,1);N(!retry.reconnect&&retry.reconnect_latched);
  a.linked=1;wifi_controls_input(&retry,&a,WIFI_R2,1,2);N(!retry.reconnect&&retry.reconnect_latched);
  wifi_controls_input(&retry,&a,0,1,3);N(!retry.reconnect_latched);
  wifi_controls_input(&retry,&a,WIFI_R2,1,4);N(!retry.reconnect&&!retry.reconnect_latched);
  a.linked=0;wifi_controls_input(&retry,&a,0,1,5);wifi_controls_input(&retry,&a,WIFI_L2|WIFI_R2,1,6);N(retry.display_changed&&!retry.reconnect&&!retry.reconnect_latched);
  wifi_controls_input(&retry,&a,WIFI_R2,1,7);N(!retry.reconnect);wifi_controls_input(&retry,&a,0,1,8);
  wifi_controls_input(&retry,&a,WIFI_R2,0,9);N(!retry.reconnect);a.linked=1;a.held_buttons=0;
 }
 {unsigned state;for(state=WIFI_BOOT;state<=WIFI_IPEDIT;state++){WifiApp any;WifiControls chord;wifi_app_init(&any);any.state=state;wifi_controls_init(&chord);
  wifi_controls_input(&chord,&any,WIFI_L2|WIFI_R2,1,1);N(chord.display&&chord.display_changed&&!chord.reconnect);
  wifi_controls_input(&chord,&any,WIFI_R2,1,2);N(chord.display&&!chord.display_changed&&!chord.reconnect);
  wifi_controls_input(&chord,&any,0,1,3);wifi_controls_input(&chord,&any,WIFI_L2|WIFI_R2,1,4);N(!chord.display&&chord.display_changed&&!chord.reconnect);
 }}
 input(&c,&a,WIFI_START,1);N(c.drive&&a.linked&&a.ticket==ticket);input(&c,&a,0,2);input(&c,&a,WIFI_SELECT,3);N(!c.drive&&a.state==WIFI_LIST&&a.linked&&a.ticket==ticket&&!a.actions&&!memcmp(a.ip,ip,4));
 input(&c,&a,WIFI_SELECT,4);input(&c,&a,0,5);input(&c,&a,WIFI_DOWN,6);N(a.selected==1&&a.active_ap==0&&a.linked);wifi_app_echo_rx(&a,ticket,ip,1,1);wifi_app_echo_tx(&a,ticket,0);N(a.ping_rx==1&&a.ping_tx==1);
 input(&c,&a,0,7);input(&c,&a,WIFI_TRIANGLE,8);N(a.state==WIFI_IPEDIT&&a.linked);strcpy(a.ip_text,"192.168.1.99");input(&c,&a,0,9);input(&c,&a,WIFI_R2,10);N(a.state==WIFI_LIST&&a.static_ip[1][3]==99&&a.linked&&a.ticket==ticket&&(a.actions&WIFI_DO_IPSAVE));
 input(&c,&a,0,11);input(&c,&a,WIFI_TRIANGLE,12);strcpy(a.ip_text,"256.1.1.1");input(&c,&a,0,13);input(&c,&a,WIFI_R2,14);N(a.state==WIFI_IPEDIT&&a.static_ip[1][3]==99);input(&c,&a,0,15);input(&c,&a,WIFI_SQUARE,16);N(!a.static_ip[1][0]&&a.state==WIFI_LIST&&a.linked);
 input(&c,&a,0,17);input(&c,&a,WIFI_SELECT|WIFI_START,100);N(!c.drive&&!c.restart&&a.linked);input(&c,&a,WIFI_SELECT|WIFI_START,1099);N(!c.restart);input(&c,&a,WIFI_SELECT|WIFI_START,1100);N(c.restart&&a.linked&&a.ticket==ticket);
 {const char*bad[]={"192.168.1.256","192.168.1","192.168.1.43x","127.0.0.1","224.0.0.1","0.0.0.0","1..1.1","-1.2.3.4"};for(i=0;i<sizeof(bad)/sizeof(*bad);i++)N(wifi_ip_parse(bad[i],last)<0);}N(!wifi_ip_parse("192.168.1.43",last)&&!memcmp(last,ip,4));
 {const uint8_t original[]="\xef\xbb\xbfSSID;Sifre\r\n# preserve\r\nIEEE;password\r\nOther;another-password\r\n";uint8_t zero[4]={0};n=wifi_known_update(original,sizeof(original)-1,(const uint8_t*)"IEEE",4,ip,zero,csv,sizeof(csv));N(n>0&&strstr((char*)csv,"IEEE;password;192.168.1.43\r\n")&&strstr((char*)csv,"Other;another-password\r\n")&&strstr((char*)csv,"# preserve"));N(wifi_known_config(csv,n,(const uint8_t*)"IEEE",4,last,manual)==1&&!memcmp(last,ip,4)&&!manual[0]);
  N(wifi_known_update(csv,n,(const uint8_t*)"IEEE",4,ip,ip,p,sizeof(p))>0);N(strstr((char*)p,";static:192.168.1.43"));N(wifi_known_config(p,strlen((char*)p),(const uint8_t*)"IEEE",4,last,manual)==1&&!memcmp(last,ip,4)&&!memcmp(manual,ip,4));
  n=wifi_known_update(original,sizeof(original)-1,(const uint8_t*)"New",3,zero,ip,csv,sizeof(csv));N(n>0&&strstr((char*)csv,"New;;0.0.0.0;static:192.168.1.43"));N(wifi_known_config(csv,n,(const uint8_t*)"New",3,last,manual)==1&&!last[0]&&!memcmp(manual,ip,4));
 }
 rtl_acd_start(&acd,mac,ip,0,42);for(i=0;i<3;i++){uint32_t at=acd.deadline;N(rtl_acd_tx(&acd,at-1,p,sizeof(p))==0);N(rtl_acd_tx(&acd,at,p,sizeof(p))==42&&!memcmp(p,"\xff\xff\xff\xff\xff\xff",6)&&!memcmp(p+28,"\0\0\0\0",4)&&!memcmp(p+38,ip,4)&&!acd.ready);N(acd.deadline-at>=1000&&acd.deadline-at<=2000);N(!rtl_acd_rx(&acd,p,42));}
 N(rtl_acd_tx(&acd,acd.deadline,p,sizeof(p))==42&&acd.ready&&acd.announcements==1&&!memcmp(p+28,ip,4));N(rtl_acd_tx(&acd,acd.deadline,p,sizeof(p))==42&&acd.announcements==2);N(!rtl_acd_tx(&acd,acd.deadline,p,sizeof(p)));
 memcpy(copy,p,42);memcpy(copy+6,other,6);memcpy(copy+22,other,6);N(rtl_acd_rx(&acd,copy,42)==1&&acd.conflict);N(rtl_acd_tx(&acd,0,p,sizeof(p))<0);
 rtl_acd_start(&acd,mac,ip,0xffffff00u,42);N(rtl_acd_tx(&acd,acd.deadline,p,sizeof(p))==42);memcpy(p+6,other,6);memcpy(p+22,other,6);N(rtl_acd_rx(&acd,p,41)==0&&rtl_acd_rx(&acd,p,42)==1);
 rtl_dhcp_start(&dhcp,mac,42,0);memcpy(dhcp.preferred,ip,4);n=rtl_dhcp_tx(&dhcp,0,p,sizeof(p));N(n>0);{unsigned j,found=0;for(j=282;j+5<(unsigned)n;j++)if(p[j]==50&&p[j+1]==4&&!memcmp(p+j+2,ip,4))found=1;N(found);}
 memcpy(dhcp.ip,ip,4);memcpy(dhcp.server,"\xc0\xa8\x01\x01",4);N(rtl_dhcp_decline(&dhcp,1,p,sizeof(p))>0&&p[284]==4);N(dhcp.state==RTL_DHCP_DISCOVER);
 printf("NETWORK_UI_TEST PASS checks=%u SELECT-preserves-link-drive-chord-IP-keyboard-CSV-ARP-probe-conflict-DHCP-preference-decline\n",checks);return 0;
}
