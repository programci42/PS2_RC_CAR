/* SPDX-License-Identifier: MIT */
#include "rtl_echo.h"
#include <stdio.h>
#include <string.h>
#include "echo_fixtures.h"
static unsigned int checks;
#define CHECK(x) do{checks++;if(!(x)){printf("ECHO_PACKET_TEST FAIL line=%d\n",__LINE__);return 1;}}while(0)
int main(void){
    static const uint8_t mac[6]={2,0x50,0x53,0x32,0,0x25},ip[4]={192,0,2,25};
    uint8_t reply[1514],bad[1514];RtlEchoEvent event;unsigned int i;
    CHECK(rtl_echo_reply(mac,ip,ping_request,sizeof(ping_request),reply,sizeof(reply),&event)==sizeof(ping_expected));
    CHECK(!memcmp(reply,ping_expected,sizeof(ping_expected)));CHECK(event.kind==RTL_ECHO_ICMP&&event.identifier==0x1234&&event.sequence==7&&event.payload_bytes==31);
    for(i=0;i<sizeof(ping_request);i++)CHECK(rtl_echo_reply(mac,ip,ping_request,i,reply,sizeof(reply),&event)==0&&event.kind==0);
    for(i=14;i<sizeof(ping_request);i++){memcpy(bad,ping_request,sizeof(ping_request));bad[i]^=1;CHECK(rtl_echo_reply(mac,ip,bad,sizeof(ping_request),reply,sizeof(reply),&event)==0&&event.kind==0);}
    CHECK(rtl_echo_reply(mac,ip,ping_request,sizeof(ping_request),reply,sizeof(ping_expected)-1,&event)==0);
    memcpy(bad,ping_request,sizeof(ping_request));bad[0]^=1;CHECK(rtl_echo_reply(mac,ip,bad,sizeof(ping_request),reply,sizeof(reply),&event)==0);
    memcpy(bad,ping_request,sizeof(ping_request));CHECK(rtl_echo_reply(mac,ip,bad,sizeof(ping_request),bad,sizeof(bad),&event)==sizeof(ping_expected)&&!memcmp(bad,ping_expected,sizeof(ping_expected)));
    CHECK(rtl_echo_reply(mac,ip,arp_request,sizeof(arp_request),reply,sizeof(reply),&event)==42&&!memcmp(reply,arp_expected,42)&&event.kind==RTL_ECHO_ARP);
    for(i=0;i<42;i++)CHECK(rtl_echo_reply(mac,ip,arp_request,i,reply,sizeof(reply),&event)==0);
    memcpy(bad,arp_request,42);bad[19]=16;CHECK(rtl_echo_reply(mac,ip,bad,42,reply,sizeof(reply),&event)==0);
    memcpy(bad,arp_request,42);bad[22]^=1;CHECK(rtl_echo_reply(mac,ip,bad,42,reply,sizeof(reply),&event)==0);
    memcpy(bad,arp_request,42);bad[41]^=1;CHECK(rtl_echo_reply(mac,ip,bad,42,reply,sizeof(reply),&event)==0);
    CHECK(rtl_echo_reply(mac,ip,arp_request,42,reply,41,&event)==0);
    printf("ECHO_PACKET_TEST PASS checks=%u input=synthetic-Ethernet no-live-ping\n",checks);return 0;
}
