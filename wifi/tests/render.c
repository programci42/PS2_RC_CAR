/* Exercise the production screen renderer; only the debug-font sink is fake. */
#include "wifi_render.h"
#include "wifi_version.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
static char screen[32][100];static int x,y;static unsigned checks,color,colors[32],clears,draws;
#define D(c) do{checks++;if(!(c)){printf("RENDER_TEST FAIL line=%d %s\n",__LINE__,#c);exit(1);}}while(0)
void scr_setbgcolor(unsigned c){(void)c;}void scr_setfontcolor(unsigned c){color=c;}void scr_setCursor(int c){(void)c;}
void scr_clear(void){clears++;memset(screen,0,sizeof(screen));}void scr_setXY(int a,int b){x=a;y=b;}
int scr_printf(const char*f,...){char text[256];va_list a;int n;draws++;va_start(a,f);n=vsnprintf(text,sizeof(text),f,a);va_end(a);if(y>=0&&y<32&&x>=0&&x<99){size_t size=strlen(text);if(size>(unsigned)(99-x))size=99-x;memcpy(screen[y]+x,text,size);screen[y][x+size]=0;colors[y]=color;}return n;}
int main(void){WifiApp a;unsigned i;wifi_app_init(&a);a.state=WIFI_LIST;a.scan.count=1;a.scan.ap[0].ssid_length=4;memcpy(a.scan.ap[0].ssid,"IEEE",4);a.scan.ap[0].channel=6;a.scan.ap[0].wpa2_ccmp_psk=1;a.known[0]=1;
 wifi_draw(&a,0,1);D(strstr(screen[7]+5,"IEEE*"));D(strstr(screen[24]+5,"R2:"));D(strstr(screen[4]+4,"USB HUB:"));
 {unsigned before=draws;wifi_draw(&a,0,1);D(draws==before&&clears==1);}
 a.known[0]=0;wifi_draw(&a,0,1);D(!strchr(screen[7]+5,'*'));D(strstr(screen[1]+4,"PS2 WIFI"));D(!strstr(screen[3]+4,"DENEYSEL"));
 a.linked=1;a.ip[0]=192;a.ip[1]=168;a.ip[2]=1;a.ip[3]=43;a.packets_rx=123;a.packets_tx=45;wifi_draw(&a,0,1);D(colors[7]==0x0066ee66&&strstr(screen[22]+4,"IP:192.168.1.43"));
 a.state=WIFI_IPEDIT;strcpy(a.ip_text,"192.168.1.43");wifi_draw(&a,0,1);D(strstr(screen[5]+4,"STATIC IP")&&strstr(screen[7]+6,"192.168.1.43"));a.state=WIFI_LIST;
 a.log_warning=1;wifi_draw(&a,0,1);D(colors[26]==0x004040ff&&strstr(screen[26]+4,"555 KB"));wifi_draw_drive(&a,0,1,1,0,1,0,0,0);D(colors[26]==0x004040ff&&strstr(screen[26]+4,"555 KB"));a.log_warning=0;
 memcpy(a.password,"PRIVATE_TEST_PASSWORD",22);
 wifi_draw_drive(&a,WIFI_X,1,1,0,1,5,1,5);D(strstr(screen[6]+5,"PRESSED BUTTON: X"));D(strstr(screen[9]+5,"FORWARD"));D(strstr(screen[14]+5,"R1 & R2"));
 wifi_draw_drive(&a,0,1,1,0,1,0,0,0);D(strstr(screen[6]+5,"NONE"));D(strstr(screen[9]+5,"STOP"));D(strstr(screen[3]+4,WIFI_VERSION));D(strstr(screen[11]+5,WIFI_VERSION));D(strstr(screen[4]+5,"USB HUB:"));D(!strstr(screen[22]+4,"USB HUB:"));
 wifi_draw_drive(&a,WIFI_X|WIFI_TRIANGLE,1,1,0,1,0,0,0);D(strstr(screen[6]+5,"X + TRIANGLE"));D(strstr(screen[9]+5,"STOP"));
 wifi_draw_drive(&a,0,0,0,2,3,0,0,0);D(strstr(screen[25]+5,"GAMEPAD DISCONNECTED"));D(strstr(screen[14]+5,"R5 & R6"));
 for(i=0;i<32;i++){unsigned j;for(j=0;j<93;j++)D(memcmp(screen[i]+j,"PRIVATE",7));}
 printf("RENDER_TEST PASS checks=%u production-renderer SSID-star-R2-hint-pressed-release-conflict-disconnect-no-password\n",checks);return 0;
}
