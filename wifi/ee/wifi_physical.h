/* SPDX-License-Identifier: MIT. Native physical-radio client, no sample SSIDs. */
#include "rtl_wire.h"
#include "rtl_supplicant.h"
#include "rtl_dhcp.h"
#include "rtl_nonce.h"
#include "rtl_echo.h"
#include "rtl_http.h"
#include "robot_control.h"
#include "rtl_tkip_guard.h"
#include "rtl_acd.h"
enum {PH_IDLE,PH_SCAN_SET,PH_SCAN,PH_AUTH_SET,PH_AUTH,PH_ASSOC,PH_WPA,PH_IP,PH_PROBE,PH_ONLINE};
static unsigned physical_phase,physical_channel,physical_ticket,physical_radio_ready;
static uint32_t physical_deadline;static uint16_t physical_sequence;
static uint32_t physical_scan_limit;static unsigned physical_preferred_channel,physical_scan_resume;
static uint8_t physical_mac[6];static RtlScanList physical_scan;
static RtlSupplicant supplicant;static RtlDhcp dhcp;
static RtlHttp http;
static RobotControl robot;
static WifiFiles file_service;
static RtlAcd acd;static unsigned physical_tx_bytes,static_fallback,address_conflicts;
static uint32_t physical_now,reconnect_at;static unsigned reconnect_pending,reconnect_attempts;
static void (*physical_http_setup)(RtlHttp*);
static RtlWire queued;static unsigned queued_kind;
static RtlTkipGuard tkip_guard;static unsigned mic_report_pending;
static unsigned handshake_restarts;
static unsigned management_attempts,join_tx,join_rx,join_ap_rx;
static uint32_t physical_ap_seen,physical_keepalive_at;
static unsigned assoc_seen,assoc_to_us,auth_seen;
static void physical_message(const char *s){snprintf(app.message,sizeof(app.message),"%s",s);app.dirty=1;}
static void physical_stop(void){physical_phase=PH_IDLE;queued.length=0;queued_kind=0;mic_report_pending=0;rtl_supplicant_clear(&supplicant);memset(&dhcp,0,sizeof(dhcp));rtl_http_close_all(&http);file_service.upload.active=0;memset(&http,0,sizeof(http));robot.web=0;}
static void physical_failure(const char *s){unsigned i=app.active_ap;physical_stop();wifi_app_failure(&app,physical_ticket,s);
    if(i<app.scan.count&&app.known[i]&&reconnect_attempts<6){unsigned wait=1000u<<(reconnect_attempts>4?4:reconnect_attempts);reconnect_pending=1;reconnect_at=physical_now+wait;reconnect_attempts++;}}
static void physical_header(uint8_t *h,unsigned fc){
    const uint8_t *apmac=app.scan.ap[app.active_ap].bssid;unsigned seq=physical_sequence++<<4;
    memset(h,0,24);h[0]=fc;h[1]=fc>>8;memcpy(h+4,apmac,6);memcpy(h+10,physical_mac,6);memcpy(h+16,apmac,6);
    h[22]=seq;h[23]=seq>>8;
}
static void physical_auth(uint32_t now){
    physical_header(queued.bytes,0xb0);memset(queued.bytes+24,0,6);queued.bytes[26]=1;
    queued.length=30;queued_kind=1;physical_phase=PH_AUTH;physical_deadline=now+5000;
    physical_message("Access point: 802.11 authentication...");
}
static void physical_assoc(void){
    static const uint8_t rates[]={1,8,0x82,0x84,0x8b,0x96,12,18,24,36,50,4,48,72,96,108};
    uint8_t rsn[22];
    RtlAccessPoint *a=&app.scan.ap[app.selected];uint8_t *p=queued.bytes;
    if(rtl_supplicant_station_rsn(a->rsn,a->rsn_length,rsn)){physical_failure("Invalid network WPA2 information.");return;}
    physical_header(p,0);p[24]=0x11;p[25]=0;p[26]=10;p[27]=0;
    p[28]=0;p[29]=a->ssid_length;memcpy(p+30,a->ssid,a->ssid_length);
    memcpy(p+30+a->ssid_length,rates,sizeof(rates));
    memcpy(p+30+a->ssid_length+sizeof(rates),rsn,sizeof(rsn));
    queued.length=30+a->ssid_length+sizeof(rates)+sizeof(rsn);queued_kind=1;physical_phase=PH_ASSOC;
    physical_message("Sending access-point association request...");
}
static void physical_scan_start(uint32_t now){
    physical_stop();memset(&physical_scan,0,sizeof(physical_scan));physical_ticket=app.ticket;
    physical_channel=physical_preferred_channel?physical_preferred_channel:1;physical_scan_resume=physical_preferred_channel?1:0;physical_scan_limit=now+30000;physical_phase=PH_SCAN_SET;physical_deadline=now;
    physical_message("Radio ready. Starting real SSID scan...");
}
static void physical_join_start(uint32_t now){
    uint8_t nonce[32];RtlAccessPoint *ap=&app.scan.ap[app.selected];
    physical_stop();physical_ticket=app.ticket;
    app.active_ap=app.selected;static_fallback=address_conflicts=0;
    handshake_restarts=0;
    management_attempts=1;join_tx=join_rx=join_ap_rx=0;
    assoc_seen=assoc_to_us=auth_seen=0;
    if(rtl_tkip_guard_blocked(&tkip_guard,now)){physical_failure("TKIP MIC protection: retry after 60 seconds.");return;}
    {int seed_result=rtl_nonce_next("mc0:/RTL_WIFI/SEED.DAT",nonce);
    if(seed_result){
        physical_failure(rtl_nonce_error(seed_result));return;
    }
    }
    if(rtl_supplicant_start(&supplicant,app.pending_pmk,ap->bssid,physical_mac,nonce,ap->rsn,ap->rsn_length)){
        wipe(nonce,sizeof(nonce));physical_failure("Invalid WPA2-CCMP network information.");return;
    }
    wipe(nonce,sizeof(nonce));physical_channel=ap->channel;physical_phase=PH_AUTH_SET;physical_deadline=now;
    physical_message("Setting channel; preparing to join access point...");
}
static int physical_ethernet_tx(const uint8_t *p,unsigned n,unsigned kind){
    uint8_t header[24],plain[1514];int len;
    if(n<14||n>1514||queued.length)return -1;
    physical_header(header,0x0108);memcpy(header+16,p,6);
    memcpy(plain,"\xaa\xaa\x03\x00\x00\x00",6);memcpy(plain+6,p+12,n-12);
    len=rtl_ccmp_protect(&supplicant,header,plain,n-6,queued.bytes,sizeof(queued.bytes));
    if(len<0)return len;
    queued.length=len;queued_kind=kind;return 0;
}
static void physical_packet(const uint8_t *p,unsigned n,uint32_t now){
    if(app.linked&&n>=24&&app.active_ap<app.scan.count&&!memcmp(p+10,app.scan.ap[app.active_ap].bssid,6))physical_ap_seen=now;
    physical_now=now;
    const uint8_t *apmac=app.scan.ap[app.active_ap].bssid;uint8_t plain[2400],answer[1514];int len;
    app.packets_rx++;app.bytes_rx+=n;
    if(physical_phase>=PH_AUTH&&physical_phase<=PH_WPA){join_rx++;if(n>=24&&!memcmp(p+10,apmac,6)){
        join_ap_rx++;if(p[0]==0x10){assoc_seen++;if(!memcmp(p+4,physical_mac,6))assoc_to_us++;}
        if(p[0]==0xb0)auth_seen++;
    }}
    if(physical_phase==PH_SCAN){RtlAccessPoint ap;
        if(!rtl_scan_parse(p,n,physical_channel,0,&ap))rtl_scan_add(&physical_scan,&ap);
        return;
    }
    if(n<24||memcmp(p+4,physical_mac,6)||memcmp(p+10,apmac,6)){
        /* Multicast data is needed for ARP/DHCP, but management is unicast. */
        if(n<24||!(p[4]&1)||memcmp(p+10,apmac,6)||(p[0]&0x0c)!=8)return;
    }
    if(p[0]==0xc0||p[0]==0xa0){physical_failure("Access point disconnected the station.");return;}
    if(physical_phase==PH_AUTH&&p[0]==0xb0&&n>=30&&!memcmp(p+16,apmac,6)){
        if(p[24]||p[25]||p[26]!=2||p[27]||p[28]||p[29]){physical_failure("802.11 authentication rejected.");return;}
        management_attempts=1;physical_deadline=now+5000;physical_assoc();return;
    }
    if(physical_phase==PH_ASSOC&&p[0]==0x10&&n>=30&&!memcmp(p+16,apmac,6)){
        if(p[26]||p[27]){char reason[96];snprintf(reason,sizeof(reason),"ASSOC rejected; access-point status: %u",p[26]|((unsigned)p[27]<<8));physical_failure(reason);return;}
        physical_phase=PH_WPA;physical_deadline=now+20000;
        physical_message("Association accepted. Waiting for WPA2 M1/M3...");return;
    }
    if(physical_phase<PH_WPA||(p[0]!=8&&p[0]!=0x88)||(p[1]&3)!=2)return;
    {unsigned h=p[0]==0x88?26:24;
        if(p[1]&0x40){
            if(rtl_tkip_guard_blocked(&tkip_guard,now))return;
            len=rtl_wifi_unprotect(&supplicant,p,n,plain,sizeof(plain));
#ifdef RTL_WIFI_PACKET_TRACE
            printf("WIFI_DATA encryptedBytes=%u group=%u cipher=%u decryptResult=%d\n",n,!!(p[4]&1),supplicant.group_cipher,len);
#endif
            if(len==-5){mic_report_pending=rtl_tkip_guard_failure(&tkip_guard,now);
                physical_message("TKIP Michael validation failed; notifying access point.");}
            if(len<8)return;
        }
        else{if(n<h+8||n-h>sizeof(plain))return;len=n-h;memcpy(plain,p+h,len);}
    }
    if(memcmp(plain,"\xaa\xaa\x03\x00\x00\x00",6))return;
    if(plain[6]==0x88&&plain[7]==0x8e){
        if(queued.length)return;
        /* Some APs restart the uncompleted handshake with a new ANonce.
         * Obtain a fresh persistent SNonce before replying; never reuse the
         * old SNonce/PTK across that new attempt, and bound retries. */
        if(len==107&&plain[13]==0&&plain[14]==0x8a&&supplicant.state==RTL_WPA_WAIT_M3){
            int replay_order=memcmp(plain+17,supplicant.replay,8);
            if(replay_order<0)return;
            if(memcmp(plain+25,supplicant.anonce,32)){
                uint8_t fresh[32];RtlAccessPoint *a=&app.scan.ap[app.selected];int e;
                if(replay_order<=0)return;
                if(++handshake_restarts>4){physical_failure("WPA2 failed; check password in CSV.");return;}
                e=rtl_nonce_next("mc0:/RTL_WIFI/SEED.DAT",fresh);
                if(!e)e=rtl_supplicant_start(&supplicant,app.pending_pmk,a->bssid,physical_mac,fresh,a->rsn,a->rsn_length);
                wipe(fresh,sizeof(fresh));
                if(e){physical_failure("Cannot prepare secure seed for WPA2 retry.");return;}
            }
        }
#ifdef RTL_WIFI_PACKET_TRACE
        if(len>=8+99)printf("WPA2_PACKET state=%u bytes=%d version=%u descriptor=%u info=%02X%02X keylen=%u data=%u\n",supplicant.state,len-8,plain[8],plain[12],plain[13],plain[14],((unsigned)plain[15]<<8)|plain[16],((unsigned)plain[105]<<8)|plain[106]);
        if(len>=107&&plain[13]==0&&plain[14]==0x8a)printf("WPA2_M1 relation=%d sameNonce=%u\n",memcmp(plain+17,supplicant.replay,8),!memcmp(plain+25,supplicant.anonce,32));
#endif
        len=rtl_supplicant_rx(&supplicant,plain+8,len-8,answer,sizeof(answer));
#ifdef RTL_WIFI_PACKET_TRACE
        printf("WPA2_PACKET_RESULT result=%d state=%u\n",len,supplicant.state);
#endif
        if(len<0){physical_failure("WPA2 validation failed: password, MIC or replay counter.");return;}
        if(len){physical_header(queued.bytes,0x0108);memcpy(queued.bytes+24,"\xaa\xaa\x03\x00\x00\x00\x88\x8e",8);
            memcpy(queued.bytes+32,answer,len);queued.length=32+len;
            queued_kind=supplicant.state==RTL_WPA_WAIT_M4_TX?2:1;
            physical_message(supplicant.state==RTL_WPA_WAIT_M3?"WPA2 M1 received; sending M2...":"WPA2 M3 verified; sending M4...");}
        return;
    }
    if(!(p[1]&0x40)||supplicant.state!=RTL_WPA_ONLINE||len>1508)return;
    /* Reconstruct Ethernet: DA=addr1, SA=addr3, then LLC EtherType/payload. */
    memcpy(answer,p+4,6);memcpy(answer+6,p+16,6);memcpy(answer+12,plain+6,len-6);len+=6;
    if(physical_phase==PH_IP){int result=rtl_dhcp_rx(&dhcp,now,answer,len);
        if(result==2){uint8_t*target=app.static_ip[app.active_ap][0]&&!static_fallback?app.static_ip[app.active_ap]:dhcp.ip;unsigned i;int valid=1;uint32_t seed;memcpy(&seed,supplicant.snonce+8,4);
            for(i=0;i<4;i++)if((target[i]&dhcp.mask[i])!=(dhcp.ip[i]&dhcp.mask[i]))valid=0;
            if(!memcmp(target,dhcp.gateway,4)||!memcmp(target,dhcp.server,4))valid=0;
            if(!valid){physical_failure("Static IP must be distinct and on the access-point subnet.");return;}
            rtl_acd_start(&acd,physical_mac,target,now,seed);physical_phase=PH_PROBE;physical_message("Checking IP conflicts with ARP...");}
        else if(result==-2)physical_failure("DHCP server rejected address request.");
    }else if(physical_phase==PH_PROBE||physical_phase==PH_ONLINE){RtlEchoEvent ev;
        if(rtl_acd_rx(&acd,answer,len)){
            if(physical_phase==PH_PROBE&&app.static_ip[app.active_ap][0]&&!static_fallback&&memcmp(acd.ip,dhcp.ip,4)){
                uint32_t seed=acd.random;static_fallback=1;rtl_acd_start(&acd,physical_mac,dhcp.ip,now,seed);physical_message("Static IP in use. Checking DHCP address...");return;
            }
            if(physical_phase==PH_PROBE&&!queued.length&&++address_conflicts<=2){uint8_t packet[400];uint32_t xid=dhcp.xid+1;int count=rtl_dhcp_decline(&dhcp,now,packet,sizeof(packet));
                if(count>0){physical_ethernet_tx(packet,count,1);}rtl_dhcp_start(&dhcp,physical_mac,xid,now);dhcp.deadline=now+10000;physical_phase=PH_IP;static_fallback=1;physical_message("IP occupied; server notified. Waiting for another DHCP address...");return;
            }physical_failure("IP is in use by another device. Choose a different IP.");return;
        }
        if(physical_phase==PH_PROBE)return;
        {int result=rtl_dhcp_rx(&dhcp,now,answer,len);if(result==-2){physical_failure("DHCP lease renewal rejected.");return;}if(result==3){memcpy(app.mask,dhcp.mask,4);memcpy(app.gateway,dhcp.gateway,4);physical_message("IP lease renewed; HTTP remains active.");}}
        {uint32_t before=http.requests;rtl_http_rx(&http,answer,len,now);if(http.requests!=before)snprintf(app.last_request,sizeof(app.last_request),"%s",http.last_request);}
        len=rtl_echo_reply(physical_mac,dhcp.ip,answer,len,plain,sizeof(plain),&ev);
        if(len>0){if(ev.kind==RTL_ECHO_ICMP){wifi_app_echo_rx(&app,physical_ticket,ev.remote,ev.identifier,ev.sequence);snprintf(app.last_request,sizeof(app.last_request),"PING %u.%u.%u.%u",ev.remote[0],ev.remote[1],ev.remote[2],ev.remote[3]);}
            if((queued.length||physical_ethernet_tx(plain,len,ev.kind==RTL_ECHO_ICMP?3:1))&&ev.kind==RTL_ECHO_ICMP)
                wifi_app_echo_tx(&app,physical_ticket,-1);}
    }
}
static void physical_sent(int result,unsigned kind,uint32_t now){
    physical_now=now;
    if(!result){app.packets_tx++;app.bytes_tx+=physical_tx_bytes;}else app.packets_failed++;
    if(!result&&physical_phase>=PH_AUTH&&physical_phase<=PH_WPA)join_tx++;
    if(kind==3)wifi_app_echo_tx(&app,physical_ticket,result);
    if(result){physical_failure("USB packet transmission failed.");return;}
    if(kind==5){physical_failure("TKIP MIC protection: disconnected; wait 60 seconds.");return;}
    if(kind==2){rtl_supplicant_tx_done(&supplicant,1);
        if(physical_phase==PH_WPA){uint32_t xid;memcpy(&xid,supplicant.snonce,4);
            wifi_app_authenticated(&app,physical_ticket);rtl_dhcp_start(&dhcp,physical_mac,xid,now);
            memcpy(dhcp.preferred,app.last_ip[app.active_ap],4);
            physical_phase=PH_IP;physical_message("WPA2 complete. Sending DHCP address request...");}
    }
    if(physical_phase==PH_AUTH||physical_phase==PH_ASSOC)physical_deadline=now+5000;
}
static void physical_tick(uint32_t now){
    physical_now=now;
    wifi_files_tick(&file_service,now);
    if(physical_phase==PH_ONLINE){
        if((int32_t)(now-physical_ap_seen)>60000){physical_failure("No access-point data for 60 seconds; reconnecting.");return;}
        if(!queued.length&&(int32_t)(now-physical_keepalive_at)>=0&&dhcp.gateway[0]){uint8_t p[42]={0};physical_keepalive_at=now+15000;memset(p,255,6);memcpy(p+6,physical_mac,6);p[12]=8;p[13]=6;p[15]=1;p[16]=8;p[18]=6;p[19]=4;p[21]=1;memcpy(p+22,physical_mac,6);memcpy(p+28,dhcp.ip,4);memcpy(p+38,dhcp.gateway,4);physical_ethernet_tx(p,sizeof(p),1);}
    }
    if(physical_ticket!=app.ticket&&physical_phase!=PH_IDLE){physical_stop();return;}
    if((physical_phase==PH_SCAN||physical_phase==PH_SCAN_SET)&&(int32_t)(now-physical_scan_limit)>=0){physical_phase=PH_IDLE;wifi_app_scan(&app,physical_ticket,&physical_scan,0);physical_message("Scan timed out; driving remains active. L2: rescan.");if(app.error_sink)app.error_sink(app.error_context,"SSID scan reached the 30-second limit.");return;}
    if(mic_report_pending&&!queued.length&&supplicant.state==RTL_WPA_ONLINE){
        uint8_t frame[113];int len;unsigned kind=mic_report_pending==2?5:4;
        memcpy(frame,supplicant.ap,6);memcpy(frame+6,physical_mac,6);frame[12]=0x88;frame[13]=0x8e;
        len=rtl_supplicant_mic_report(&supplicant,frame+14,sizeof(frame)-14);
        if(len<0||physical_ethernet_tx(frame,len+14,kind)){physical_failure("Cannot send TKIP MIC failure report.");return;}
        mic_report_pending=0;return;
    }
    if(physical_phase==PH_SCAN&&(int32_t)(now-physical_deadline)>=0){
        if(physical_scan_resume){physical_channel=physical_scan_resume++;if(physical_channel==physical_preferred_channel)physical_channel=physical_scan_resume++;}else physical_channel++;
        if(physical_channel>13){physical_phase=PH_IDLE;wifi_app_scan(&app,physical_ticket,&physical_scan,0);}
        else physical_phase=PH_SCAN_SET;
    }
    if((physical_phase==PH_AUTH||physical_phase==PH_ASSOC||physical_phase==PH_WPA)&&
       !queued.length&&(int32_t)(now-physical_deadline)>=0){
        if((physical_phase==PH_AUTH||physical_phase==PH_ASSOC)&&management_attempts<3){
            management_attempts++;physical_deadline=now+5000;
            if(physical_phase==PH_AUTH)physical_auth(now);else physical_assoc();return;
        }
        char detail[96];const char *reason=physical_phase==PH_AUTH?"AUTH":
            physical_phase==PH_ASSOC?"ASSOC":
            supplicant.state==RTL_WPA_WAIT_M3?"No WPA2 M3; check CSV/password.":"No WPA2 M1 received (20 s).";
        if(physical_phase==PH_AUTH||physical_phase==PH_ASSOC){
            snprintf(detail,sizeof(detail),"No %s CH:%u USB-TX:%u RX:%u AP:%u AS:%u ME:%u AU:%u",reason,physical_channel,join_tx,join_rx,join_ap_rx,assoc_seen,assoc_to_us,auth_seen);
            physical_failure(detail);
        }else physical_failure(reason);return;}
    if((physical_phase==PH_IP||physical_phase==PH_ONLINE)&&!queued.length){uint8_t p[400];
        int len=rtl_dhcp_tx(&dhcp,now,p,sizeof(p));
        if(len<0){physical_failure("DHCP timeout; no IP address received.");return;}
        if(physical_phase==PH_ONLINE&&!rtl_dhcp_has_lease(&dhcp)){physical_failure("IP lease expired; reconnecting.");return;}
        if(len>0)physical_ethernet_tx(p,len,1);
    }
    if(physical_phase==PH_ONLINE&&!queued.length){uint8_t packet[1514];int len=rtl_http_tx(&http,now,packet,sizeof(packet));if(len>0)physical_ethernet_tx(packet,len,1);}
    if((physical_phase==PH_PROBE||physical_phase==PH_ONLINE)&&!queued.length){uint8_t p[42];int len=rtl_acd_tx(&acd,now,p,sizeof(p));
        if(len>0){physical_ethernet_tx(p,len,1);if(acd.ready&&physical_phase==PH_PROBE){uint32_t seed;memcpy(&seed,supplicant.snonce+8,4);
            if(memcmp(dhcp.ip,acd.ip,4)){memcpy(dhcp.ip,acd.ip,4);dhcp.lease_seconds=0xffffffffu;}
            rtl_http_init(&http,physical_mac,dhcp.ip,seed);http.control=robot_web;http.control_context=&robot;http.files=file_service.io.stat?&file_service:0;physical_phase=PH_ONLINE;physical_ap_seen=now;physical_keepalive_at=now+15000;
            if(physical_http_setup){physical_http_setup(&http);}reconnect_pending=reconnect_attempts=0;
            wifi_app_bound(&app,physical_ticket,dhcp.ip,dhcp.mask,dhcp.gateway);physical_message(static_fallback?"Static IP occupied; connected with DHCP. HTTP ready.":"Connected. HTTP server ready.");}}
    }
}
