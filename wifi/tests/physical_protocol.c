int program_test(void);
int network_ui_test(void);
int files_test(void);
#include "rtl_supplicant.h"
#include "rtl_dhcp.h"
#include "rtl_wire.h"
#include "rtl_nonce.h"
#include "physical_fixtures.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static unsigned checks;
/* Model USB short-packet framing independently of rtl_wire_tx. PS2SDK's
 * unaligned OUT pipe advertises 62 on the host while the device expects 64. */
static int usb_packet_accepts(const uint8_t*raw,unsigned bytes,unsigned host_packet){
    uint8_t assembled[4096];unsigned pos=0,used=0,accepted=0;
    while(pos<bytes){unsigned n=bytes-pos,expected;if(n>host_packet)n=host_packet;
        memcpy(assembled+used,raw+pos,n);used+=n;pos+=n;
        if(n<64){expected=used>=32?(assembled[0]|((unsigned)assembled[1]<<8))+assembled[2]+((assembled[7]>>2)&31)*8:0;
            if(used>=32&&(assembled[3]&0x8c)==0x8c&&assembled[2]==32&&expected==used)accepted++;
            used=0;
        }
    }return accepted==1;
}
extern int physical_flow_test(void);
extern int nonce_setup_test(void);
extern int wifi_known_test(void);
extern int http_test(void);
extern int robot_test(void);
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint8_t regs[65536],fuses[512];static unsigned calls,fail_call;static int timeout;
static int ctrl(void*ctx,const RtlRequest*q,uint8_t*p){unsigned i;uint32_t v=0;(void)ctx;calls++;
    if(calls==fail_call)return -4;
    if(q->type==0xc0){memcpy(p,regs+q->value,q->length);return q->length;}
    CHECK(rtl_radio_write_allowed(q->value,q->length,p));
    memcpy(regs+q->value,p,q->length);
    if(q->value==0x30){for(i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);CHECK(!(v&0x80000000u));
        regs[0x30]=fuses[(v>>8)&511];if(!timeout)regs[0x33]|=0x80;}
    return q->length;
}
static void delay(void*ctx,unsigned us){(void)ctx;(void)us;}
static void radio_reset(void){unsigned i,j,a=0;uint8_t rom[128];
    memset(regs,0,sizeof(regs));regs[0xf3]=8;memset(fuses,255,sizeof(fuses));memset(rom,0,sizeof(rom));rom[0]=0x29;rom[1]=0x81;memcpy(rom+22,fx_sta,6);
    for(i=0;i<16;i++){fuses[a++]=i<<4;for(j=0;j<8;j++)fuses[a++]=rom[i*8+j];}
    calls=fail_call=0;timeout=0;
}
int main(void){RtlSupplicant s;RtlDhcp d;RtlRadioReport r;RtlIo io={ctrl,0};uint8_t out[4096],copy[4096],before[32];unsigned i,total;int n;
    memset(copy,0,sizeof(copy));copy[0]=0xb0;
    n=rtl_wire_tx(copy,30,out,sizeof(out),64);CHECK(n==62&&usb_packet_accepts(out,n,62)&&usb_packet_accepts(out,n,64));
    copy[0]=0;for(i=31;i<=1500;i++){n=rtl_wire_tx(copy,i,out,sizeof(out),64);CHECK(n>62);CHECK(!usb_packet_accepts(out,n,62));CHECK(usb_packet_accepts(out,n,64));}
    {uint8_t phy[64];memset(phy,0,sizeof(phy));CHECK(!rtl_wire_quality(phy,sizeof(phy)));phy[3]=4;phy[2]=2;phy[28]=20;CHECK(rtl_wire_quality(phy,sizeof(phy))==101);phy[28]=64;CHECK(rtl_wire_quality(phy,sizeof(phy))==1);phy[28]=42;CHECK(rtl_wire_quality(phy,sizeof(phy))==51);phy[12]=4;phy[37]=(uint8_t)-40;CHECK(rtl_wire_quality(phy,sizeof(phy))==61);phy[2]=1;CHECK(!rtl_wire_quality(phy,sizeof(phy)));CHECK(!rtl_wire_quality(phy,24));}
    CHECK(sizeof(RtlWire)==2432&&!(sizeof(RtlWire)%64));
    printf("USB_PACKET_TEST PASS AUTH62-valid legacy-ASSOC-short-packet-rejected aligned64-long-frames-valid PHY-quality RPC64\n");
    http_test();
    robot_test();
    CHECK(!rtl_supplicant_start(&s,fx_pmk,fx_ap,fx_sta,fx_sn,fx_rsn,sizeof(fx_rsn)));
    CHECK(rtl_supplicant_rx(&s,fx_m1,sizeof(fx_m1),out,sizeof(out))==121);CHECK(s.state==RTL_WPA_WAIT_M3);
    CHECK(out[7]==0&&out[8]==0); /* RSN supplicant Key Length = 0 */
    CHECK(rtl_supplicant_rx(&s,fx_m1,sizeof(fx_m1),out,sizeof(out))==121);
    memcpy(copy,fx_m1,sizeof(fx_m1));copy[16]=2;
    CHECK(rtl_supplicant_rx(&s,copy,sizeof(fx_m1),out,sizeof(out))==121&&s.replay[7]==2);
    CHECK(!rtl_supplicant_start(&s,fx_pmk,fx_ap,fx_sta,fx_sn,fx_rsn,sizeof(fx_rsn)));
    CHECK(rtl_supplicant_rx(&s,fx_m1,sizeof(fx_m1),out,sizeof(out))==121);
    memcpy(copy,fx_m3,sizeof(fx_m3));copy[81]^=1;CHECK(rtl_supplicant_rx(&s,copy,sizeof(fx_m3),out,sizeof(out))<0);CHECK(!s.installs);
    CHECK(rtl_supplicant_rx(&s,fx_m3,sizeof(fx_m3),out,sizeof(out))==99);CHECK(s.installs==1);CHECK(s.state==RTL_WPA_WAIT_M4_TX);
    rtl_supplicant_tx_done(&s,0);CHECK(s.state==RTL_WPA_WAIT_M4_TX);rtl_supplicant_tx_done(&s,1);CHECK(s.state==RTL_WPA_ONLINE);
    CHECK(rtl_ccmp_unprotect(&s,fx_ccmp,sizeof(fx_ccmp),out,sizeof(out))==12);
    CHECK(rtl_ccmp_unprotect(&s,fx_ccmp,sizeof(fx_ccmp),out,sizeof(out))<0);CHECK(s.rx_pn[16]==1);
    s.tx_pn=123;CHECK(rtl_supplicant_rx(&s,fx_m3,sizeof(fx_m3),out,sizeof(out))==99);CHECK(s.installs==1&&s.tx_pn==123&&s.rx_pn[16]==1);
    for(i=0;i<sizeof(fx_m3);i++)CHECK(rtl_supplicant_rx(&s,fx_m3,i,out,sizeof(out))<1);
    rtl_supplicant_clear(&s);CHECK(!s.state&&!s.ptk[0]);
    rtl_dhcp_start(&d,fx_sta,0x12345678,0);CHECK(rtl_dhcp_tx(&d,0,out,sizeof(out))==342);CHECK(rtl_dhcp_tx(&d,1,out,sizeof(out))==0);
    CHECK(rtl_dhcp_rx(&d,1,fx_offer,sizeof(fx_offer))==1);CHECK(rtl_dhcp_tx(&d,1,out,sizeof(out))==342);
    memcpy(copy,fx_ack,sizeof(fx_ack));copy[46]^=1;CHECK(rtl_dhcp_rx(&d,2,copy,sizeof(fx_ack))==-1);
    CHECK(rtl_dhcp_rx(&d,2,fx_ack,sizeof(fx_ack))==2);CHECK(d.ip[3]==25&&d.state==RTL_DHCP_BOUND);
    CHECK(rtl_dhcp_tx(&d,30002,out,sizeof(out))>0&&d.state==RTL_DHCP_RENEW&&d.ip[3]==25);
    CHECK(rtl_dhcp_tx(&d,53002,out,sizeof(out))>0&&d.state==RTL_DHCP_REBIND&&d.ip[3]==25);
    CHECK(rtl_dhcp_tx(&d,60003,out,sizeof(out))<0&&!d.ip[0]);
    for(i=0;i<sizeof(fx_offer);i++){rtl_dhcp_start(&d,fx_sta,0x12345678,0);CHECK(rtl_dhcp_rx(&d,0,fx_offer,i)<0);}
    memset(copy,0,64);copy[0]=8;copy[22]=0x50;copy[23]=0x34;CHECK(rtl_wire_tx(copy,32,out,sizeof(out),64)==72);CHECK(out[2]==32&&out[7]==4&&out[6]==6);CHECK(out[14]==0x45&&out[15]==3);
    copy[1]=0x40;CHECK(rtl_wire_tx_rate(copy,32,out,sizeof(out),64,4)>0&&out[6]==5&&out[20]==4);copy[1]=0;CHECK(rtl_wire_tx_rate(copy,32,out,sizeof(out),64,4)>0&&out[6]==6&&out[20]==0);
    for(i=0;i<32;i++)CHECK(out[40+i]==copy[i]);
    radio_reset();n=rtl_radio_init(&io,delay,0,&r);if(n)printf("radio stage=%lu addr=%lx result=%d\n",(unsigned long)r.stage,(unsigned long)r.last_address,n);CHECK(!n);CHECK(r.channel==1);CHECK(!memcmp(r.mac,fx_sta,6));total=calls;
    CHECK(!rtl_radio_channel(&io,delay,0,&r,13));CHECK(r.channel==13);CHECK(rtl_radio_channel(&io,delay,0,&r,14)<0);
    regs[0x522]=255;CHECK(!rtl_radio_bssid(&io,delay,0,&r,fx_ap));
    CHECK(!memcmp(regs+0x618,fx_ap,6)&&!regs[0x522]);
    CHECK((regs[0x608]&0xc1)==0xc0); /* targeted BSS, no accept-all */
    CHECK(!rtl_radio_bssid(&io,delay,0,&r,0));CHECK((regs[0x608]&0xc1)==1);
    for(i=1;i<=total;i++){radio_reset();fail_call=i;CHECK(rtl_radio_init(&io,delay,0,&r)<0);CHECK(calls==i);}
    /* Real RTL8192CU signature accepted; the previous mistaken fixture must
     * now be rejected. Never bypass MAC/power checks to mask ROM failure. */
    radio_reset();fuses[1]=0x92;CHECK(rtl_radio_init(&io,delay,0,&r)==RTL_RADIO_BAD_ID);CHECK(r.rom[0]==0x92&&r.rom[1]==0x81);
    radio_reset();memset(fuses,255,sizeof(fuses));CHECK(rtl_radio_init(&io,delay,0,&r)==RTL_RADIO_BAD_ID);CHECK(r.rom[0]==255&&r.rom[1]==255);
    radio_reset();fuses[25]=3;CHECK(rtl_radio_init(&io,delay,0,&r)==RTL_RADIO_BAD_MAC);
    radio_reset();fuses[102]=255;CHECK(rtl_radio_init(&io,delay,0,&r)==RTL_RADIO_BAD_POWER);
    radio_reset();timeout=1;CHECK(rtl_radio_init(&io,delay,0,&r)==RTL_E_TIMEOUT);
    memset(copy,255,4);CHECK(!rtl_radio_write_allowed(0x30,4,copy));CHECK(!rtl_radio_write_allowed(0x34,1,copy));
#ifndef RTL_PROTOCOL_EE
    {FILE*f=fopen("reports/test-seed.bin","wb");CHECK(f!=0);for(i=0;i<64;i++)copy[i]=i+1;CHECK(fwrite(copy,1,64,f)==64);fclose(f);
     CHECK(!rtl_nonce_next("reports/test-seed.bin",before));CHECK(!rtl_nonce_next("reports/test-seed.bin",out));CHECK(memcmp(before,out,32));remove("reports/test-seed.bin");
     CHECK(rtl_nonce_next("reports/test-seed.bin",out)<0);}
#else
    (void)before;
#endif
    CHECK(!program_test());CHECK(!network_ui_test());CHECK(!files_test());CHECK(!wifi_known_test());CHECK(!nonce_setup_test());CHECK(!physical_flow_test());printf("PHYSICAL_PROTOCOL_TEST PASS %u checks; hardware=untested\n",checks);return 0;
}
