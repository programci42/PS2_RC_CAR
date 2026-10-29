#include "wifi_controls.h"
#include <string.h>
void wifi_controls_init(WifiControls*c){memset(c,0,sizeof(*c));c->drive=1;}
uint32_t wifi_controls_input(WifiControls*c,WifiApp*a,uint32_t b,int connected,uint32_t now){
 unsigned before=c->held,edge=b&~before;c->held=b;c->display_changed=0;c->reconnect=0;
 if(!(b&WIFI_R2))c->reconnect_latched=0;
 if(c->display_latched){if(!(b&(WIFI_L2|WIFI_R2)))c->display_latched=0;else{a->held_buttons=b;return 0;}}
 if(connected&&(b&(WIFI_L2|WIFI_R2))==(WIFI_L2|WIFI_R2)){
  if((before&(WIFI_L2|WIFI_R2))!=(WIFI_L2|WIFI_R2)){c->display=!c->display;c->display_changed=1;c->display_latched=1;a->dirty=1;}
  a->held_buttons=b;return 0;
 }
 if(connected&&(b&(WIFI_SELECT|WIFI_START))==(WIFI_SELECT|WIFI_START)){
  c->drive=0;if(!c->chord_at)c->chord_at=now?now:1;
  if((uint32_t)(now-c->chord_at)>=1000)c->restart=1;
  a->held_buttons=b;return 0;
 }c->chord_at=0;
 if((edge&WIFI_START)&&a->state!=WIFI_PASSWORD&&a->state!=WIFI_IPEDIT){c->drive=!c->drive;a->dirty=1;a->held_buttons=b;return 0;}
 if(b&WIFI_SELECT){if(c->drive){c->drive=0;if(a->linked)a->state=WIFI_LIST;a->dirty=1;a->held_buttons=b;return 0;}}
 if(c->drive){if(connected&&!a->linked&&(edge&WIFI_R2)){c->reconnect=1;c->reconnect_latched=1;}a->held_buttons=b;return 0;}
 return b&~WIFI_START;
}
