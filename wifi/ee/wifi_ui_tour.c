/* Visual fixture only: production renderer, simulated network/device values.
 * Never install these tour ELFs on a vehicle. */
#define NEWLIB_PORT_AWARE
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "wifi_app.h"
#include "wifi_render.h"
#ifndef WIFI_TOUR_SCREEN
#define WIFI_TOUR_SCREEN 1
#endif
static WifiApp app;
int main(void){const char*names[]={"PS2 Lab","Workshop","Guest"};unsigned i;SifInitRpc(0);init_scr();wifi_app_init(&app);
 app.state=WIFI_LIST;app.scan.count=3;app.selected=0;app.known[0]=1;app.ram_bytes=32u*1024*1024;app.battery_set=1;app.battery_percent=85;
 for(i=0;i<3;i++){app.scan.ap[i].ssid_length=strlen(names[i]);memcpy(app.scan.ap[i].ssid,names[i],strlen(names[i]));app.scan.ap[i].channel=i*5+1;app.scan.ap[i].wpa2_ccmp_psk=i<2;}
 if(WIFI_TOUR_SCREEN==2){app.state=WIFI_PASSWORD;app.password_length=8;memcpy(app.password,"password",9);app.row=1;app.column=3;}
 if(WIFI_TOUR_SCREEN>=3){static const unsigned char ip[]={192,0,2,25},mask[]={255,255,255,0},gw[]={192,0,2,1};app.state=WIFI_ONLINE;app.linked=1;memcpy(app.ip,ip,4);memcpy(app.mask,mask,4);memcpy(app.gateway,gw,4);app.ping_rx=app.ping_tx=3;}
 if(WIFI_TOUR_SCREEN==4){app.state=WIFI_ERROR;app.boot_flags=15;}
 snprintf(app.message,sizeof(app.message),"UI PREVIEW: simulated data; no real network traffic.");
 if(WIFI_TOUR_SCREEN==5)wifi_draw_drive(&app,0,1,1,2,1,0,0,0);else wifi_draw(&app,1,2);
 printf("WIFI_UI_TOUR screen=%d synthetic=YES physical-network=NO\n",WIFI_TOUR_SCREEN);SleepThread();return 0;}
