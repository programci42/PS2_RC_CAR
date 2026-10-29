/* SPDX-License-Identifier: MIT -- RFC 826 ARP / RFC 792 ICMP echo. */
#include "rtl_echo.h"
#include <string.h>
static unsigned int be16(const uint8_t *p){return ((unsigned int)p[0]<<8)|p[1];}
static void put16(uint8_t *p,unsigned int v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static unsigned int checksum(const uint8_t *p,size_t n){uint32_t sum=0;while(n>1){sum+=be16(p);p+=2;n-=2;}if(n)sum+=(uint32_t)*p<<8;while(sum>>16)sum=(sum&65535)+(sum>>16);return (unsigned int)(~sum&65535);}
static int nonzero(const uint8_t *p,size_t n){uint8_t v=0;while(n--)v|=*p++;return v!=0;}
int rtl_echo_reply(const uint8_t mac[6],const uint8_t ip[4],const uint8_t *f,size_t n,uint8_t *out,size_t cap,RtlEchoEvent *event){
    RtlEchoEvent e;uint8_t remote_mac[6];unsigned int type,total,icmp;
    if(event)memset(event,0,sizeof(*event));
    if(!mac||!ip||!f||!out||n<14||!ip[0]||ip[0]>=224||ip[0]==127||mac[0]&1||!nonzero(mac,6))return 0;
    if((f[6]&1)||!nonzero(f+6,6)||!memcmp(f+6,mac,6))return 0;
    memcpy(remote_mac,f+6,6);memset(&e,0,sizeof(e));type=be16(f+12);
    if(type==0x0806){
        static const uint8_t broadcast[6]={255,255,255,255,255,255};
        if(n<42||cap<42||(memcmp(f,mac,6)&&memcmp(f,broadcast,6))||be16(f+14)!=1||be16(f+16)!=0x0800||
            f[18]!=6||f[19]!=4||be16(f+20)!=1||memcmp(f+22,remote_mac,6)||memcmp(f+38,ip,4))return 0;
        memcpy(e.remote,f+28,4);e.kind=RTL_ECHO_ARP;
        memset(out,0,42);memcpy(out,remote_mac,6);memcpy(out+6,mac,6);put16(out+12,0x0806);
        put16(out+14,1);put16(out+16,0x0800);out[18]=6;out[19]=4;put16(out+20,2);
        memcpy(out+22,mac,6);memcpy(out+28,ip,4);memcpy(out+32,remote_mac,6);memcpy(out+38,e.remote,4);
        if(event)*event=e;
        return 42;
    }
    if(type!=0x0800||n<42||memcmp(f,mac,6)||f[14]!=0x45||f[22]==0||f[23]!=1)return 0;
    total=be16(f+16);
    if(total<28||total>1500||total>n-14||cap<total+14||(be16(f+20)&0xbfff)||checksum(f+14,20)||memcmp(f+30,ip,4))return 0;
    if(!f[26]||f[26]==127||f[26]>=224)return 0;
    icmp=total-20;
    if(f[34]!=8||f[35]!=0||checksum(f+34,icmp))return 0;
    e.kind=RTL_ECHO_ICMP;memcpy(e.remote,f+26,4);e.identifier=(uint16_t)be16(f+38);e.sequence=(uint16_t)be16(f+40);e.payload_bytes=icmp-8;
    memmove(out,f,total+14);memcpy(out,remote_mac,6);memcpy(out+6,mac,6);
    memcpy(out+26,ip,4);memcpy(out+30,e.remote,4);out[22]=64;out[24]=out[25]=0;put16(out+24,checksum(out+14,20));
    out[34]=0;out[36]=out[37]=0;put16(out+36,checksum(out+34,icmp));
    if(event)*event=e;
    return (int)(total+14);
}
