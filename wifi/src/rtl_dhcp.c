/* SPDX-License-Identifier: MIT. DHCPv4 BOOTP/UDP client (RFC 2131/2132).
 * The controlled WPA2 port must be open before calling this layer.
 */
#include "rtl_dhcp.h"
#include <string.h>
static unsigned b16(const uint8_t*p){return ((unsigned)p[0]<<8)|p[1];}
static uint32_t b32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void p16(uint8_t*p,unsigned v){p[0]=v>>8;p[1]=v;}
static void p32(uint8_t*p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static uint32_t sum(const uint8_t*p,size_t n,uint32_t v){while(n>1){v+=b16(p);p+=2;n-=2;}if(n)v+=(unsigned)*p<<8;return v;}
static unsigned fold(uint32_t v){while(v>>16)v=(v&65535)+(v>>16);return (~v)&65535;}
static int ipok(const uint8_t*p){return p[0]&&p[0]!=127&&p[0]<224&&b32(p)!=0xffffffffu;}
void rtl_dhcp_start(RtlDhcp*s,const uint8_t mac[6],uint32_t xid,uint32_t now){
    if(!s||!mac)return;
    memset(s,0,sizeof(*s));memcpy(s->mac,mac,6);
    s->xid=xid;s->state=RTL_DHCP_DISCOVER;s->deadline=now;
}
int rtl_dhcp_has_lease(const RtlDhcp*s){return s->state==RTL_DHCP_BOUND||s->state==RTL_DHCP_RENEW||s->state==RTL_DHCP_REBIND;}
int rtl_dhcp_tx(RtlDhcp*s,uint32_t now,uint8_t*out,size_t cap){
    uint8_t *ip,*udp,*b,*o;unsigned n,un;
    if(!s||!out||cap<400)return -1;
    if(rtl_dhcp_has_lease(s)){
        uint32_t age=(uint32_t)(now-s->lease_started)/1000;
        if(s->lease_seconds==0xffffffffu)return 0;
        if(age>=s->lease_seconds){s->state=RTL_DHCP_FAILED;memset(s->ip,0,4);return -2;}
        if(s->state==RTL_DHCP_BOUND&&age>=s->t1){s->state=RTL_DHCP_RENEW;s->xid++;s->attempts=0;s->deadline=now;}
        if(s->state==RTL_DHCP_RENEW&&age>=s->t2){s->state=RTL_DHCP_REBIND;s->attempts=0;s->deadline=now;}
        if(s->state==RTL_DHCP_BOUND)return 0;
    }
    if(s->state!=RTL_DHCP_DISCOVER&&s->state!=RTL_DHCP_REQUEST&&!rtl_dhcp_has_lease(s))return 0;
    if((int32_t)(now-s->deadline)<0)return 0;
    if(s->attempts>=5&&!rtl_dhcp_has_lease(s)){s->state=RTL_DHCP_FAILED;memset(s->ip,0,4);return -2;}
    memset(out,0,400);memset(out,255,6);memcpy(out+6,s->mac,6);out[12]=8;
    ip=out+14;udp=ip+20;b=udp+8;o=b+240;
    b[0]=1;b[1]=1;b[2]=6;p32(b+4,s->xid);p16(b+10,0x8000);memcpy(b+28,s->mac,6);
    if(rtl_dhcp_has_lease(s)){memcpy(b+12,s->ip,4);if(s->state==RTL_DHCP_RENEW){memcpy(out,s->server_mac,6);p16(b+10,0);}}
    memcpy(b+236,"\x63\x82\x53\x63",4);
    *o++=53;*o++=1;*o++=s->state==RTL_DHCP_DISCOVER?1:3;
    *o++=61;*o++=7;*o++=1;memcpy(o,s->mac,6);o+=6;
    if(s->state==RTL_DHCP_REQUEST){*o++=50;*o++=4;memcpy(o,s->ip,4);o+=4;
        *o++=54;*o++=4;memcpy(o,s->server,4);o+=4;}
    else if(s->state==RTL_DHCP_DISCOVER&&ipok(s->preferred)){*o++=50;*o++=4;memcpy(o,s->preferred,4);o+=4;}
    *o++=55;*o++=4;*o++=1;*o++=3;*o++=6;*o++=51;*o++=255;
    /* DHCP minimum BOOTP message length is 300 bytes. */
    n=(unsigned)(o-b);if(n<300)n=300;un=n+8;
    p16(udp,68);p16(udp+2,67);p16(udp+4,un);
    ip[0]=0x45;p16(ip+2,un+20);ip[8]=64;ip[9]=17;memset(ip+16,255,4);
    if(rtl_dhcp_has_lease(s)){memcpy(ip+12,s->ip,4);if(s->state==RTL_DHCP_RENEW)memcpy(ip+16,s->server,4);}
    p16(ip+10,fold(sum(ip,20,0))); /* IPv4 permits zero UDP checksum. */
    if(rtl_dhcp_has_lease(s)){uint32_t age=(uint32_t)(now-s->lease_started)/1000,remain=(s->state==RTL_DHCP_RENEW?s->t2:s->lease_seconds)-age,wait=remain/2;if(wait<60)wait=60;if(wait>remain)wait=remain;if(wait>3600)wait=3600;s->deadline=now+wait*1000;}
    else s->deadline=now+(4000u<<s->attempts);
    s->attempts++;
    return 14+20+un;
}
int rtl_dhcp_decline(const RtlDhcp*s,uint32_t now,uint8_t*p,size_t cap){RtlDhcp c=*s;int n;c.state=RTL_DHCP_REQUEST;c.deadline=now;c.attempts=0;n=rtl_dhcp_tx(&c,now,p,cap);if(n>0)p[14+20+8+240+2]=4;return n;}
int rtl_dhcp_rx(RtlDhcp*s,uint32_t now,const uint8_t*p,size_t n){
    const uint8_t *ip,*u,*b,*o,*end;unsigned ih,il,ul,type=0,seen=0;
    uint8_t server[4]={0},mask[4]={0},gw[4]={0},dns[4]={0};uint32_t lease=0,t1=0,t2=0;int renewing;
    if(!s||!p||n<14+20+8+240||(s->state!=RTL_DHCP_DISCOVER&&s->state!=RTL_DHCP_REQUEST&&!rtl_dhcp_has_lease(s)))return -1;
    renewing=s->state==RTL_DHCP_RENEW||s->state==RTL_DHCP_REBIND;
    if(b16(p+12)!=0x800||(!(p[0]&1)&&memcmp(p,s->mac,6)))return -1;
    ip=p+14;ih=(ip[0]&15)*4;il=b16(ip+2);
    if((ip[0]>>4)!=4||ih<20||ih>60||il<ih+248||il>n-14||ip[9]!=17||
       (b16(ip+6)&0x3fff)||fold(sum(ip,ih,0)))return -1;
    u=ip+ih;ul=b16(u+4);if(b16(u)!=67||b16(u+2)!=68||ul!=il-ih)return -1;
    if(b16(u+6)&&fold(sum(u,ul,sum(ip+12,8,17+ul))))return -1;
    b=u+8;if(b[0]!=2||b[1]!=1||b[2]!=6||b32(b+4)!=s->xid||memcmp(b+28,s->mac,6)||
       memcmp(b+236,"\x63\x82\x53\x63",4))return -1;
    o=b+240;end=u+ul;
    while(o<end){unsigned code=*o++,len,bit=0;if(code==255){seen|=128;break;}if(!code)continue;
        if(o==end)return -1;
        len=*o++;if((size_t)(end-o)<len)return -1;
        switch(code){case 53:bit=1;if(len!=1)return -1;type=o[0];break;
        case 54:bit=2;if(len!=4)return -1;memcpy(server,o,4);break;
        case 1:bit=4;if(len!=4)return -1;memcpy(mask,o,4);break;
        case 3:bit=8;if(len<4||len%4)return -1;memcpy(gw,o,4);break;
        case 6:bit=16;if(len<4||len%4)return -1;memcpy(dns,o,4);break;
        case 51:bit=32;if(len!=4)return -1;lease=b32(o);break;
        case 58:bit=256;if(len!=4)return -1;t1=b32(o);break;
        case 59:bit=512;if(len!=4)return -1;t2=b32(o);break;
        case 52:return -1; /* overloaded BOOTP options unsupported, fail closed */
        default:break;}
        if(seen&bit)return -1;
        seen|=bit;o+=len;
    }
    if((seen&131)!=131||!ipok(server))return -1;
    if((s->state==RTL_DHCP_REQUEST||s->state==RTL_DHCP_RENEW)&&memcmp(server,s->server,4))return -1;
    if(type==6&&(s->state==RTL_DHCP_REQUEST||renewing)){s->state=RTL_DHCP_FAILED;memset(s->ip,0,4);return -2;}
    if(renewing&&type==5){if(memcmp(b+16,"\0\0\0\0",4)&&memcmp(b+16,s->ip,4))return -1;if(!(seen&4))memcpy(mask,s->mask,4);if(!(seen&8))memcpy(gw,s->gateway,4);if(!(seen&16))memcpy(dns,s->dns,4);}
    if(!renewing&&!ipok(b+16))return -1;
    if(type==2&&s->state==RTL_DHCP_DISCOVER){memcpy(s->ip,b+16,4);memcpy(s->server,server,4);
        s->state=RTL_DHCP_REQUEST;s->attempts=0;s->deadline=now;return 1;}
    if(type==5&&(s->state==RTL_DHCP_REQUEST||renewing)){
        uint32_t inv=~b32(mask);
        if((!renewing&&memcmp(b+16,s->ip,4))||(!renewing&&!(seen&4))||!(seen&32)||!lease||!b32(mask)||
           (inv&(inv+1))||(!((b32(s->ip))&inv))||((b32(s->ip)&inv)==inv))return -1;
        memcpy(s->mask,mask,4);memcpy(s->gateway,gw,4);memcpy(s->dns,dns,4);
        memcpy(s->server,server,4);memcpy(s->server_mac,p+6,6);
        s->lease_seconds=lease;s->lease_started=now;s->t1=t1?t1:lease/2;s->t2=t2?t2:lease-lease/8;
        if(s->t1>=s->t2||s->t2>=lease){s->t1=lease/2;s->t2=lease-lease/8;if(s->t2>=lease&&lease>1)s->t2=lease-1;}
        s->state=RTL_DHCP_BOUND;return renewing?3:2;
    }
    return -1;
}
