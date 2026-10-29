/* RFC5227: three randomized ARP probes and two announcements. */
#include "rtl_acd.h"
#include <string.h>
static uint32_t random_(RtlAcd*a){a->random=a->random*1664525u+1013904223u;return a->random;}
void rtl_acd_start(RtlAcd*a,const uint8_t*mac,const uint8_t*ip,uint32_t now,uint32_t seed){memset(a,0,sizeof(*a));memcpy(a->mac,mac,6);memcpy(a->ip,ip,4);a->random=seed;a->deadline=now+random_(a)%1001;}
int rtl_acd_rx(RtlAcd*a,const uint8_t*p,unsigned n){
 if(n<42||p[12]!=8||p[13]!=6||p[14]||p[15]!=1||p[16]!=8||p[17]||p[18]!=6||p[19]!=4||p[20]||(p[21]!=1&&p[21]!=2)||!memcmp(p+22,a->mac,6)||memcmp(p+6,p+22,6))return 0;
 if(!memcmp(p+28,a->ip,4)||(!a->ready&&!memcmp(p+28,"\0\0\0\0",4)&&!memcmp(p+38,a->ip,4))){a->conflict=1;return 1;}return 0;
}
int rtl_acd_tx(RtlAcd*a,uint32_t now,uint8_t*p,unsigned cap){int announce;if(a->conflict||cap<42)return -1;if((int32_t)(now-a->deadline)<0||a->announcements==2)return 0;
 announce=a->probes==3;if(announce){a->ready=1;a->announcements++;a->deadline=now+2000;}else{a->probes++;a->deadline=now+(a->probes==3?2000:1000+random_(a)%1001);}
 memset(p,0,42);memset(p,255,6);memcpy(p+6,a->mac,6);p[12]=8;p[13]=6;p[15]=1;p[16]=8;p[18]=6;p[19]=4;p[21]=1;memcpy(p+22,a->mac,6);if(announce)memcpy(p+28,a->ip,4);memcpy(p+38,a->ip,4);return 42;
}
