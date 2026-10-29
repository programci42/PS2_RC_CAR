#include "robot_program.h"
#include "rtl_dhcp.h"
#include "physical_fixtures.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define P(x) do{checks++;if(!(x)){printf("PROGRAM_TEST FAIL line=%d %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned checksum(const uint8_t*p,unsigned n){unsigned s=0,i;for(i=0;i<n;i++)s+=(i&1)?p[i]:p[i]*256u;while(s>>16)s=(s&65535)+(s>>16);return s;}
static void p32(uint8_t*p,uint32_t n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
int program_test(void){RobotProgram p,loaded;RpConfig cfg;char json[3501];int n;unsigned i,mask;RtlDhcp d;uint8_t frame[400],ack[sizeof(fx_ack)];
 rp_default(&p,0,1);n=rp_json(&p.config,json,sizeof(json));P(n>0&&!rp_parse(json,n,&cfg)&&!memcmp(&cfg,&p.config,sizeof(cfg)));
 for(i=0;i<4;i++){unsigned expected[]={5,9,6,10};p.held=0;p.target=0;P(rp_tick(&p,rp_buttons[i],1,0,1,0)==expected[i]);P(rp_tick(&p,0,1,expected[i],1,1)==0);}
 {const char*bad[]={"{}","{\"version\":2,\"bindings\":[]}","{\"version\":1,\"bindings\":[{\"button\":\"SELECT\",\"actions\":[\"on1\"]}]}","{\"version\":1,\"bindings\":[{\"button\":\"CROSS\",\"actions\":[\"on1\",\"on2\"]}]}","{\"version\":1,\"bindings\":[{\"button\":\"CROSS\",\"actions\":[\"other\"]}]}","{\"version\":1,\"bindings\":[],\"bindings\":[]}","{\"version\":1,\"bindings\":[]}junk"};for(i=0;i<sizeof(bad)/sizeof(*bad);i++)P(rp_parse(bad[i],strlen(bad[i]),&cfg)<0);}
 {const char*text="{\"version\":1,\"left\":2,\"right\":3,\"bindings\":[{\"button\":\"L1\",\"actions\":[\"on1\",\"wait\",\"off1\",\"wait\",\"on2\"]}]}";P(!rp_parse(text,strlen(text),&cfg));memset(&p,0,sizeof(p));p.config=cfg;
  P(rp_tick(&p,0x400,1,0,1,0)==1);P(rp_tick(&p,0x400,1,0,0,1000)==1&&!p.waiting);P(rp_tick(&p,0x400,1,1,1,1100)==1&&p.waiting);P(rp_tick(&p,0x400,1,1,1,1199)==1);P(rp_tick(&p,0x400,1,1,1,1200)==0);P(rp_tick(&p,0x400,1,1,0,1400)==0&&!p.waiting);P(rp_tick(&p,0x400,1,0,1,1500)==0&&p.waiting);P(rp_tick(&p,0x400,1,0,1,1599)==0);P(rp_tick(&p,0x400,1,0,1,1600)==2);P(rp_tick(&p,0,1,2,1,1601)==0);}
 rp_default(&p,0,1);P(rp_tick(&p,0x4000|0x1000,1,0,1,0)==0);P(rp_tick(&p,0x4000,0,0,1,1)==0);P(rp_tick(&p,0x4000,1,5,0,2)==0);P(rp_tick(&p,0x4000,1,0,1,3)==5);
 for(mask=0;mask<256;mask++){int safe=1;for(i=0;i<4;i++)if(((mask>>(2*i))&3)==3)safe=0;P(rp_mask_safe(mask)==safe);}
 /* Long-lived DHCP: unicast renewal retains the address and refreshes lease. */
 rtl_dhcp_start(&d,fx_sta,0x12345678,0);P(rtl_dhcp_rx(&d,1,fx_offer,sizeof(fx_offer))==1);P(rtl_dhcp_rx(&d,2,fx_ack,sizeof(fx_ack))==2);P(d.t1==30&&d.t2==53);
 P(rtl_dhcp_tx(&d,30002,frame,sizeof(frame))==342&&d.state==RTL_DHCP_RENEW&&rtl_dhcp_has_lease(&d));P(!memcmp(frame,d.server_mac,6)&&!memcmp(frame+26,d.ip,4)&&!memcmp(frame+30,d.server,4)&&!memcmp(frame+54,d.ip,4)&&checksum(frame+14,20)==65535);
 {unsigned at=282;while(frame[at]!=255){unsigned code=frame[at],len=frame[at+1];P(code!=50&&code!=54);at+=len+2;P(at<342);}}
 memcpy(ack,fx_ack,sizeof(ack));p32(ack+46,d.xid);P(rtl_dhcp_rx(&d,30003,ack,sizeof(ack))==3&&d.state==RTL_DHCP_BOUND&&d.lease_started==30003&&d.ip[3]==25);
 P(rtl_dhcp_tx(&d,60003,frame,sizeof(frame))==342&&d.state==RTL_DHCP_RENEW);P(rtl_dhcp_tx(&d,83003,frame,sizeof(frame))==342&&d.state==RTL_DHCP_REBIND&&!memcmp(frame,"\xff\xff\xff\xff\xff\xff",6)&&rtl_dhcp_has_lease(&d));
 memcpy(ack,fx_ack,sizeof(ack));p32(ack+46,d.xid);ack[290]=2;P(rtl_dhcp_rx(&d,83004,ack,sizeof(ack))==3&&d.server[3]==2&&d.state==RTL_DHCP_BOUND);
 P(rtl_dhcp_tx(&d,113004,frame,sizeof(frame))>0);memcpy(ack,fx_ack,sizeof(ack));p32(ack+46,d.xid);ack[284]=6;ack[290]=2;P(rtl_dhcp_rx(&d,113005,ack,sizeof(ack))==-2&&!rtl_dhcp_has_lease(&d)&&!d.ip[0]);
 rp_default(&loaded,2,3);P(loaded.config.left==2&&loaded.config.right==3);printf("PROGRAM_TEST PASS checks=%u JSON-actions-release-collision-wait-after-USB-ack-DHCP-T1-T2-renew-rebind-NAK\n",checks);return 0;
}
