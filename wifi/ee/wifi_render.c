/* SPDX-License-Identifier: MIT */
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "wifi_render.h"
#include "wifi_version.h"
enum {TEXT=0x00eeeeee,FAINT=0x00999999,ACCENT=0x00ffd166,WARNING=0x006699ff,GREEN=0x0066ee66};
/* Compose in RAM, then write only changed rows. Never blank a live TV frame. */
static char frame[32][77],previous[32][77];static unsigned ink[32][76],old_ink[32][76];static int initialized;
void wifi_render_reset(void){initialized=0;memset(previous,0,sizeof(previous));memset(old_ink,0,sizeof(old_ink));}
static void begin_frame(void){unsigned y,x;if(!initialized){scr_setbgcolor(0x001c1712);scr_clear();scr_setCursor(0);initialized=1;}for(y=0;y<32;y++){memset(frame[y],' ',76);frame[y][76]=0;for(x=0;x<76;x++)ink[y][x]=TEXT;}}
static void line(int x,int y,unsigned int color,const char *text){unsigned n;if(x<0||x>=76||y<0||y>=32)return;for(n=0;text[n]&&x+(int)n<76;n++){frame[y][x+n]=text[n];ink[y][x+n]=color;}}
static void full_line(int x,int y,unsigned color,const char*text){unsigned i;memset(frame[y],' ',76);for(i=0;i<76;i++)ink[y][i]=color;line(x,y,color,text);}
static void end_frame(void){unsigned y,x,end,start,last;char run[77];for(y=0;y<28;y++){last=0;for(x=0;x<76;x++)if(frame[y][x]!=' ')last=x;for(x=last+1;x<76;x++)ink[y][x]=ink[y][last];if(!memcmp(frame[y],previous[y],76)&&!memcmp(ink[y],old_ink[y],sizeof(ink[y])))continue;
 for(start=0;start<76;start=end){for(end=start+1;end<76&&ink[y][end]==ink[y][start];end++);memcpy(run,frame[y]+start,end-start);run[end-start]=0;scr_setfontcolor(ink[y][start]);scr_setXY(start,y);scr_printf("%s",run);}memcpy(previous[y],frame[y],77);memcpy(old_ink[y],ink[y],sizeof(ink[y]));}}
static void ipline(int y,const char *title,const uint8_t ip[4]){char s[80];snprintf(s,sizeof(s),"%-12s %u.%u.%u.%u",title,ip[0],ip[1],ip[2],ip[3]);line(6,y,TEXT,s);}
static void diagnostics(const WifiApp*a){char s[160];if(a->log_warning)full_line(4,26,0x004040ff,"WARNING: Log reached / exceeded the 555 KB limit!");else if(a->log_failed)full_line(4,26,0x004040ff,"ERROR: Cannot write log to memory card.");
 snprintf(s,sizeof(s),"RAM:%uMB Heap:%uKB | Battery estimate:%s %u%%",a->ram_bytes/(1024*1024),a->heap_bytes/1024,a->battery_set?"":"not set",a->battery_percent);line(4,27,FAINT,s);
 if(a->linked){snprintf(s,sizeof(s),"IP:%u.%u.%u.%u Q:%s%u%% RX:%ukbps/%u.%03uM TX:%ukbps/%u.%03uM",a->ip[0],a->ip[1],a->ip[2],a->ip[3],a->signal_valid?"":"?",a->signal_quality,a->rx_kbps,a->rx_kbps/1000,a->rx_kbps%1000,a->tx_kbps,a->tx_kbps/1000,a->tx_kbps%1000);full_line(4,22,FAINT,s);}
 if(a->transfer_active){const char*b=strrchr(a->transfer_path,'/');snprintf(s,sizeof(s),"%s: %.24s %lu/%lu B %u%%",a->transfer_direction?"Sent":"Received",b?b+1:a->transfer_path,(unsigned long)a->transfer_done,(unsigned long)a->transfer_size,a->transfer_size?(unsigned)((uint64_t)a->transfer_done*100/a->transfer_size):0);full_line(4,17,ACCENT,s);snprintf(s,sizeof(s),"Path: %.63s",a->transfer_path);full_line(4,15,FAINT,s);}
}
void wifi_draw(const WifiApp *a,int preview,int pad_connected){
    char s[160],ssid[129],key[12];unsigned int i,j,start,end;
    begin_frame();
    line(4,1,ACCENT,"PS2 WIFI " WIFI_VERSION " | RTL8192CU");
    line(4,2,FAINT,"--------------------------------------------------------------------");
    line(4,3,preview?WARNING:FAINT,preview?"UI PREVIEW - sample network/IP/ping; NO real connection":"PS2 WIFI v" WIFI_VERSION " - USB Wi-Fi and HTTP robot control");
    if(a->state==WIFI_LIST||a->state==WIFI_SCANNING){
        line(4,5,ACCENT,"Wireless network selection");
        start=(a->selected/8)*8;end=start+8;if(end>a->scan.count)end=a->scan.count;
        for(i=start;i<end;i++){
            rtl_scan_label(&a->scan.ap[i],ssid,sizeof(ssid));
            if(strlen(ssid)>44){ssid[41]='.';ssid[42]='.';ssid[43]='.';ssid[44]=0;}
            if(a->known[i])strcat(ssid,"*");
            snprintf(s,sizeof(s),"%s %-44s CH %2u %s",i==a->selected?">":" ",ssid,a->scan.ap[i].channel,rtl_scan_security_label(&a->scan.ap[i]));
            line(5,7+(int)(i-start)*2,a->linked&&i==a->active_ap?GREEN:i==a->selected?ACCENT:TEXT,s);
        }
        if(!a->scan.count)line(6,9,FAINT,a->state==WIFI_SCANNING?"Scanning...":"No networks to display.");
        line(5,24,FAINT,"* Saved | R2: connect | X: select | TRIANGLE: IP | L2: scan");
        snprintf(s,sizeof(s),"%u networks | %u-%u",a->scan.count,a->scan.count?start+1:0,end);line(5,23,FAINT,s);
        if(a->linked){snprintf(s,sizeof(s),"HTTP: %u.%u.%u.%u | RX:%lu TX:%lu",a->ip[0],a->ip[1],a->ip[2],a->ip[3],(unsigned long)a->packets_rx,(unsigned long)a->packets_tx);line(5,22,GREEN,s);}
    }else if(a->state==WIFI_IPEDIT){
        line(4,5,ACCENT,"STATIC IP ADDRESS");snprintf(s,sizeof(s),"IP: %s",a->ip_text);line(6,7,TEXT,s);
        for(i=0;i<4;i++)for(j=0;j<4;j++){snprintf(s,sizeof(s),"%s%s%s",a->row==i&&a->column==j?"[":" ",wifi_ip_key(i,j),a->row==i&&a->column==j?"]":" ");line(6+j*15,10+i*2,a->row==i&&a->column==j?ACCENT:TEXT,s);}
        line(4,20,FAINT,"D-pad: select | X: type | O: erase | R2: save | SQUARE: DHCP");
        line(4,22,FAINT,"SELECT: cancel. Applied on next connection.");
    }else if(a->state==WIFI_PASSWORD){
        rtl_scan_label(&a->scan.ap[a->selected],ssid,sizeof(ssid));snprintf(s,sizeof(s),"Network: %.60s",ssid);line(4,5,ACCENT,s);
        memset(s,0,sizeof(s));memcpy(s,"Password: ",10);
        for(i=0;i<a->password_length;i++)s[10+i]=a->reveal?a->password[i]:'*';
        line(4,7,TEXT,s);snprintf(s,sizeof(s),"%u / 63 characters  %s",a->password_length,a->shift?"UPPERCASE":"lowercase");line(4,8,FAINT,s);
        for(i=0;i<5;i++)for(j=0;j<wifi_keyboard_columns(i);j++){
            int x=i<4?6+(int)j*5:5+(int)j*11;
            snprintf(s,sizeof(s),"%s%s%s",a->row==i&&a->column==j?"[":" ",wifi_keyboard_label(i,j,a->shift,key),a->row==i&&a->column==j?"]":" ");
            line(x,10+(int)i*2,a->row==i&&a->column==j?ACCENT:TEXT,s);
        }
        line(4,21,FAINT,"D-pad: select X: type O: erase TRIANGLE: space L2: case");
        line(4,22,FAINT,"SQUARE: show/hide R2: connect SELECT: back to networks");
    }else if(a->state==WIFI_ONLINE){
        line(4,5,ACCENT,"CONNECTED");rtl_scan_label(&a->scan.ap[a->selected],ssid,sizeof(ssid));snprintf(s,sizeof(s),"Network: %.60s",ssid);line(6,7,TEXT,s);
        ipline(9,"IP address:",a->ip);ipline(11,"Subnet mask:",a->mask);ipline(13,"Gateway:",a->gateway);
        snprintf(s,sizeof(s),"On another device: ping %u.%u.%u.%u",a->ip[0],a->ip[1],a->ip[2],a->ip[3]);line(6,16,ACCENT,s);
        snprintf(s,sizeof(s),"Ping requests: %lu   Replies sent: %lu",(unsigned long)a->ping_rx,(unsigned long)a->ping_tx);line(6,18,TEXT,s);
        snprintf(s,sizeof(s),"All packets RX:%lu TX:%lu Errors:%lu",(unsigned long)a->packets_rx,(unsigned long)a->packets_tx,(unsigned long)a->packets_failed);line(6,19,TEXT,s);
        snprintf(s,sizeof(s),"Bytes RX:%lu TX:%lu",(unsigned long)a->bytes_rx,(unsigned long)a->bytes_tx);line(6,20,TEXT,s);
        if(a->last_request[0]){snprintf(s,sizeof(s),"Last request: %.60s",a->last_request);line(6,21,TEXT,s);}
        else if(a->ping_rx){snprintf(s,sizeof(s),"Last request: PING %u.%u.%u.%u",a->ping_remote[0],a->ping_remote[1],a->ping_remote[2],a->ping_remote[3]);line(6,21,TEXT,s);}
        line(6,23,FAINT,preview?"SELECT: networks   R1: synthetic ping test":"SELECT: networks (Wi-Fi/HTTP stays on) | TRIANGLE: IP");
    }else{
        const char *title=a->state==WIFI_DHCP?"Waiting for IP address...":(a->state==WIFI_JOINING||a->state==WIFI_DERIVING?"Connecting...":(a->state==WIFI_ERROR?"Device error":"Preparing USB device..."));
        line(6,6,ACCENT,title);
        if(a->state==WIFI_BOOT||a->state==WIFI_ERROR){
            const char *stages[]={"Driver loaded","Wi-Fi attached / USB ready","Firmware running","Radio settings loaded"};
            for(i=0;i<4;i++){snprintf(s,sizeof(s),"[%s] %s",a->boot_flags&(1u<<i)?"OK":"--",stages[i]);line(6,9+i*2,a->boot_flags&(1u<<i)?ACCENT:FAINT,s);}
        }else line(6,11,TEXT,"Network settings are saved after successful connection.");
    }
    if(a->state!=WIFI_LIST&&a->state!=WIFI_SCANNING)line(4,24,FAINT,"--------------------------------------------------------------------");
    snprintf(s,sizeof(s),"%.70s",a->message);line(4,25,TEXT,s);
    snprintf(s,sizeof(s),"USB HUB: %.62s",a->hub_message[0]?a->hub_message:"Checking devices...");line(4,4,FAINT,s);
    line(4,26,pad_connected?FAINT:WARNING,pad_connected==2?"Automatic UI tour - simulated input and network events":(pad_connected?"Gamepad ready":"Gamepad not found; connect it to continue."));
    diagnostics(a);
    end_frame();
}
void wifi_draw_drive(const WifiApp*a,uint32_t buttons,int connected,unsigned ready,unsigned left,unsigned right,unsigned applied,unsigned owner,unsigned desired){
    char s[160],pressed[100];const char *movement=desired?"CUSTOM ACTION":"STOP";wifi_pad_description(buttons,pressed,sizeof(pressed));
    if(desired==((1u<<(left*2))|(1u<<(right*2))))movement="FORWARD";
    else if(desired==((2u<<(left*2))|(2u<<(right*2))))movement="REVERSE";
    else if(desired==((2u<<(left*2))|(1u<<(right*2))))movement="LEFT";
    else if(desired==((1u<<(left*2))|(2u<<(right*2))))movement="RIGHT";
    begin_frame();
    line(4,1,ACCENT,"PS2 WIFI " WIFI_VERSION);line(4,3,ACCENT,"DRIVE MODE v" WIFI_VERSION " - GAMEPAD AND WEB");
    snprintf(s,sizeof(s),"PRESSED BUTTON: %.70s",pressed);line(5,6,buttons?ACCENT:TEXT,s);
    snprintf(s,sizeof(s),"Motion command: %s",movement);line(5,9,desired?ACCENT:TEXT,s);
    line(5,11,FAINT,"PS2 WIFI v" WIFI_VERSION " | Button actions: HTTP / SETTINGS");
    snprintf(s,sizeof(s),"Left motor: R%u & R%u    Right motor: R%u & R%u",left*2+1,left*2+2,right*2+1,right*2+2);line(5,14,TEXT,s);
    snprintf(s,sizeof(s),"Relay board: %s   Last USB relay mask: %02X",ready?"READY":"NOT CONNECTED",applied);line(5,16,ready?TEXT:WARNING,s);
    snprintf(s,sizeof(s),"Control source: %s",owner==1?"GAMEPAD":owner==2?"WEB":"STOPPED");line(5,18,TEXT,s);
    if(a->linked){snprintf(s,sizeof(s),"Web: http://%u.%u.%u.%u/ RX:%lu TX:%lu",a->ip[0],a->ip[1],a->ip[2],a->ip[3],(unsigned long)a->packets_rx,(unsigned long)a->packets_tx);line(5,21,GREEN,s);}
    else line(5,21,FAINT,"No Wi-Fi. R2: reconnect to a known network.");
    line(5,23,FAINT,"SELECT: STOP/menu | SELECT+START (1 s): restart PS2");
    snprintf(s,sizeof(s),"%.70s",a->message);line(5,24,TEXT,s);
    snprintf(s,sizeof(s),"USB HUB: %.62s",a->hub_message[0]?a->hub_message:"Checking devices...");line(5,4,FAINT,s);
    line(5,25,connected?TEXT:WARNING,connected?"GAMEPAD READY - release button to stop":"GAMEPAD DISCONNECTED - stopping motors");
    diagnostics(a);
    end_frame();
}
