/* Executes the actual EE connection backend against independent AP packets.
 * Only the nonce source and USB transfer completion are test doubles. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wifi_app.h"
#include "rtl_nonce.h"
#include "rtl_wpa_crypto.h"
#include "rtl_tkip.h"
#include "wifi_controls.h"
#include "physical_fixtures.h"
#include "tkip_fixtures.h"
static WifiApp app;
static unsigned nonce_calls;
static void wipe(void*p,size_t n){memset(p,0,n);}
static int public_test_nonce(const char *path,uint8_t *out){(void)path;memcpy(out,fx_sn,32);nonce_calls++;return 0;}
#define rtl_nonce_next public_test_nonce
#include "../ee/wifi_physical.h"
#undef rtl_nonce_next
#define REQUIRE(x) do{if(!(x)){printf("FLOW FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void sent(uint32_t now){unsigned kind=queued_kind;REQUIRE(queued.length>0);physical_tx_bytes=queued.length;queued.length=0;queued_kind=0;physical_sent(0,kind,now);}
static void eapol(const uint8_t*p,unsigned n,uint32_t now){uint8_t frame[1024]={0};frame[0]=8;frame[1]=2;
    memcpy(frame+4,fx_sta,6);memcpy(frame+10,fx_ap,6);memcpy(frame+16,fx_ap,6);
    memcpy(frame+24,"\xaa\xaa\x03\x00\x00\x00\x88\x8e",8);memcpy(frame+32,p,n);physical_packet(frame,n+32,now);}
static void run_flow(unsigned tkip,unsigned ip_case){uint8_t frame[30]={0};
    wifi_app_init(&app);wifi_app_driver(&app,0);physical_radio_ready=1;memcpy(physical_mac,fx_sta,6);
    physical_scan_start(0);physical_phase=PH_SCAN;physical_channel=6;
    physical_packet(tkip?fx_tkip_beacon:fx_beacon,sizeof(fx_beacon),1);REQUIRE(physical_scan.count==1);
    REQUIRE(app.state==WIFI_SCANNING&&physical_scan.ap[0].wpa2_ccmp_psk);
    physical_channel=13;physical_deadline=2;physical_tick(2);REQUIRE(app.state==WIFI_LIST&&app.scan.count==1);
    /* Exercise X on a parsed AP, not a directly assigned JOINING state.
     * Selection must open the OSK without starting association or DHCP. */
    wifi_app_take_actions(&app);wifi_app_input(&app,WIFI_X,3);wifi_app_input(&app,0,4);
    REQUIRE(app.state==WIFI_PASSWORD&&!app.password_length);
    REQUIRE(!wifi_app_take_actions(&app)&&!app.authenticated&&!app.ip[0]);
    memcpy(app.password,"password",9);app.password_length=8;
    wifi_app_input(&app,WIFI_R2,5);wifi_app_input(&app,0,6);
    REQUIRE(app.state==WIFI_DERIVING&&wifi_app_take_actions(&app)==WIFI_DO_DERIVE);
    {uint8_t pmk[32];REQUIRE(!rtl_wpa_pmk((const uint8_t*)app.password,app.password_length,
        app.scan.ap[0].ssid,app.scan.ap[0].ssid_length,pmk));
     REQUIRE(!memcmp(pmk,fx_pmk,32));wifi_app_key(&app,app.ticket,pmk,0);wipe(pmk,32);}
    REQUIRE(app.state==WIFI_JOINING&&wifi_app_take_actions(&app)==WIFI_DO_JOIN);
    if(ip_case==1||ip_case==2)memcpy(app.static_ip[0],"\xc0\x00\x02\x63",4);
    if(ip_case==3)memcpy(app.last_ip[0],"\xc0\x00\x02\x19",4);
    physical_join_start(3);
    REQUIRE(physical_phase==PH_AUTH_SET);physical_auth(3);
    /* Real asynchronous USB removes the queued packet before completion.
     * The old code expired immediately in that gap (deadline == join time). */
    queued.length=0;physical_tick(4);REQUIRE(physical_phase==PH_AUTH&&app.state==WIFI_JOINING);
    queued.length=30;sent(4);
    frame[0]=0xb0;memcpy(frame+4,fx_sta,6);memcpy(frame+10,fx_ap,6);memcpy(frame+16,fx_ap,6);frame[26]=2;
    physical_packet(frame,30,5);REQUIRE(physical_phase==PH_ASSOC);
    REQUIRE(queued.bytes[queued.length-22+7]==(tkip?2:4));sent(6);
    frame[0]=0x10;frame[26]=0;frame[28]=1;physical_packet(frame,30,7);REQUIRE(physical_phase==PH_WPA);
    eapol(fx_m1,sizeof(fx_m1),8);REQUIRE(supplicant.state==RTL_WPA_WAIT_M3);
    REQUIRE(queued.bytes[32+99+7]==(tkip?2:4));sent(9);
    eapol(tkip?fx_tkip_m3:fx_m3,tkip?sizeof(fx_tkip_m3):sizeof(fx_m3),10);REQUIRE(supplicant.state==RTL_WPA_WAIT_M4_TX);REQUIRE(!app.authenticated);sent(11);
    REQUIRE(app.authenticated&&app.state==WIFI_DHCP&&physical_phase==PH_IP);REQUIRE(!app.ip[0]);
    physical_tick(12);sent(13);physical_packet(tkip?fx_tkip_offer:fx_wire_offer,tkip?sizeof(fx_tkip_offer):sizeof(fx_wire_offer),14);
    REQUIRE(dhcp.state==RTL_DHCP_REQUEST);physical_tick(15);sent(16);
    physical_packet(tkip?fx_tkip_ack:fx_wire_ack,tkip?sizeof(fx_tkip_ack):sizeof(fx_wire_ack),17);REQUIRE(physical_phase==PH_PROBE&&!app.linked);
    if(ip_case==2){physical_packet(fx_wire_static_claim,sizeof(fx_wire_static_claim),18);REQUIRE(physical_phase==PH_PROBE&&static_fallback&&acd.ip[3]==25);}
    if(ip_case==3){physical_packet(fx_wire_lease_claim,sizeof(fx_wire_lease_claim),18);REQUIRE(physical_phase==PH_IP&&queued.length&&dhcp.state==RTL_DHCP_DISCOVER&&dhcp.deadline==10018&&!app.linked);sent(19);
        physical_tick(10017);REQUIRE(!queued.length);physical_tick(10018);sent(10019);physical_packet(fx_wire_reoffer,sizeof(fx_wire_reoffer),10020);REQUIRE(dhcp.state==RTL_DHCP_REQUEST);physical_tick(10021);sent(10022);physical_packet(fx_wire_reack,sizeof(fx_wire_reack),10023);REQUIRE(physical_phase==PH_PROBE&&acd.ip[3]==26);
    }
    {unsigned i;for(i=0;i<4;i++){uint32_t at=acd.deadline;physical_tick(at);REQUIRE(queued.length);sent(at);}}
    REQUIRE(app.state==WIFI_ONLINE&&app.linked&&app.ip[3]==(ip_case==1?99:ip_case==3?26:25)&&app.has_profile);
    if(ip_case==1||ip_case==3){physical_stop();return;}
    {WifiControls c={0};uint32_t ticket=app.ticket;void*files=http.files;wifi_controls_input(&c,&app,WIFI_START,1,20);REQUIRE(c.drive&&app.linked);wifi_controls_input(&c,&app,0,1,21);wifi_controls_input(&c,&app,WIFI_SELECT,1,22);REQUIRE(!c.drive&&app.state==WIFI_LIST&&app.linked&&app.ticket==ticket);app.scan.ap[1]=app.scan.ap[0];app.scan.ap[1].bssid[5]^=1;app.scan.count=2;app.selected=1;physical_tick(23);REQUIRE(physical_phase==PH_ONLINE&&http.files==files);}
    physical_packet(ip_case==2?fx_wire_ping4:fx_wire_ping,sizeof(fx_wire_ping),24);REQUIRE(app.ping_rx==1&&app.ping_tx==0);sent(25);REQUIRE(app.ping_tx==1);
    physical_packet(ip_case==2?fx_wire_ping4:fx_wire_ping,sizeof(fx_wire_ping),26);REQUIRE(app.ping_rx==1&&!queued.length);
    REQUIRE(app.packets_rx>app.ping_rx&&app.packets_tx>app.ping_tx&&app.bytes_rx&&app.bytes_tx);
    if(tkip){
        physical_packet(fx_tkip_badmic1,sizeof(fx_tkip_badmic1),100);REQUIRE(mic_report_pending==1&&supplicant.group_pn[16]==2);
        physical_tick(101);REQUIRE(queued_kind==4);sent(102);REQUIRE(app.linked);
        physical_packet(fx_tkip_badmic2,sizeof(fx_tkip_badmic2),200);REQUIRE(mic_report_pending==2);
        physical_tick(201);REQUIRE(queued_kind==5);sent(202);REQUIRE(app.state==WIFI_LIST&&tkip_guard.blocked);
        app.state=WIFI_JOINING;physical_join_start(203);REQUIRE(app.state==WIFI_LIST);
        REQUIRE(!rtl_tkip_guard_blocked(&tkip_guard,60200));
    }
    app.ticket++;physical_tick(21);REQUIRE(!supplicant.state&&physical_phase==PH_IDLE);
}
static void primitive_checks(void){
    static const uint8_t *const vec[]={fx_tkip_vector_0,fx_tkip_vector_1,fx_tkip_vector_2,fx_tkip_vector_3,fx_tkip_vector_4,fx_tkip_vector_5};
    static const unsigned lens[]={sizeof(fx_tkip_vector_0),sizeof(fx_tkip_vector_1),sizeof(fx_tkip_vector_2),sizeof(fx_tkip_vector_3),sizeof(fx_tkip_vector_4),sizeof(fx_tkip_vector_5)};
    static const uint8_t *const plain[]={fx_tkip_plain_0,fx_tkip_plain_1,fx_tkip_plain_2,fx_tkip_plain_3,fx_tkip_plain_4,fx_tkip_plain_5};
    static const unsigned sizes[]={sizeof(fx_tkip_plain_0),sizeof(fx_tkip_plain_1),sizeof(fx_tkip_plain_2),sizeof(fx_tkip_plain_3),sizeof(fx_tkip_plain_4),sizeof(fx_tkip_plain_5)};
    uint64_t replay[17];uint8_t out[2400],copy[2400];unsigned i,j;RtlSupplicant s;
    for(i=0;i<6;i++){
        memset(replay,0,sizeof(replay));memset(out,0xa5,sizeof(out));
        for(j=0;j<lens[i];j++)REQUIRE(rtl_tkip_group_rx(fx_tkip_gtk,fx_ap,1,replay,vec[i],j,out,sizeof(out))<0);
        REQUIRE(!replay[16]&&out[0]==0xa5);
        memcpy(copy,vec[i],lens[i]);copy[lens[i]-1]^=1;
        REQUIRE(rtl_tkip_group_rx(fx_tkip_gtk,fx_ap,1,replay,copy,lens[i],out,sizeof(out))<0&&!replay[16]);
        REQUIRE(rtl_tkip_group_rx(fx_tkip_gtk,fx_ap,1,replay,vec[i],lens[i],out,sizeof(out))==(int)sizes[i]);
        REQUIRE(!memcmp(out,plain[i],sizes[i]));
        REQUIRE(rtl_tkip_group_rx(fx_tkip_gtk,fx_ap,1,replay,vec[i],lens[i],out,sizeof(out))<0);
    }
    memset(replay,0,sizeof(replay));
    REQUIRE(rtl_tkip_group_rx(fx_tkip_gtk,fx_ap,1,replay,fx_tkip_qos,sizeof(fx_tkip_qos),out,sizeof(out))==sizeof(fx_tkip_qos_plain));
    REQUIRE(!memcmp(out,fx_tkip_qos_plain,sizeof(fx_tkip_qos_plain))&&replay[7]>0&&!replay[16]);
    REQUIRE(rtl_tkip_group_rx(fx_tkip_gtk,fx_ap,0,replay,fx_tkip_qos,sizeof(fx_tkip_qos),out,sizeof(out))<0);
    REQUIRE(!rtl_supplicant_start(&s,fx_pmk,fx_ap,fx_sta,fx_sn,fx_tkip_rsn,sizeof(fx_tkip_rsn)));
    REQUIRE(rtl_supplicant_rx(&s,fx_m1,sizeof(fx_m1),out,sizeof(out))==121);
    REQUIRE(rtl_supplicant_rx(&s,fx_tkip_m3,sizeof(fx_tkip_m3),out,sizeof(out))==99);rtl_supplicant_tx_done(&s,1);
    REQUIRE(!memcmp(s.gtk,fx_tkip_gtk,32));s.group_pn[16]=100;
    REQUIRE(rtl_supplicant_rx(&s,fx_tkip_rekey,sizeof(fx_tkip_rekey),out,sizeof(out))==99);
    REQUIRE(!memcmp(s.gtk,fx_tkip_newgtk,32)&&!s.group_pn[16]);s.group_pn[16]=200;
    REQUIRE(rtl_supplicant_rx(&s,fx_tkip_rekey,sizeof(fx_tkip_rekey),out,sizeof(out))==99&&s.group_pn[16]==200);
    REQUIRE(rtl_supplicant_rx(&s,fx_tkip_samekey,sizeof(fx_tkip_samekey),out,sizeof(out))==99&&s.group_pn[16]==200);
    REQUIRE(rtl_supplicant_rx(&s,fx_tkip_badgtk,sizeof(fx_tkip_badgtk),out,sizeof(out))<0&&s.group_pn[16]==200);
    {RtlTkipGuard g={0};REQUIRE(rtl_tkip_guard_failure(&g,0xfffffff0u)==1);
     REQUIRE(rtl_tkip_guard_failure(&g,0x10u)==2&&rtl_tkip_guard_blocked(&g,0x20u));
     REQUIRE(!rtl_tkip_guard_blocked(&g,0x10u+60000));}
    rtl_supplicant_clear(&s);
}
static void handshake_restart_checks(void){uint8_t m1[sizeof(fx_m1)];unsigned calls,i;
    physical_stop();wifi_app_init(&app);app.state=WIFI_JOINING;physical_ticket=app.ticket;
    memcpy(app.pending_pmk,fx_pmk,32);memcpy(physical_mac,fx_sta,6);
    memcpy(app.scan.ap[0].bssid,fx_ap,6);memcpy(app.scan.ap[0].rsn,fx_rsn,sizeof(fx_rsn));app.scan.ap[0].rsn_length=sizeof(fx_rsn);
    physical_join_start(0);physical_phase=PH_WPA;physical_deadline=20000;
    eapol(fx_m1,sizeof(fx_m1),1);sent(2);calls=nonce_calls;memcpy(m1,fx_m1,sizeof(m1));
    for(i=0;i<4;i++){m1[16]++;m1[17]++;eapol(m1,sizeof(m1),3+i*2);
        REQUIRE(nonce_calls==calls+i+1&&supplicant.state==RTL_WPA_WAIT_M3&&!app.authenticated);sent(4+i*2);}
    m1[16]++;m1[17]++;eapol(m1,sizeof(m1),20);REQUIRE(app.state==WIFI_LIST&&!supplicant.state&&!app.authenticated);
}
static void management_retry_checks(void){unsigned phase,i;uint8_t frame[30]={0};
    for(phase=PH_AUTH;phase<=PH_ASSOC;phase++){
        physical_stop();wifi_app_init(&app);app.state=WIFI_JOINING;physical_ticket=app.ticket;
        physical_phase=phase;physical_channel=6;physical_deadline=5000;
        management_attempts=1;join_tx=join_rx=join_ap_rx=0;
        memcpy(app.scan.ap[0].bssid,fx_ap,6);memcpy(app.scan.ap[0].rsn,fx_rsn,sizeof(fx_rsn));app.scan.ap[0].rsn_length=sizeof(fx_rsn);
        frame[0]=0x80;memcpy(frame+10,fx_ap,6);physical_packet(frame,sizeof(frame),100);
        REQUIRE(join_rx==1&&join_ap_rx==1);
        for(i=1;i<=2;i++){physical_tick(i*5000);REQUIRE(queued.length&&management_attempts==i+1&&physical_phase==phase);sent(i*5000);}
        physical_tick(15000);REQUIRE(physical_phase==PH_IDLE&&app.state==WIFI_LIST&&!queued.length);
        REQUIRE(strstr(app.message,phase==PH_AUTH?"No AUTH":"No ASSOC")&&strstr(app.message,"USB-TX:2 RX:1 AP:1"));
    }
}
static void aggregate_assoc_checks(void){
    uint8_t bulk[1024]={0},frame[2400];unsigned offset,consumed;int n;
    /* A real descriptor aggregate: beacon first, ASSOC response second.
     * The old single-frame receive path silently threw away the response. */
    bulk[0]=sizeof(fx_beacon);bulk[1]=sizeof(fx_beacon)>>8;bulk[10]=2;
    memcpy(bulk+24,fx_beacon,sizeof(fx_beacon));offset=(24+sizeof(fx_beacon)+127)&~127u;
    bulk[offset]=30;bulk[offset+24]=0x10;
    memcpy(bulk+offset+28,fx_sta,6);memcpy(bulk+offset+34,fx_ap,6);memcpy(bulk+offset+40,fx_ap,6);
    bulk[offset+52]=1; /* AID, status is zero. */
    physical_stop();wifi_app_init(&app);app.state=WIFI_JOINING;physical_ticket=app.ticket;physical_phase=PH_ASSOC;
    memcpy(physical_mac,fx_sta,6);memcpy(app.scan.ap[0].bssid,fx_ap,6);
    n=rtl_wire_rx_next(bulk,offset+54,frame,sizeof(frame),&consumed);
    REQUIRE(n==(int)sizeof(fx_beacon)&&consumed==offset);physical_packet(frame,n,10);REQUIRE(physical_phase==PH_ASSOC);
    n=rtl_wire_rx_next(bulk+consumed,54,frame,sizeof(frame),&consumed);
    REQUIRE(n==30&&consumed==54);physical_packet(frame,n,11);REQUIRE(physical_phase==PH_WPA);
    /* CRC-bad first packets advance, truncated descriptors never advance. */
    bulk[1]|=0x40;REQUIRE(rtl_wire_rx_next(bulk,offset+54,frame,sizeof(frame),&consumed)<0&&consumed==offset);
    REQUIRE(rtl_wire_rx_next(bulk+offset,53,frame,sizeof(frame),&consumed)<0&&!consumed);
    /* A rejected association is an explicit status, not a silent timeout. */
    physical_phase=PH_ASSOC;frame[26]=17;physical_packet(frame,30,12);
    REQUIRE(app.state==WIFI_LIST&&strstr(app.message,"status: 17"));
}
int physical_flow_test(void){primitive_checks();run_flow(0,0);run_flow(0,1);run_flow(0,2);run_flow(0,3);run_flow(1,0);handshake_restart_checks();management_retry_checks();aggregate_assoc_checks();
    {unsigned i;wifi_app_init(&app);wifi_app_driver(&app,0);physical_preferred_channel=6;physical_scan_start(0);REQUIRE(physical_channel==6);for(i=1;i<=13;i++){physical_phase=PH_SCAN;physical_deadline=i;physical_tick(i);if(i==1)REQUIRE(physical_channel==1);if(i==6)REQUIRE(physical_channel==7);}REQUIRE(app.state==WIFI_LIST);app.state=WIFI_SCANNING;physical_scan_start(0);physical_tick(30000);REQUIRE(physical_phase==PH_IDLE&&app.state==WIFI_LIST);physical_preferred_channel=0;}
    printf("PHYSICAL_FLOW_TEST PASS scan-auth-assoc-WPA2-DHCP-CCMP-ping; AP=synthetic group=CCMP+TKIP\n");return 0;
}
