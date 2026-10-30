#include "wifi_i18n.h"
unsigned wifi_language; /* Turkish is the default, independent of browser locale. */
/* SPDX-License-Identifier: MIT */
#include "wifi_app.h"
#include <string.h>
#include <stdio.h>
static const char *keys[2][4]={
    {"1234567890-=","qwertyuiop[]","asdfghjkl;'\\","zxcvbnm,./`"},
    {"!@#$%^&*()_+","QWERTYUIOP{}","ASDFGHJKL:\"|","ZXCVBNM<>?~"}
};
static const char *commands[]={"Space","Delete","Shift","Show","Connect","Cancel"};
static void wipe(void *p,size_t n){volatile unsigned char *v=p;while(n--)*v++=0;}
static void message(WifiApp *a,const char *text){size_t n=strlen(text);if(n>=sizeof(a->message))n=sizeof(a->message)-1;memcpy(a->message,text,n);a->message[n]=0;a->dirty=1;}
static void clear_password(WifiApp *a){wipe(a->password,sizeof(a->password));a->password_length=0;a->reveal=0;}
static void clear_link(WifiApp *a){a->linked=0;memset(a->ip,0,4);memset(a->mask,0,4);memset(a->gateway,0,4);a->authenticated=0;a->ping_rx=a->ping_tx=a->ping_failed=a->ping_pending=0;memset(a->ping_remote,0,4);}
int wifi_ip_parse(const char*s,uint8_t ip[4]){unsigned i;uint8_t out[4];for(i=0;i<4;i++){unsigned v=0,n=0;while(*s>='0'&&*s<='9'){v=v*10+*s++-'0';if(++n>3||v>255)return -1;}if(!n)return -1;out[i]=v;if(i<3){if(*s++!='.')return -1;}else if(*s)return -1;}if(!out[0]||out[0]==127||out[0]>=224||(!out[3]||out[3]==255))return -1;memcpy(ip,out,4);return 0;}
const char*wifi_ip_key(unsigned r,unsigned c){static const char*k[4][4]={{"1","2","3","Delete"},{"4","5","6","Save"},{"7","8","9","DHCP"},{"0",".","Cancel",""}};return r<4&&c<4?WIFI_TR(k[r][c]):"";}
static void ip_save(WifiApp*a,int automatic){uint8_t ip[4]={0};if(!automatic&&wifi_ip_parse(a->ip_text,ip)){message(a,WIFI_TR("Invalid IPv4. Example: 192.0.2.25"));return;}memcpy(a->static_ip[a->selected],ip,4);a->state=a->ip_return;a->actions|=WIFI_DO_IPSAVE;message(a,WIFI_TR("Saving IP settings. Applied on next connection."));}
static void scan_start(WifiApp *a){a->ticket++;a->state=WIFI_SCANNING;a->actions=WIFI_DO_SCAN;message(a,WIFI_TR("Scanning wireless networks..."));}
void wifi_app_init(WifiApp *a){memset(a,0,sizeof(*a));a->state=WIFI_BOOT;a->ticket=1;message(a,WIFI_TR("Starting driver and USB device..."));}
void wifi_app_driver(WifiApp *a,int result){if(a->state!=WIFI_BOOT)return;if(result){a->state=WIFI_ERROR;message(a,WIFI_TR("Cannot initialize USB device. Check adapter."));if(a->error_sink)a->error_sink(a->error_context,a->message);}else scan_start(a);}
unsigned int wifi_keyboard_columns(unsigned int row){return row<4?(unsigned int)strlen(keys[0][row]):(row==4?6:0);}
const char *wifi_keyboard_label(unsigned int row,unsigned int col,unsigned int shift,char out[12]){
    if(row>4||col>=wifi_keyboard_columns(row))return "";
    if(row==4)return WIFI_TR(commands[col]);
    out[0]=keys[!!shift][row][col];out[1]=0;return out;
}
static void typed(WifiApp *a,char c){
    if(a->password_length==63){message(a,WIFI_TR("Password cannot exceed 63 characters."));return;}
    a->use_saved=0;a->password[a->password_length++]=c;a->password[a->password_length]=0;a->dirty=1;message(a,"");
}
static void erase(WifiApp *a){
    a->use_saved=0;if(a->password_length)a->password[--a->password_length]=0;
    a->dirty=1;message(a,"");
}
static void submit(WifiApp *a){
    if(a->use_saved){memcpy(a->pending_pmk,a->profile.pmk,32);a->state=WIFI_JOINING;a->actions=WIFI_DO_JOIN;}
    else{
        if(a->password_length<8||a->password_length>63){message(a,WIFI_TR("WPA2 password must contain 8-63 characters."));return;}
        a->state=WIFI_DERIVING;a->actions=WIFI_DO_DERIVE;
    }
    a->reveal=0;message(a,WIFI_TR("Joining network... SELECT: cancel"));
}
static void cancel(WifiApp *a){
    a->ticket++;a->state=WIFI_LIST;a->actions=WIFI_DO_LEAVE;clear_password(a);wipe(a->pending_pmk,32);clear_link(a);a->use_saved=0;message(a,"");
}
static void choose(WifiApp *a){
    RtlAccessPoint *ap;
    if(a->selected>=a->scan.count)return;
    ap=&a->scan.ap[a->selected];
    if(a->linked&&a->selected==a->active_ap){a->state=WIFI_ONLINE;message(a,WIFI_TR("Already connected to this network. SELECT: networks"));return;}
    if(!ap->ssid_length){message(a,WIFI_TR("Hidden network SSID is unknown."));return;}
    if(!ap->wpa2_ccmp_psk){message(a,rtl_scan_join_message(ap));return;}
    a->ticket++;clear_link(a);clear_password(a);wipe(a->pending_pmk,32);a->row=a->column=a->shift=0;
    a->use_saved=a->has_profile&&a->profile.ssid_length==ap->ssid_length&&!memcmp(a->profile.ssid,ap->ssid,ap->ssid_length);
    a->state=WIFI_PASSWORD;message(a,a->use_saved?WIFI_TR("Saved network: R2 to connect or enter a new password."):WIFI_TR("Enter password. R2: connect SELECT: back"));
}
static void choose_known(WifiApp *a){
    RtlAccessPoint *ap;if(a->selected>=a->scan.count)return;ap=&a->scan.ap[a->selected];
    if(a->linked&&a->selected==a->active_ap){a->state=WIFI_ONLINE;message(a,WIFI_TR("Already connected. Select network with X to change IP."));return;}
    if(!ap->wpa2_ccmp_psk){message(a,rtl_scan_join_message(ap));return;}
    if(!(a->known[a->selected]&WIFI_KNOWN_CSV)&&a->has_profile&&a->profile.ssid_length==ap->ssid_length&&!memcmp(a->profile.ssid,ap->ssid,ap->ssid_length)){
        a->ticket++;clear_password(a);clear_link(a);a->use_saved=1;submit(a);return;
    }
    if(!(a->known[a->selected]&WIFI_KNOWN_CSV)){message(a,WIFI_TR("No saved password for this network. X: enter password"));return;}
    a->ticket++;clear_password(a);clear_link(a);a->state=WIFI_DERIVING;a->actions=WIFI_DO_KNOWN;message(a,WIFI_TR("Preparing saved network key..."));
}
void wifi_app_input(WifiApp *a,uint32_t buttons,uint32_t now){
    uint32_t edge=buttons&~a->held_buttons,dir=0;int move=0;
    a->held_buttons=buttons;
    if(buttons&WIFI_UP)dir=WIFI_UP;else if(buttons&WIFI_DOWN)dir=WIFI_DOWN;
    else if(buttons&WIFI_LEFT)dir=WIFI_LEFT;else if(buttons&WIFI_RIGHT)dir=WIFI_RIGHT;
    if(dir){if(dir!=a->repeat_dir){move=1;a->repeat_at=now+380;}
        else if((uint32_t)(now-a->repeat_at)<0x80000000u){move=1;a->repeat_at=now+110;}}
    a->repeat_dir=dir;
    if(edge&WIFI_SELECT){if(a->state==WIFI_IPEDIT){a->state=a->ip_return;message(a,WIFI_TR("IP unchanged."));}
        else if(a->linked){a->state=WIFI_LIST;message(a,WIFI_TR("Connection and HTTP remain active. TRIANGLE: IP settings"));}
        else if(a->state>=WIFI_PASSWORD&&a->state<=WIFI_ONLINE){cancel(a);}
        return;
    }
    if(a->state==WIFI_IPEDIT){
        if(move){if(dir==WIFI_UP)a->row=(a->row+3)%4;else if(dir==WIFI_DOWN)a->row=(a->row+1)%4;else if(dir==WIFI_LEFT)a->column=(a->column+3)%4;else if(dir==WIFI_RIGHT)a->column=(a->column+1)%4;a->dirty=1;}
        if(edge&WIFI_R2)ip_save(a,0);else if(edge&WIFI_SQUARE)ip_save(a,1);else if(edge&WIFI_O){size_t n=strlen(a->ip_text);if(n)a->ip_text[n-1]=0;a->dirty=1;}
        else if(edge&WIFI_X){const char*k=wifi_ip_key(a->row,a->column);size_t n=strlen(a->ip_text);if(!strcmp(k,WIFI_TR("Save")))ip_save(a,0);else if(!strcmp(k,"DHCP"))ip_save(a,1);else if(!strcmp(k,WIFI_TR("Cancel"))){a->state=a->ip_return;a->dirty=1;}else if(!strcmp(k,WIFI_TR("Delete"))){if(n)a->ip_text[n-1]=0;a->dirty=1;}else if(k[0]&&!k[1]&&n<15){a->ip_text[n]=k[0];a->ip_text[n+1]=0;a->dirty=1;}}
        return;
    }
    if((a->state==WIFI_LIST||a->state==WIFI_ONLINE)&&(edge&WIFI_TRIANGLE)&&a->selected<a->scan.count){const uint8_t*p=a->static_ip[a->selected][0]?a->static_ip[a->selected]:a->last_ip[a->selected];a->ip_return=a->state;a->state=WIFI_IPEDIT;a->row=a->column=0;if(p[0])snprintf(a->ip_text,sizeof(a->ip_text),"%u.%u.%u.%u",p[0],p[1],p[2],p[3]);else a->ip_text[0]=0;message(a,WIFI_TR("STATIC IP: R2 save | SQUARE DHCP | SELECT cancel"));return;}
    if(a->state==WIFI_LIST){
        if(move&&a->scan.count){if(dir==WIFI_UP)a->selected=(a->selected+a->scan.count-1)%a->scan.count;else if(dir==WIFI_DOWN)a->selected=(a->selected+1)%a->scan.count;a->dirty=1;}
        if(edge&WIFI_R2)choose_known(a);else if(edge&WIFI_X)choose(a);else if((edge&WIFI_L2)&&!a->linked)scan_start(a);
    }else if(a->state==WIFI_PASSWORD){
        if(move){
            if(dir==WIFI_UP)a->row=(a->row+4)%5;else if(dir==WIFI_DOWN)a->row=(a->row+1)%5;
            if(a->column>=wifi_keyboard_columns(a->row))a->column=wifi_keyboard_columns(a->row)-1;
            if(dir==WIFI_LEFT)a->column=(a->column+wifi_keyboard_columns(a->row)-1)%wifi_keyboard_columns(a->row);
            else if(dir==WIFI_RIGHT)a->column=(a->column+1)%wifi_keyboard_columns(a->row);
            a->dirty=1;
        }
        if(edge&WIFI_R2)submit(a);
        else if(edge&WIFI_O)erase(a);
        else if(edge&WIFI_TRIANGLE)typed(a,' ');
        else if(edge&WIFI_L2){a->shift=!a->shift;a->dirty=1;}
        else if(edge&WIFI_SQUARE){a->reveal=!a->reveal;a->dirty=1;}
        else if(edge&WIFI_X){
            if(a->row<4)typed(a,keys[a->shift][a->row][a->column]);
            else switch(a->column){case 0:typed(a,' ');break;case 1:erase(a);break;case 2:a->shift=!a->shift;break;case 3:a->reveal=!a->reveal;break;case 4:submit(a);break;case 5:cancel(a);break;}
            a->dirty=1;
        }
    }else if(a->state==WIFI_ERROR&&(edge&WIFI_TRIANGLE)){a->state=WIFI_BOOT;message(a,WIFI_TR("Restarting USB device..."));}
}
unsigned int wifi_app_take_actions(WifiApp *a){unsigned int result=a->actions;a->actions=0;return result;}
void wifi_pad_description(uint32_t buttons,char *out,size_t capacity){
    static const struct {uint32_t mask;const char*name;} key[]={
        {0x4000,"X"},{0x1000,"TRIANGLE"},{0x8000,"SQUARE"},{0x2000,"O"},
        {0x10,"UP"},{0x40,"DOWN"},{0x80,"LEFT"},{0x20,"RIGHT"},
        {0x400,"L1"},{0x800,"R1"},{0x100,"L2"},{0x200,"R2"},
        {0x8,"START"},{0x1,"SELECT"},{0x2,"L3"},{0x4,"R3"}};
    size_t used=0,i,len;if(!out||!capacity)return;out[0]=0;
    for(i=0;i<sizeof(key)/sizeof(key[0]);i++)if(buttons&key[i].mask){
        len=strlen(key[i].name);if(used+len+(used?3:0)>=capacity)break;
        if(used){memcpy(out+used," + ",3);used+=3;}memcpy(out+used,key[i].name,len);used+=len;out[used]=0;
    }
    if(!buttons)snprintf(out,capacity,"%s",WIFI_TR("NONE"));
}
void wifi_app_scan(WifiApp *a,uint32_t ticket,const RtlScanList *list,int result){
    unsigned int i;
    if(ticket!=a->ticket||a->state!=WIFI_SCANNING)return;
    a->state=WIFI_LIST;a->selected=0;a->scan.count=0;
    if(result||!list){message(a,WIFI_TR("Wireless scanning unavailable. TRIANGLE: retry"));return;}
    if(list->count>RTL_SCAN_CAPACITY){message(a,WIFI_TR("Invalid network list received."));return;}
    for(i=0;i<list->count;i++)if(list->ap[i].ssid_length>32||!list->ap[i].channel||list->ap[i].channel>14){message(a,WIFI_TR("Invalid network information received."));return;}
    a->scan=*list;message(a,list->count?WIFI_TR("D-pad: select X: join TRIANGLE: IP L2: rescan"):WIFI_TR("No networks found. L2: rescan"));
    memset(a->known,0,sizeof(a->known));
    if(a->has_profile)for(i=0;i<list->count;i++)if(list->ap[i].wpa2_ccmp_psk&&list->ap[i].ssid_length==a->profile.ssid_length&&!memcmp(list->ap[i].ssid,a->profile.ssid,a->profile.ssid_length))a->known[i]=WIFI_KNOWN_PROFILE;
}
void wifi_app_key(WifiApp *a,uint32_t ticket,const uint8_t pmk[32],int result){
    if(ticket!=a->ticket||a->state!=WIFI_DERIVING)return;
    clear_password(a);
    if(result||!pmk){wifi_app_failure(a,ticket,WIFI_TR("Cannot prepare network key."));return;}
    memcpy(a->pending_pmk,pmk,32);a->state=WIFI_JOINING;a->actions=WIFI_DO_JOIN;message(a,WIFI_TR("Authenticating network password..."));
}
void wifi_app_authenticated(WifiApp *a,uint32_t ticket){
    if(ticket!=a->ticket||a->state!=WIFI_JOINING)return;
    a->authenticated=1;a->state=WIFI_DHCP;message(a,WIFI_TR("Password verified. Obtaining IP address..."));
}
void wifi_app_bound(WifiApp *a,uint32_t ticket,const uint8_t ip[4],const uint8_t mask[4],const uint8_t gateway[4]){
    RtlAccessPoint *ap;
    if(ticket!=a->ticket||a->state!=WIFI_DHCP||!a->authenticated||!ip||!mask||!gateway)return;
    if(!ip[0]||ip[0]==127||ip[0]>=224){wifi_app_failure(a,ticket,WIFI_TR("Invalid IP address received."));return;}
    memcpy(a->ip,ip,4);memcpy(a->mask,mask,4);memcpy(a->gateway,gateway,4);a->state=WIFI_ONLINE;a->linked=1;a->active_ap=a->selected;memcpy(a->last_ip[a->selected],ip,4);
    ap=&a->scan.ap[a->selected];a->profile.generation++;
    a->profile.ssid_length=ap->ssid_length;memset(a->profile.ssid,0,32);memcpy(a->profile.ssid,ap->ssid,ap->ssid_length);
    memcpy(a->profile.pmk,a->pending_pmk,32);wipe(a->pending_pmk,32);a->has_profile=1;a->actions=WIFI_DO_SAVE;
    a->known[a->selected]|=WIFI_KNOWN_PROFILE;
    message(a,WIFI_TR("Connected. You can ping this IP from another device."));
}
void wifi_app_failure(WifiApp *a,uint32_t ticket,const char *text){
    if(ticket!=a->ticket)return;
    if(a->error_sink)a->error_sink(a->error_context,text);
    a->ticket++;a->state=WIFI_LIST;a->actions=WIFI_DO_LEAVE;clear_password(a);wipe(a->pending_pmk,32);clear_link(a);message(a,text);
}
void wifi_app_disconnected(WifiApp *a,const char *text){if(a->error_sink)a->error_sink(a->error_context,text);a->ticket++;a->state=WIFI_BOOT;a->actions=WIFI_DO_LEAVE;a->scan.count=0;clear_password(a);wipe(a->pending_pmk,32);clear_link(a);message(a,text);}
void wifi_app_echo_rx(WifiApp *a,uint32_t ticket,const uint8_t remote[4],uint16_t id,uint16_t seq){
    if(ticket!=a->ticket||!a->linked||!remote)return;
    a->ping_rx++;a->ping_pending++;memcpy(a->ping_remote,remote,4);a->ping_id=id;a->ping_sequence=seq;a->dirty=1;
}
void wifi_app_echo_tx(WifiApp *a,uint32_t ticket,int result){if(ticket!=a->ticket||!a->linked||!a->ping_pending)return;a->ping_pending--;if(result)a->ping_failed++;else a->ping_tx++;a->dirty=1;}
void wifi_app_saved(WifiApp *a,int result){if(a->state==WIFI_ONLINE)message(a,result?WIFI_TR("Connected; memory-card save failed. Connection stays active."):WIFI_TR("Connected and saved. Waiting for ping..."));}
static uint32_t crc(const uint8_t *p,size_t n){uint32_t v=0xffffffffu;unsigned int j;while(n--){v^=*p++;for(j=0;j<8;j++)v=(v>>1)^((0u-(v&1))&0xedb88320u);}return ~v;}
static void put32(uint8_t *p,uint32_t v){unsigned int i;for(i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int wifi_profile_encode(const WifiProfile *p,uint8_t out[WIFI_PROFILE_BYTES]){
    if(!p||!out||!p->ssid_length||p->ssid_length>32)return -1;
    memset(out,0,WIFI_PROFILE_BYTES);memcpy(out,"RTLNET1",7);put32(out+8,p->generation);out[12]=p->ssid_length;
    memcpy(out+16,p->ssid,p->ssid_length);memcpy(out+48,p->pmk,32);put32(out+80,crc(out,80));return 0;
}
int wifi_profile_decode(const uint8_t *data,size_t n,WifiProfile *p){
    WifiProfile copy;
    if(!p||!data||n!=WIFI_PROFILE_BYTES||memcmp(data,"RTLNET1\0",8)||!data[12]||data[12]>32||data[13]||data[14]||data[15]||get32(data+80)!=crc(data,80))return -1;
    memset(&copy,0,sizeof(copy));copy.generation=get32(data+8);copy.ssid_length=data[12];memcpy(copy.ssid,data+16,32);memcpy(copy.pmk,data+48,32);*p=copy;return 0;
}
int wifi_profile_newer(uint32_t a,uint32_t b){return a!=b&&(uint32_t)(a-b)<0x80000000u;}
