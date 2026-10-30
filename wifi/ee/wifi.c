#include "wifi_i18n.h"
/* SPDX-License-Identifier: MIT
 * Native gamepad UI with physical USB/RF, WPA2, DHCP and ICMP backend.
 * Preview networks and leases are compiled only into the preview target. */
#define main diagnostic_main
#include "probe.c"
#undef main
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <timer.h>
#include <libpad.h>
#include <libmc.h>
#include "rtl_bringup.h"
#include "rtl_wpa_crypto.h"
#include "wifi_app.h"
#include "wifi_store.h"
#include "wifi_render.h"
#include "wifi_known.h"
#include "wifi_controls.h"
#include <malloc.h>
#include "wifi_video.h"
#ifndef RTL_WIFI_UI_PREVIEW
#include <libpwroff.h>
extern unsigned char poweroff_irx[];extern unsigned int size_poweroff_irx;
#endif
#ifndef RTL_WIFI_UI_PREVIEW
#include "wifi_seed.h"
#include "wifi_files_native.h"
#include "wifi_runtime.h"
#endif
#ifdef RTL_WIFI_UI_PREVIEW
#include "rtl_echo.h"
#include "../tests/echo_fixtures.h"
#endif
static WifiApp app;
static unsigned char pad_buffer[256] __attribute__((aligned(64)));
static volatile int pad_open,pad_initialized;static int pad_state=-1;
static volatile unsigned pad_service_request,pad_service_stage;
#ifndef RTL_WIFI_UI_PREVIEW
static unsigned char pad_service_stack[8192] __attribute__((aligned(16)));
static int pad_service_tid=-1;
#endif
static uint32_t pad_retry_at,pad_error_at;static unsigned pad_reopens;
static unsigned pad_reported_mode;
static unsigned pad_bytes,pad_raw;
static uint32_t pad_dma_stamp,pad_dma_at;
#ifndef RTL_WIFI_UI_PREVIEW
static int mc_ready;
#endif
static void wipe(void *p,size_t n){volatile unsigned char *v=p;while(n--)*v++=0;}
static uint32_t millis(void){return (uint32_t)(GetTimerSystemTime()/147456u);}
static void pad_service_step(void){unsigned request=pad_service_request;if(!request)return;pad_service_request=0;
    if(request==2){pad_service_stage=5;padPortClose(0,0);}
    if(!pad_initialized){pad_service_stage=2;pad_initialized=padInit(0)>=0;}
    if(pad_initialized){pad_service_stage=3;pad_open=padPortOpen(0,0,pad_buffer)!=0;}
    pad_dma_at=millis();pad_retry_at=pad_dma_at+3000;pad_service_stage=pad_open?4:6;
}
static void init_pad(void){
    /* Use the same extended SIO2 family as XMCMAN. Older ROMs retain the
       legacy fallback; libpad detects either RPC protocol. */
    int r=SifLoadModule("rom0:XSIO2MAN",0,0);if(r<0)SifLoadModule("rom0:SIO2MAN",0,0);
    r=SifLoadModule("rom0:XPADMAN",0,0);if(r<0)SifLoadModule("rom0:PADMAN",0,0);
    pad_service_stage=1;pad_service_request=1;pad_dma_at=millis();
#ifdef RTL_WIFI_UI_PREVIEW
    pad_service_step();
#endif
}
static uint32_t read_pad(int *connected){
    struct padButtonStatus b;int state;unsigned n;uint32_t now=millis(),stamp;
    *connected=0;
    if(!pad_open){if((int32_t)(now-pad_retry_at)>=0&&pad_service_stage!=2&&pad_service_stage!=3&&pad_service_stage!=5&&!pad_service_request){pad_retry_at=now+3000;pad_reopens++;pad_service_request=1;}
#ifdef RTL_WIFI_UI_PREVIEW
      pad_service_step();
#endif
      return 0;}
    state=padGetState(0,0);pad_state=state;
    /* Both ROM protocols publish two DMA frames, even while no button moves.
       Detect a stopped stream, rather than treating FINDCTP1 as an error:
       that state is normal for controllers without configuration support. */
    SyncDCache(pad_buffer,pad_buffer+sizeof(pad_buffer));
    stamp=*(uint32_t*)(pad_buffer)+*(uint32_t*)(pad_buffer+64)+*(uint32_t*)(pad_buffer+88)+*(uint32_t*)(pad_buffer+216);
    if(stamp!=pad_dma_stamp){pad_dma_stamp=stamp;pad_dma_at=now;}
    if((uint32_t)(now-pad_dma_at)>=3000){pad_open=0;pad_service_request=2;pad_retry_at=now+500;pad_dma_at=now;return 0;}
    if(state==PAD_STATE_ERROR){if(!pad_error_at)pad_error_at=now?now:1;if((uint32_t)(now-pad_error_at)>=3000){pad_open=0;pad_service_request=2;pad_error_at=0;pad_retry_at=now+500;}return 0;}pad_error_at=0;
    if(state!=PAD_STATE_STABLE&&state!=PAD_STATE_FINDCTP1)return 0;
    memset(&b,0xff,sizeof(b));n=padRead(0,0,&b);
    pad_bytes=n;memcpy(&pad_raw,&b,sizeof(pad_raw));
#ifdef RTL_WIFI_NATIVE_PAD_TEST
    {static uint32_t printed;if((uint32_t)(now-printed)>1000){printed=now;printf("NATIVE_PAD_FRAME state=%d n=%u ok=%u mode=%u buttons=%04x\n",state,n,b.ok,b.mode,b.btns);}}
#endif
    if(n<4||b.ok!=0||((b.mode>>4)!=4&&(b.mode>>4)!=7))return 0;
    pad_reported_mode=b.mode;*connected=1;return (uint32_t)(0xffffu^b.btns);
}
#ifndef RTL_WIFI_UI_PREVIEW
static void init_store(void){
    int r=SifLoadModule("rom0:XMCMAN",0,0);if(r<0)SifLoadModule("rom0:MCMAN",0,0);
    r=SifLoadModule("rom0:XMCSERV",0,0);if(r<0)SifLoadModule("rom0:MCSERV",0,0);
    mc_ready=mcInit(MC_TYPE_XMC)>=0;if(!mc_ready)mc_ready=mcInit(MC_TYPE_MC)>=0;
    if(mc_ready&&!wifi_store_load("mc0:/RTL_WIFI",&app.profile))app.has_profile=1;
}

#define rtl_nonce_next wifi_seed_nonce
#include "wifi_physical.h"
#include "robot_usb.h"
#include "wifi_clock.h"
#undef rtl_nonce_next
static WifiRuntime runtime;
static void runtime_error(void*ctx,const char*text){wifi_runtime_error(ctx,millis(),text);}
static int control_web(void*ctx,const char*s,uint32_t now){int result;DIntr();result=robot_web(ctx,s,now);EIntr();return result;}
static int extended_api(void*,const char*,const char*,const char*,const uint8_t*,unsigned,uint32_t,char*,unsigned);
static void setup_http(RtlHttp*h){h->api=extended_api;h->api_context=&runtime;h->error_sink=runtime_error;h->single_peer=1;h->control=control_web;}

#include "usb_inventory.h"
static union {RtlReport usb;RtlBootReport boot;RtlRadioReport radio;RtlWire wire;UsbInventoryDevice inventory;} response __attribute__((aligned(64)));
static unsigned int pending,usb_generation,sending_kind,sending_ticket;static uint32_t poll_at,rpc_deadline;static int rpc_fault;
static UsbInventoryDevice hub_last;static unsigned hub_power_wanted;static unsigned known_tried;static uint32_t hub_check_at;static unsigned hub_cursor=1,hub_powered_ports;
static unsigned recovery_wanted,recovery_attempts;static uint32_t recovery_at;
static uint32_t offline_usb_at;static unsigned offline_usb_wanted,offline_usb_checks,offline_usb_present;
static uint8_t known_cache[4096];static int known_cache_n;static unsigned fast_scan=1,fast_count;
static unsigned drive_mode;static WifiControls controls;
static unsigned ip_save_pending,ip_save_index;static uint32_t ip_save_after;
static volatile unsigned control_ready,control_running,control_buttons,control_connected,control_menu,control_reconnect,display_reset_pending;
static unsigned char control_stack[16384] __attribute__((aligned(16)));
static unsigned char video_stack[16384] __attribute__((aligned(16)));
static unsigned video_running;static int control_tid=-1,video_tid=-1;
static unsigned control_draws;
extern void *_gp;
static char control_error_text[160];static volatile unsigned control_error_pending;
static void control_error_sink(void*ctx,const char*s){(void)ctx;if(!control_error_pending){snprintf(control_error_text,sizeof(control_error_text),"%s",s);control_error_pending=1;}}
static void drive_input(uint32_t buttons,int connected,uint32_t now){unsigned d=0;
    static uint32_t last_buttons;unsigned old_ready=robot.ready,old_applied=robot.applied,old_owner=robot.owner;
    if(drive_mode&&last_buttons!=buttons)app.dirty=1;
    last_buttons=buttons;
    if(buttons&(PAD_SELECT|PAD_START)){robot.web=robot.timed=0;robot.halt=1;}
    if(drive_mode)d=buttons&0xfff6u;
    if(controls.display_latched||controls.reconnect_latched){d=0;robot.web=robot.timed=0;robot.halt=1;}
    robot.program_mask=rp_tick(&runtime.program,d,connected&&drive_mode&&!robot.halt&&!wifi_files_busy(&file_service),robot.applied,robot_sync&&!robot_pending&&!robot_reset,now);
    robot_usb_tick(d,connected,app.linked,now);
    if(buttons&PAD_SELECT){robot.web=0;robot.halt=1;robot.desired=0;}
    if(drive_mode&&(old_ready!=robot.ready||old_applied!=robot.applied||old_owner!=robot.owner))app.dirty=1;
}
static void control_step(void){
    static uint32_t last;int connected;uint32_t now=millis(),buttons=read_pad(&connected);
    if(last&&(uint32_t)(now-last)>runtime.pad_max_gap_ms)runtime.pad_max_gap_ms=now-last;
    last=now;runtime.pad_ticks++;runtime.pad_thread=control_running;runtime.pad_state=pad_state;runtime.pad_port_open=pad_open;runtime.pad_reopens=pad_reopens;runtime.pad_init_stage=pad_service_stage;runtime.pad_bytes=pad_bytes;runtime.pad_raw=pad_raw;runtime.pad_dma=pad_dma_stamp;
    control_buttons=buttons;control_connected=connected;
    control_menu=wifi_controls_input(&controls,&app,buttons,connected,now);drive_mode=controls.drive;
    if(runtime.display_requested){if(!controls.display_changed){controls.display=runtime.display_enabled;controls.display_changed=1;}runtime.display_requested=0;}
    if(controls.display_changed){*(volatile uint64_t*)0x12000000=wifi_video_pmode(controls.display);if(controls.display){display_reset_pending=1;app.dirty=1;}runtime.display_toggles++;}
    runtime.display_enabled=controls.display;runtime.pad_connected=connected;runtime.pad_buttons=buttons;
    if(controls.reconnect)control_reconnect=1;
    if(control_ready)drive_input(buttons,connected,now);
}
static void video_step(void){uint32_t now=millis(),buttons=control_buttons;int connected=control_connected;
    static unsigned last_language=2;if(last_language!=wifi_language){last_language=wifi_language;display_reset_pending=1;app.dirty=1;}
    /* Separate owner for GIF/font state; pad priority remains above video. */
    {static uint32_t draw_at;if(controls.display&&(app.dirty||display_reset_pending)&&(uint32_t)(now-draw_at)>=100){draw_at=now;
      if(display_reset_pending){display_reset_pending=0;wifi_render_reset();}
      if(drive_mode)wifi_draw_drive(&app,buttons,connected,robot.ready,robot.left,robot.right,robot.applied,robot.owner,robot.desired);
      else wifi_draw(&app,0,connected);
      app.dirty=0;control_draws++;}}
}
static void control_thread(void*arg){(void)arg;for(;;){control_step();DelayThread(10000);}}
static void video_thread(void*arg){(void)arg;for(;;){video_step();DelayThread(100000);}}
static void pad_service_thread(void*arg){(void)arg;for(;;){pad_service_step();DelayThread(10000);}}
static void control_start(void){ee_thread_t t;int id;ChangeThreadPriority(GetThreadId(),64);memset(&t,0,sizeof(t));t.func=control_thread;t.stack=control_stack;t.stack_size=sizeof(control_stack);t.gp_reg=&_gp;t.initial_priority=32;
    id=CreateThread(&t);if(id>=0){control_tid=id;control_running=StartThread(id,0)>=0;if(!control_running){DeleteThread(id);control_tid=-1;}}
    t.func=video_thread;t.stack=video_stack;t.stack_size=sizeof(video_stack);t.initial_priority=48;
    id=CreateThread(&t);if(id>=0){video_tid=id;video_running=StartThread(id,0)>=0;if(!video_running){DeleteThread(id);video_tid=-1;}}
    if(pad_service_tid<0){t.func=pad_service_thread;t.stack=pad_service_stack;t.stack_size=sizeof(pad_service_stack);t.initial_priority=56;
      id=CreateThread(&t);if(id>=0){pad_service_tid=id;if(StartThread(id,0)<0){DeleteThread(id);pad_service_tid=-1;}}}}
#include "background_job.h"
#include "device_inventory.h"
static void platform_stop_workers(void){control_ready=0;job_stop(5);if(control_tid>=0){TerminateThread(control_tid);DeleteThread(control_tid);control_tid=-1;}if(video_tid>=0){TerminateThread(video_tid);DeleteThread(video_tid);video_tid=-1;}if(pad_service_tid>=0){TerminateThread(pad_service_tid);DeleteThread(pad_service_tid);pad_service_tid=-1;}control_running=video_running=0;display_reset_pending=1;}
static void platform_checkpoint(const char*s){uint32_t now=millis();runtime_error(&runtime,s);runtime.flush_at=now-1001;wifi_runtime_tick(&runtime,now);}
#ifdef RTL_WIFI_NATIVE_PAD_TEST
static int request(unsigned int,uint32_t);
static void offline_tick(uint32_t);
static void offline_probe_done(unsigned,uint32_t);
#include "../tests/native_jobs.h"
#endif
static int extended_api(void*ctx,const char*m,const char*t,const char*h,const uint8_t*b,unsigned n,uint32_t now,char*out,unsigned cap){int code;
 if(!strcmp(t,"/network")){char message[576];device_quote(message,sizeof(message),app.message);snprintf(out,cap,"{\"linked\":%u,\"state\":%u,\"phase\":%u,\"radioReady\":%u,\"recoveryAttempts\":%u,\"autoUsbIntervalMs\":30000,\"autoUsbChecks\":%u,\"autoUsbWifiPresent\":%u,\"hubId\":%u,\"hubConfiguration\":%u,\"hubConfigResult\":%d,\"hubResult\":%d,\"hubPoweredMask\":%u,\"hubPortResult\":[%d,%d,%d,%d],\"message\":\"%s\"}",app.linked,app.state,physical_phase,physical_radio_ready,recovery_attempts,offline_usb_checks,offline_usb_present,hub_last.id,hub_last.config_total,hub_last.location_result,hub_last.hub_result,hub_powered_ports,hub_last.hub_port_result[0],hub_last.hub_port_result[1],hub_last.hub_port_result[2],hub_last.hub_port_result[3],message);return 200;}
 if(!strcmp(t,"/hub/power")){if(strcmp(m,"POST")||!job_auth(h)||strstr(h,"cross-site"))return 403;if(robot.applied||robot.desired||wifi_files_busy(&file_service))return 409;hub_power_wanted=1;hub_check_at=0;snprintf(out,cap,"{\"hubPowerRequested\":true}");return 200;}
 if(!strcmp(t,"/relay/recover")){if(strcmp(m,"POST")||!job_auth(h)||strstr(h,"cross-site"))return 403;if(robot.ready||robot.applied||robot.desired)return 409;robot_recover_wanted=1;snprintf(out,cap,"{\"relayRecoveryRequested\":true}");return 200;}
 code=devices_http(m,t,out,cap);if(code)return code;code=job_http(m,t,h,b,n,now,out,cap);return code?code:wifi_runtime_api(ctx,m,t,h,b,n,now,out,cap);}
static void try_known(void){
    static uint8_t csv[4096];uint8_t pmk[32];unsigned selected;int n,result;
    if(app.state==WIFI_SCANNING){known_tried=0;return;}
    if(app.linked||app.state!=WIFI_LIST||!app.scan.count||known_tried)return;
    known_tried=1;n=wifi_seed_read_known(csv,sizeof(csv));
    if(n>0){uint8_t flags[RTL_SCAN_CAPACITY];unsigned i;
        memcpy(known_cache,csv,n);known_cache_n=n;
        if(wifi_known_mark(csv,n,&app.scan,flags)>=0)for(i=0;i<app.scan.count;i++)if(flags[i])app.known[i]|=WIFI_KNOWN_CSV;
        for(i=0;i<app.scan.count;i++)wifi_known_config(csv,n,app.scan.ap[i].ssid,app.scan.ap[i].ssid_length,app.last_ip[i],app.static_ip[i]);
        app.dirty=1;
    }
    result=n>0?wifi_known_select(csv,n,&app.scan,&selected,pmk):0;if(result==0&&app.has_profile){unsigned i;for(i=0;i<app.scan.count;i++)if(app.scan.ap[i].wpa2_ccmp_psk&&app.scan.ap[i].ssid_length==app.profile.ssid_length&&!memcmp(app.scan.ap[i].ssid,app.profile.ssid,app.profile.ssid_length)){selected=i;memcpy(pmk,app.profile.pmk,32);result=1;break;}}
    wipe(csv,sizeof(csv));
    if(result==1){app.selected=selected;app.state=WIFI_DERIVING;wifi_app_key(&app,app.ticket,pmk,0);
        snprintf(app.message,sizeof(app.message),WIFI_TR("Known network found; connecting automatically..."));app.dirty=1;}
    else physical_message(n<0?WIFI_TR("Cannot read bilinenaglar.csv. Check file/card."):
        n==0?WIFI_TR("bilinenaglar.csv missing or empty. X: connect manually"):
        result<0?WIFI_TR("Invalid CSV; use UTF-8 SSID;Password rows."):
        WIFI_TR("CSV loaded; no matching SSID (case sensitive). X: select"));
    wipe(pmk,sizeof(pmk));
}
static void join_selected_known(void){
    static uint8_t csv[4096];static RtlScanList selected_scan;uint8_t pmk[32];unsigned selected;int n,result;
    if(app.state!=WIFI_DERIVING||app.selected>=app.scan.count)return;
    selected_scan.count=1;selected_scan.ap[0]=app.scan.ap[app.selected];
    n=wifi_seed_read_known(csv,sizeof(csv));result=n>0?wifi_known_select(csv,n,&selected_scan,&selected,pmk):0;
    wipe(csv,sizeof(csv));
    if(result==1)wifi_app_key(&app,app.ticket,pmk,0);
    else wifi_app_failure(&app,app.ticket,WIFI_TR("Cannot read saved password. Press X to enter it."));
    wipe(pmk,sizeof(pmk));
}
static int request(unsigned int command,uint32_t now){
    unsigned int size=(command==RTL_RPC_DEVICE||command==RTL_RPC_HUB_POWER||command==RTL_RPC_USB_CHECK)?sizeof(UsbInventoryDevice):command==RTL_RPC_RECOVER?sizeof(RtlReport):(command==RTL_RPC_BOOT?sizeof(RtlBootReport):(command==RTL_RPC_RADIO?sizeof(RtlRadioReport):(command>=RTL_RPC_CHANNEL?sizeof(RtlWire):sizeof(RtlReport))));
    if(pending||rpc_fault)return -1;
    memset(&response,0,sizeof(response));
    if(command==RTL_RPC_DEVICE)response.inventory.id=devices_cursor;
    if(command==RTL_RPC_HUB_POWER){response.inventory.id=hub_power_wanted?1:hub_cursor;response.inventory.driver_flags=hub_power_wanted?0x80000000u:0;hub_power_wanted=0;}
    if(command>=RTL_RPC_CHANNEL&&command<=RTL_RPC_TX)response.wire.result=RTL_E_BUSY;
    if(command==RTL_RPC_CHANNEL)response.wire.channel=physical_channel;
    if(command==RTL_RPC_CHANNEL&&physical_phase==PH_AUTH_SET){response.wire.length=6;memcpy(response.wire.bytes,app.scan.ap[app.selected].bssid,6);}
    if(command==RTL_RPC_TX){response.wire=queued;response.wire.quality=0;physical_tx_bytes=queued.length;sending_kind=queued_kind;sending_ticket=app.ticket;queued.length=0;queued_kind=0;}
    if(SifCallRpc(&client,command,SIF_RPC_M_NOWAIT,&response,size,&response,size,0,0)<0)return -1;
    /* An HTTP request may have spent seconds in MC verification. Its loop
       timestamp is stale: time the RPC from actual dispatch, not that loop. */
    pending=command;now=millis();rpc_deadline=now+((command==RTL_RPC_BOOT||command==RTL_RPC_RADIO)?120000:(command==RTL_RPC_DEVICE||command==RTL_RPC_HUB_POWER)?5000:2000);return 0;
}
static void offline_tick(uint32_t now){
    if(app.linked){offline_usb_at=now+30000;offline_usb_wanted=0;return;}
    if(!offline_usb_at){offline_usb_at=now+30000;return;}
    if((int32_t)(now-offline_usb_at)>=0){offline_usb_at=now+30000;offline_usb_wanted=1;}
}
static void offline_probe_done(unsigned present,uint32_t now){
    offline_usb_checks++;offline_usb_present=present;
    if(app.linked||!present)return;
    if(!physical_radio_ready){if((int32_t)(now-recovery_at)>=0)recovery_wanted=1;return;}
    /* Never restart an association, WPA/DHCP exchange, or password editor. */
    if(app.state!=WIFI_LIST&&app.state!=WIFI_ERROR)return;
    known_tried=0;app.ticket++;app.state=WIFI_SCANNING;app.actions=WIFI_DO_SCAN;
    fast_scan=1;fast_count=0;physical_message(WIFI_TR("USB Wi-Fi found; scanning known networks..."));
}
static void usb_poll(uint32_t now){
    unsigned int done;
    if(!client.server)return;
    /* A timed-out SIF buffer remains owned by the old RPC until completion. */
    if(rpc_fault){if(pending&&SifCheckStatRpc(&client))return;pending=0;rpc_fault=0;recovery_wanted=1;}
    if(app.linked){recovery_attempts=0;recovery_at=now+5000;}
    if(recovery_wanted&&!pending){
        recovery_wanted=0;recovery_attempts++;recovery_at=now+30000;
        physical_stop();physical_radio_ready=0;known_tried=0;fast_scan=1;fast_count=0;
        app.actions=0;wifi_app_disconnected(&app,WIFI_TR("Re-detecting USB Wi-Fi; gamepad control remains available."));
        if(request(RTL_RPC_RECOVER,now))rpc_fault=1;
        return;
    }
    if(pending){
        if(SifCheckStatRpc(&client)){
            if((uint32_t)(now-rpc_deadline)<0x80000000u){char detail[96];rpc_fault=1;physical_stop();snprintf(detail,sizeof(detail),WIFI_TR("Wi-Fi RPC timeout: cmd=%u channel=%u; gamepad stays active."),pending,physical_channel);wifi_app_disconnected(&app,detail);}
            return;
        }
        done=pending;pending=0;
        if(done==RTL_RPC_USB_CHECK){offline_probe_done(response.inventory.present,now);
        }else if(done==RTL_RPC_DEVICE){if(response.inventory.id<USB_INVENTORY_MAX)devices[response.inventory.id]=response.inventory;if(++devices_cursor>=USB_INVENTORY_MAX)devices_scanning=0;devices_due=now+100;
        }else if(done==RTL_RPC_HUB_POWER){if(response.inventory.present){hub_last=response.inventory;snprintf(app.hub_message,sizeof(app.hub_message),"#%u cfg:%u rc:%d P1:%04X P2:%04X P3:%04X P4:%04X",hub_last.id,hub_last.config_total,hub_last.hub_result,usb_word(hub_last.hub_ports[0]),usb_word(hub_last.hub_ports[1]),usb_word(hub_last.hub_ports[2]),usb_word(hub_last.hub_ports[3]));app.dirty=1;}else if(hub_cursor==1){memset(&hub_last,0,sizeof(hub_last));snprintf(app.hub_message,sizeof(app.hub_message),WIFI_TR("No hub detected; monitoring direct USB devices."));app.dirty=1;}hub_powered_ports|=response.inventory.driver_flags;hub_cursor=response.inventory.id+1;if(hub_cursor>=USB_INVENTORY_MAX){hub_cursor=1;hub_check_at=now+3000;}
        }else if(done==RTL_RPC_BOOT){
            if(response.boot.magic==RTL_BOOT_MAGIC&&!response.boot.result&&response.boot.stage==RTL_BOOT_COMPLETE){
                app.boot_flags|=4;
                physical_message(WIFI_TR("Firmware running. Loading radio settings..."));
                if(request(RTL_RPC_RADIO,now))wifi_app_driver(&app,-1);
                return;
            }else wifi_app_driver(&app,-1);
        }else if(done==RTL_RPC_RADIO){
            if(response.radio.result){char msg[96];snprintf(msg,sizeof(msg),WIFI_TR("Radio: stage %lu code %ld reg %04lX ROM %02X%02X"),(unsigned long)response.radio.stage,(long)response.radio.result,(unsigned long)response.radio.last_address,response.radio.rom[1],response.radio.rom[0]);wifi_app_driver(&app,-1);physical_message(msg);}
            else{app.boot_flags|=8;memcpy(physical_mac,response.radio.mac,6);physical_radio_ready=1;wifi_app_driver(&app,0);}
        }else if(done==RTL_RPC_CHANNEL){
            if(response.wire.result)physical_failure(WIFI_TR("Cannot set radio channel."));
            else if(physical_phase==PH_SCAN_SET){physical_phase=PH_SCAN;physical_deadline=now+350;char msg[96];snprintf(msg,sizeof(msg),WIFI_TR("SSID scan: channel %u/13, networks found %u"),physical_channel,physical_scan.count);physical_message(msg);}
            else if(physical_phase==PH_AUTH_SET)physical_auth(now);
        }else if(done==RTL_RPC_RX){
            if(response.wire.result){physical_radio_ready=0;physical_stop();wifi_app_disconnected(&app,WIFI_TR("USB receive error / device disconnected."));}
            else if(response.wire.length&&response.wire.length<=RTL_WIRE_MAX){if(response.wire.quality&&response.wire.quality<=101&&app.linked&&response.wire.length>=24&&!memcmp(response.wire.bytes+10,app.scan.ap[app.active_ap].bssid,6)){app.signal_valid=1;app.signal_quality=response.wire.quality-1;}physical_packet(response.wire.bytes,response.wire.length,now);}
        }else if(done==RTL_RPC_TX){if(sending_ticket==app.ticket)physical_sent(response.wire.result,sending_kind,now);
        }else if(response.usb.magic!=RTL_RPC_MAGIC){rpc_fault=1;wifi_app_disconnected(&app,WIFI_TR("Invalid USB status response."));}
        else if(response.usb.state==RTL_READY){
            if(app.state==WIFI_BOOT){app.boot_flags=3;physical_message(WIFI_TR("Wi-Fi attached. Driver ready; loading firmware..."));usb_generation=response.usb.generation;if(request(RTL_RPC_BOOT,now))wifi_app_driver(&app,-1);return;}
            if(usb_generation!=response.usb.generation){physical_stop();physical_radio_ready=0;app.boot_flags=1;wifi_app_disconnected(&app,WIFI_TR("USB adapter reconnected."));}
        }else if(app.state!=WIFI_BOOT){physical_stop();physical_radio_ready=0;app.boot_flags=1;wifi_app_disconnected(&app,WIFI_TR("USB adapter removed. Connect the adapter."));}
        else if(response.usb.state==RTL_WAITING&&strcmp(app.message,WIFI_TR("Waiting for USB adapter. Connect the adapter."))){
            snprintf(app.message,sizeof(app.message),WIFI_TR("Waiting for USB adapter. Connect the adapter."));app.dirty=1;
        }
    }
    physical_tick(now);
    if(!app.linked&&offline_usb_wanted&&!pending&&!queued.length){if(!request(RTL_RPC_USB_CHECK,now)){offline_usb_wanted=0;return;}}
    if(!pending&&!queued.length&&(int32_t)(now-hub_check_at)>=0){if(!request(RTL_RPC_HUB_POWER,now))return;}
    if(!pending&&devices_scanning&&(int32_t)(now-devices_due)>=0&&!queued.length){if(!request(RTL_RPC_DEVICE,now))return;}
    if(fast_scan&&physical_phase==PH_SCAN&&physical_scan.count!=fast_count){uint8_t flags[RTL_SCAN_CAPACITY];unsigned found=0,i;fast_count=physical_scan.count;
        if(known_cache_n>0&&wifi_known_mark(known_cache,known_cache_n,&physical_scan,flags)>0)found=1;
        if(app.has_profile)for(i=0;i<physical_scan.count;i++)if(physical_scan.ap[i].wpa2_ccmp_psk&&physical_scan.ap[i].ssid_length==app.profile.ssid_length&&!memcmp(physical_scan.ap[i].ssid,app.profile.ssid,app.profile.ssid_length))found=1;
        if(found){physical_phase=PH_IDLE;fast_scan=0;wifi_app_scan(&app,app.ticket,&physical_scan,0);}}
    if(fast_scan&&app.state==WIFI_LIST)fast_scan=0;
    if(!pending&&physical_radio_ready){
        if(queued.length){if(request(RTL_RPC_TX,now))physical_failure(WIFI_TR("Cannot start USB transmit request."));return;}
        if(physical_phase==PH_SCAN_SET||physical_phase==PH_AUTH_SET){if(request(RTL_RPC_CHANNEL,now))physical_failure(WIFI_TR("Cannot start channel request."));return;}
        if((int32_t)(now-poll_at)<0){if(request(RTL_RPC_RX,now))physical_failure(WIFI_TR("Cannot start USB receive request."));return;}
    }
    if(!pending&&(uint32_t)(now-poll_at)<0x80000000u){poll_at=now+1000;if(request(RTL_RPC_STATUS,now)){rpc_fault=1;wifi_app_disconnected(&app,WIFI_TR("Cannot start USB communication request."));}}
}
static int init_usb(void){
    int tick;
    for(tick=0;tick<500&&!SifIopReset("",0);tick++)DelayThread(10000);
    if(tick==500)return -1;
    for(tick=0;tick<500&&!SifIopSync();tick++)DelayThread(10000);
    if(tick==500)return -1;
    SifInitRpc(0);SifLoadFileInit();SifInitIopHeap();sbv_patch_enable_lmb();sbv_patch_disable_prefix_check();
    {static const char args[]="conf=2048\0hub=8\0port=8\0";int result=0;
     if(SifExecModuleBuffer(usbd_irx,size_usbd_irx,sizeof(args),args,&result)<0||result<0||load(rtl_probe_irx,size_rtl_probe_irx))return -1;}
    memset(&client,0,sizeof(client));
    for(tick=0;tick<100;tick++){
        if(SifBindRpc(&client,RTL_RPC_ID,SIF_RPC_M_NOWAIT)<0||wait_rpc())return -1;
        if(client.server)return 0;
        DelayThread(10000);
    }
    return -1;
}
#else
static uint32_t preview_at;static unsigned int preview_phase;
static void preview_scan(void){
    RtlScanList list;const char *names[]={"PS2 Lab","Atolye","Misafir"};unsigned int i;
    memset(&list,0,sizeof(list));list.count=3;
    for(i=0;i<3;i++){list.ap[i].ssid_length=(uint8_t)strlen(names[i]);memcpy(list.ap[i].ssid,names[i],list.ap[i].ssid_length);list.ap[i].channel=(uint8_t)(i*5+1);list.ap[i].wpa2_ccmp_psk=i<2;list.ap[i].bssid[0]=2;list.ap[i].bssid[5]=(uint8_t)i;}
    wifi_app_scan(&app,app.ticket,&list,0);
}
static void preview_join(uint32_t now){
    uint8_t expected[32];RtlAccessPoint *ap=&app.scan.ap[app.selected];
    /* Public preview password. Not the user's private WiFi credential. */
    if(rtl_wpa_pmk((const uint8_t*)"password",8,ap->ssid,ap->ssid_length,expected)||memcmp(expected,app.pending_pmk,32)){
        wifi_app_failure(&app,app.ticket,WIFI_TR("PREVIEW: wrong password. Sample password: password"));preview_phase=0;
    }else{preview_at=now+900;preview_phase=1;}
    wipe(expected,sizeof(expected));
}
static void preview_poll(uint32_t now){
    static const uint8_t ip[4]={192,0,2,25},mask[4]={255,255,255,0},gw[4]={192,0,2,1};
    if(!preview_phase||(uint32_t)(now-preview_at)>=0x80000000u)return;
    if(preview_phase==1&&app.state==WIFI_JOINING){wifi_app_authenticated(&app,app.ticket);preview_phase=2;preview_at=now+900;}
    else if(preview_phase==2&&app.state==WIFI_DHCP){wifi_app_bound(&app,app.ticket,ip,mask,gw);preview_phase=0;}
    else preview_phase=0;
}
static void preview_ping(void){
    static const uint8_t mac[6]={2,0x50,0x53,0x32,0,0x25};uint8_t reply[1514];RtlEchoEvent e;
    if(app.state!=WIFI_ONLINE)return;
    if(rtl_echo_reply(mac,app.ip,ping_request,sizeof(ping_request),reply,sizeof(reply),&e)>0&&e.kind==RTL_ECHO_ICMP){
        wifi_app_echo_rx(&app,app.ticket,e.remote,e.identifier,e.sequence);wifi_app_echo_tx(&app,app.ticket,0);
        snprintf(app.message,sizeof(app.message),WIFI_TR("PREVIEW: synthetic ping processed; no real network packet."));app.dirty=1;
    }
}
#endif
static void actions(uint32_t now){
    unsigned int action=wifi_app_take_actions(&app);uint8_t pmk[32];RtlAccessPoint *ap;
#ifndef RTL_WIFI_UI_PREVIEW
    if(action&WIFI_DO_IPSAVE){ip_save_pending=1;ip_save_index=app.selected;ip_save_after=now;}
    if(action&WIFI_DO_KNOWN)join_selected_known();
#endif
    if(action&WIFI_DO_DERIVE){
        ap=&app.scan.ap[app.selected];int result=rtl_wpa_pmk((const uint8_t*)app.password,app.password_length,ap->ssid,ap->ssid_length,pmk);
        wifi_app_key(&app,app.ticket,pmk,result);wipe(pmk,sizeof(pmk));
    }
    
#ifndef RTL_WIFI_UI_PREVIEW
    if(action&WIFI_DO_LEAVE)physical_stop();
#endif
    if(action&WIFI_DO_SCAN){
#ifdef RTL_WIFI_UI_PREVIEW
        preview_scan();
#else
        physical_scan_start(now);
#endif
    }
    if(action&WIFI_DO_JOIN){
#ifdef RTL_WIFI_UI_PREVIEW
        preview_join(now);
#else
        physical_join_start(now);
#endif
    }
    if(action&WIFI_DO_SAVE){
#ifdef RTL_WIFI_UI_PREVIEW
        snprintf(app.message,sizeof(app.message),WIFI_TR("PREVIEW: saved in RAM; memory card unchanged."));app.dirty=1;
#else
        int result=-1;if(mc_ready){mkdir("mc0:/RTL_WIFI",0777);result=wifi_store_save("mc0:/RTL_WIFI",&app.profile);}wifi_app_saved(&app,result);if(result)runtime_error(&runtime,WIFI_TR("Cannot save network profile to memory card."));
        ip_save_pending=1;ip_save_index=app.active_ap;ip_save_after=now;
#endif
    }
#ifndef RTL_WIFI_UI_PREVIEW
    if(ip_save_pending&&!robot.applied&&!robot.desired&&!robot.pad&&!robot.web&&!file_service.upload.active&&!file_service.readers&&(int32_t)(now-ip_save_after)>=0){char path[256];unsigned i=ip_save_index;ip_save_pending=0;
        if(wifi_runtime_channel(&runtime,app.scan.ap[i].channel)){runtime_error(&runtime,WIFI_TR("Cannot save last Wi-Fi channel."));}physical_preferred_channel=runtime.last_channel;
        if(wifi_seed_known_path(path,sizeof(path))||wifi_known_save(&file_service,path,app.scan.ap[i].ssid,app.scan.ap[i].ssid_length,app.last_ip[i],app.static_ip[i])){physical_message(WIFI_TR("Cannot save IP to CSV; connection remains active."));runtime_error(&runtime,app.message);}
    }
#endif
    (void)now;
}
int main(int argc,char **argv){
    int connected=0,previous_connected=0;uint32_t buttons,now;
#ifdef RTL_WIFI_TRACE
    unsigned int trace_frames=0;uint32_t trace_last_buttons=0;
#endif
#ifdef RTL_WIFI_UI_PREVIEW
    uint32_t previous_buttons=0;
#endif
    wifi_app_init(&app);
#ifdef RTL_WIFI_UI_PREVIEW
    init_scr();wifi_draw(&app,1,0);
#else
    /* Initialize video only before SIF/PAD are active. init_scr resets DMA;
       doing it on a live toggle could interrupt USB/gamepad RPC traffic. */
    init_scr();*(volatile uint64_t*)0x12000000=wifi_video_pmode(0);wifi_controls_init(&controls);drive_mode=1;
#endif
    SifInitRpc(0);
#ifndef RTL_WIFI_UI_PREVIEW
    if(init_usb()){wifi_app_driver(&app,-1);rpc_fault=1;}
    else{app.boot_flags=1;app.dirty=1;}
#else
    SifLoadFileInit();SifInitIopHeap();
#endif
    init_pad();
#ifndef RTL_WIFI_UI_PREVIEW
    control_start();
    recovery_at=millis()+15000;
#ifndef RTL_WIFI_NATIVE_PAD_TEST
    init_store();
    wifi_seed_init(mc_ready,argc,argv);
#else
    (void)argc;(void)argv;mc_ready=0; /* Emulator test only: no MC files touched. */
#endif
    wifi_files_native_init(&file_service,mc_ready,argc&&argv?argv[0]:0);
#ifdef RTL_WIFI_NATIVE_PAD_TEST
    robot_usb_init(0);
#else
    robot_usb_init(1);
#endif
    wifi_runtime_init(&runtime,&file_service,&robot,millis());wifi_runtime_clock(&runtime,wifi_clock_read(),millis());
    app.error_sink=runtime_error;app.error_context=&runtime;robot.error_sink=control_error_sink;robot.error_context=&runtime;physical_http_setup=setup_http;
    control_ready=1;if(!control_running)runtime_error(&runtime,WIFI_TR("Cannot start gamepad thread; using main-loop fallback."));
    if(robot_module_error){runtime_error(&runtime,WIFI_TR("Cannot load relay driver."));}if(app.state==WIFI_ERROR)runtime_error(&runtime,app.message);
#ifndef RTL_WIFI_NATIVE_PAD_TEST
    known_cache_n=wifi_seed_read_known(known_cache,sizeof(known_cache));
#endif
    physical_preferred_channel=runtime.last_channel;
#ifdef RTL_WIFI_NATIVE_PAD_TEST
    {int result=test_native_jobs();if(result){printf("NATIVE_JOB_TEST FAIL %d\n",result);return 1;}result=test_native_sources();if(result){printf("NATIVE_SOURCE_COMPILER_TEST FAIL %d\n",result);return 1;}result=test_native_relay_recovery();if(result){printf("NATIVE_RELAY_RECOVERY_TEST FAIL %d\n",result);return 1;}}
    {unsigned ticks=runtime.pad_ticks;platform_stop_workers();DelayThread(30000);if(runtime.pad_ticks!=ticks){printf("NATIVE_WORKER_SHUTDOWN_TEST FAIL thread still running\n");return 1;}control_start();control_ready=1;DelayThread(30000);if(runtime.pad_ticks==ticks){printf("NATIVE_WORKER_SHUTDOWN_TEST FAIL restart\n");return 1;}printf("NATIVE_WORKER_SHUTDOWN_TEST PASS pad-video stopped before platform reset and restarted\n");}
    {uint32_t start=millis(),ticks=runtime.pad_ticks;volatile unsigned work=0;ChangeThreadPriority(GetThreadId(),40);while((uint32_t)(millis()-start)<250)work++;ChangeThreadPriority(GetThreadId(),64);
     if(!control_running||runtime.pad_ticks-ticks<10||runtime.pad_max_gap_ms>100){printf("NATIVE_PAD_PRIORITY_TEST FAIL ticks=%u gap=%u\n",(unsigned)(runtime.pad_ticks-ticks),runtime.pad_max_gap_ms);return 1;}
     printf("NATIVE_PAD_PRIORITY_TEST PASS main-busy-250ms worker-ticks=%u max-gap-ms=%u work=%u\n",(unsigned)(runtime.pad_ticks-ticks),runtime.pad_max_gap_ms,work);}
    {uint32_t start=millis();unsigned draws=control_draws;volatile unsigned work=0;runtime.display_enabled=runtime.display_requested=1;
     while((uint32_t)(millis()-start)<400)work++;
     if(!video_running||control_draws==draws){printf("NATIVE_VIDEO_PRIORITY_TEST FAIL no frame while network main busy\n");return 1;}
     runtime.display_enabled=0;runtime.display_requested=1;DelayThread(30000);
     printf("NATIVE_VIDEO_PRIORITY_TEST PASS video-worker drew while network main busy, pad priority above video\n");}
    {uint32_t issued=millis();if(request(RTL_RPC_STATUS,issued-10000u)||(int32_t)(rpc_deadline-issued)<2000){printf("NATIVE_RPC_CLOCK_TEST FAIL stale caller clock\n");return 1;}printf("NATIVE_RPC_CLOCK_TEST PASS stale-loop-clock ignored at RPC dispatch\n");}
    {uint32_t until=millis()+4000;while(SifCheckStatRpc(&client)&&(int32_t)(millis()-until)<0)DelayThread(1000);
     if(SifCheckStatRpc(&client)){printf("NATIVE_USB_RECOVERY_TEST FAIL status stalled\n");return 1;}pending=0;
     if(request(RTL_RPC_RECOVER,millis()))return 1;
     until=millis()+4000;while(SifCheckStatRpc(&client)&&(int32_t)(millis()-until)<0)DelayThread(1000);
     if(SifCheckStatRpc(&client)||response.usb.magic!=RTL_RPC_MAGIC||response.usb.state!=RTL_WAITING){printf("NATIVE_USB_RECOVERY_TEST FAIL rebind stalled\n");return 1;}pending=0;
     printf("NATIVE_USB_RECOVERY_TEST PASS actual-IOP WiFi-only unregister-register pad-alive RF=absent\n");}
    {int result=test_native_offline_recovery();if(result){printf("NATIVE_OFFLINE_RECOVERY_TEST FAIL %d\n",result);return 1;}}
#endif
#else
    (void)argc;(void)argv;
    wifi_app_driver(&app,0);
#endif
    while(1){
        now=millis();
#ifndef RTL_WIFI_UI_PREVIEW
        if(!control_running)control_step();
        buttons=control_buttons;connected=control_connected;
        if(!video_running)video_step();
        if(control_error_pending){runtime_error(&runtime,control_error_text);control_error_pending=0;}
#else
        buttons=read_pad(&connected);
#endif
#ifdef RTL_WIFI_TRACE
        if(trace_frames<5||(buttons&&buttons!=trace_last_buttons))printf("WIFI_TRACE frame=%u clock_ms=%lu pad=%04lX connected=%d\n",trace_frames,(unsigned long)now,(unsigned long)buttons,connected);
        trace_frames++;trace_last_buttons=buttons;
#endif
        if(connected!=previous_connected){app.dirty=1;app.held_buttons=buttons;app.repeat_dir=0;previous_connected=connected;}
#ifndef RTL_WIFI_UI_PREVIEW
        {uint32_t menu=control_menu;
         if(control_reconnect){control_reconnect=0;known_tried=0;reconnect_pending=reconnect_attempts=0;physical_stop();
          if(physical_radio_ready){wifi_app_failure(&app,app.ticket,WIFI_TR("R2: searching for a known network..."));if(!app.scan.count){app.ticket++;app.state=WIFI_SCANNING;app.actions=WIFI_DO_SCAN;fast_scan=1;fast_count=0;}}
          else{recovery_wanted=1;recovery_attempts=0;physical_message(WIFI_TR("R2: preparing USB Wi-Fi; waiting for adapter..."));}}
         if(!drive_mode)wifi_app_input(&app,menu,now);
         if(controls.restart){static uint32_t restart_at;if(!restart_at){restart_at=now;physical_stop();physical_message(WIFI_TR("Restarting PS2..."));}robot.web=0;robot.halt=1;
             if((!robot.applied&&!robot_pending&&(uint32_t)(now-restart_at)>300)||(uint32_t)(now-restart_at)>2000){platform_stop_workers();if(pad_open){pad_open=0;padPortClose(0,0);}ExecOSD(0,0);}}
        }
#else
        wifi_app_input(&app,buttons,now);
#endif
#ifdef RTL_WIFI_UI_PREVIEW
        if((buttons&PAD_R1)&&!(previous_buttons&PAD_R1))preview_ping();
        previous_buttons=buttons;preview_poll(now);
#else
        usb_poll(now);
        {static unsigned last_transfer;unsigned active=file_service.upload.active||file_service.readers;static unsigned priority=64;if(priority!=(active?40u:64u)){priority=active?40:64;ChangeThreadPriority(GetThreadId(),priority);}if(last_transfer&&!file_service.upload.active&&wifi_files_native_flush())runtime_error(&runtime,WIFI_TR("Cannot flush file write cache."));last_transfer=file_service.upload.active;}
        offline_tick(now);
        try_known();
        if(reconnect_pending&&!app.linked&&app.state==WIFI_LIST&&(int32_t)(now-reconnect_at)>=0){unsigned i=app.active_ap;reconnect_pending=0;
            if(i<app.scan.count){app.selected=i;app.ticket++;if(app.known[i]&WIFI_KNOWN_CSV){app.state=WIFI_DERIVING;app.actions|=WIFI_DO_KNOWN;}
                else if(app.has_profile&&app.profile.ssid_length==app.scan.ap[i].ssid_length&&!memcmp(app.profile.ssid,app.scan.ap[i].ssid,app.profile.ssid_length)){memcpy(app.pending_pmk,app.profile.pmk,32);app.state=WIFI_JOINING;app.actions|=WIFI_DO_JOIN;}
                physical_message(WIFI_TR("Reconnecting to saved network automatically..."));}}
        {static uint32_t memory_at;static uint8_t controller_ip[4];unsigned warn=runtime.log_warning,failed=runtime.log_failed;if(http.peer_valid&&memcmp(controller_ip,http.peer_ip,4)){memcpy(controller_ip,http.peer_ip,4);robot.web=0;robot.halt=1;}if((uint32_t)(now-memory_at)>=1000){struct mallinfo mem=mallinfo();memory_at=now;runtime.heap_bytes=mem.uordblks;runtime.ram_bytes=GetMemorySize();}
         job_tick(now);if(runtime.system_action&&job_state==2)job_stop(5);
         wifi_runtime_energy(&runtime,now);wifi_runtime_tick(&runtime,now);
         app.log_warning=runtime.log_warning;app.log_failed=runtime.log_failed;app.heap_bytes=runtime.heap_bytes;app.ram_bytes=runtime.ram_bytes;if(warn!=runtime.log_warning||failed!=runtime.log_failed)app.dirty=1;
         app.battery_set=runtime.battery_set;app.battery_percent=runtime.battery_capacity?(unsigned)((uint64_t)runtime.battery_uwh*100/(runtime.battery_capacity*11100u)):0;
         snprintf(runtime.last_request,sizeof(runtime.last_request),"%s",app.last_request);
         app.transfer_active=file_service.upload.active||file_service.download.active;app.transfer_direction=!file_service.upload.active;
         if(file_service.upload.active){snprintf(app.transfer_path,sizeof(app.transfer_path),"%s",file_service.upload.path);app.transfer_size=file_service.upload.size;app.transfer_done=file_service.upload.received;}
         else{snprintf(app.transfer_path,sizeof(app.transfer_path),"%s",file_service.download.path);app.transfer_size=file_service.download.size;app.transfer_done=file_service.download.done;}
         runtime.signal_valid=app.signal_valid;runtime.signal_quality=app.signal_quality;
         if(runtime.system_action){static unsigned saved;robot.system_locked=1;
          if((int32_t)(now-runtime.system_due)>=0&&!robot.applied&&!robot_pending&&(robot_sync||(uint32_t)(now-runtime.system_requested)>2000)){
           if(!saved){runtime.battery_saved_at=now-300000u;wifi_runtime_tick(&runtime,now);saved=1;}
           if((uint32_t)(now-runtime.system_requested)>=250){unsigned action=runtime.system_action;uint32_t until=millis()+2000;while(pending&&SifCheckStatRpc(&client)&&(int32_t)(millis()-until)<0)DelayThread(1000);
            if(!pending||!SifCheckStatRpc(&client)){if(action==2){physical_stop();platform_checkpoint("RESTART: stopping worker threads");platform_stop_workers();platform_checkpoint("RESTART: workers stopped; closing pad port");if(pad_open){pad_open=0;padPortClose(0,0);}platform_checkpoint("RESTART: pad closed; entering ExecOSD");ExecOSD(0,0);platform_checkpoint("RESTART FAILED: ExecOSD returned");runtime.system_action=0;robot.system_locked=0;saved=0;control_start();control_ready=1;recovery_wanted=1;controls.restart=0;}else{int status=0;platform_checkpoint("POWEROFF: stopping worker threads");platform_stop_workers();if(SifExecModuleBuffer(poweroff_irx,size_poweroff_irx,0,0,&status)>=0&&status>=0&&poweroffInit()>=0){physical_stop();platform_checkpoint("POWEROFF: module ready; requesting standby");poweroffShutdown();DelayThread(500000);}runtime_error(&runtime,WIFI_TR("PS2 power-off service failed or shutdown did not complete."));runtime.system_action=0;robot.system_locked=0;saved=0;control_start();control_ready=1;}}
           }
          }
         }
         if(runtime.reload_pending){robot.halt=1;robot.web=0;if((uint32_t)(now-runtime.reload_at)>3000&&!robot.applied&&!robot_pending){unsigned i,transmitting=0;for(i=0;i<RTL_HTTP_CLIENTS;i++)if(http.client[i].state==3)transmitting=1;
           if(!transmitting||(uint32_t)(now-runtime.reload_at)>15000){char path[WF_PATH];uint32_t deadline=millis()+2000;snprintf(path,sizeof(path),"%s",file_service.running);while(pending&&SifCheckStatRpc(&client)&&(int32_t)(millis()-deadline)<0)DelayThread(1000);if(!pending||!SifCheckStatRpc(&client)){physical_stop();platform_stop_workers();if(pad_open){pad_open=0;padPortClose(0,0);}LoadExecPS2(path,0,0);}else{runtime.reload_pending=0;runtime_error(&runtime,WIFI_TR("Cannot reopen ELF: USB RPC is busy."));}}}}
        }
#endif
        {static uint32_t stats_at,rx,tx;if((uint32_t)(now-stats_at)>=1000){uint32_t elapsed=now-stats_at;app.rx_kbps=(unsigned)((uint64_t)(app.bytes_rx-rx)*8/elapsed);app.tx_kbps=(unsigned)((uint64_t)(app.bytes_tx-tx)*8/elapsed);stats_at=now;rx=app.bytes_rx;tx=app.bytes_tx;if(app.linked||app.transfer_active)app.dirty=1;
#ifndef RTL_WIFI_UI_PREVIEW
         runtime.rx_kbps=app.rx_kbps;runtime.tx_kbps=app.tx_kbps;
#endif
        }}
#ifdef RTL_WIFI_UI_PREVIEW
        {static uint32_t draw_at;if(app.dirty&&(uint32_t)(now-draw_at)>=100){draw_at=now;wifi_draw(&app,1,connected);app.dirty=0;}}
#endif
        actions(now);
#ifdef RTL_WIFI_NATIVE_PAD_TEST
        if(connected&&controls.drive&&!controls.display&&(buttons&PAD_CROSS)&&(robot.pad&PAD_CROSS)){
          *(volatile uint64_t*)0x12000000=wifi_video_pmode(1);wifi_render_reset();wifi_draw_drive(&app,buttons,connected,robot.ready,robot.left,robot.right,robot.applied,robot.owner,robot.desired);*(volatile uint64_t*)0x12000000=wifi_video_pmode(0);
          printf("NATIVE_VIDEO_TEST PASS explicit-PMODE on-render-off no-register-readback\n");
          printf("NATIVE_HEADLESS_PAD_TEST PASS actual-libpad drive=1 display=0 WiFi-independent MC=disabled hardware=untested\n");
          printf("WIFI_PAD_TEST PASS actual-pad-path=native-headless MC=disabled RF=absent\n");SleepThread();}
#endif
#ifdef RTL_WIFI_PAD_TEST
#ifdef RTL_WIFI_PAD_REJECT_TEST
        if(app.state==WIFI_LIST&&!app.has_profile&&!app.ip[0]&&strstr(app.message,WIFI_TR("wrong password"))){
            printf("WIFI_PAD_REJECT_TEST PASS no-profile-no-IP actual-pad-path auth=synthetic\n");return 0;
        }
#else
        if(app.state==WIFI_ONLINE&&app.ping_rx==1&&app.ping_tx==1){
            printf("WIFI_PAD_TEST PASS actual-pad-path=emulator-PADMAN-libpad-EE-UI auth-DHCP-ping=synthetic\n");return 0;
        }
#endif
#endif
#ifdef RTL_WIFI_UI_PREVIEW
        DelayThread(16667);
#else
        /* Network RPCs are independent of the 60Hz pad and 10Hz display.
           Yield to IOP instead of blocking every packet behind a TV frame. */
        DelayThread(app.linked?1000:4000);
#endif
    }
    return 0;
}
