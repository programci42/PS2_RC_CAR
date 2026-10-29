/* SPDX-License-Identifier: MIT */
#define main diagnostic_main
#include "probe.c"
#undef main
#include "rtl_bringup.h"
static RtlBootReport boot __attribute__((aligned(64)));
static void draw_boot(void){
    static const char *stage[]={"image validation","silicon identification","autoload",
        "power sequence","MAC enable","8051 reset","firmware upload",
        "firmware checksum","firmware readiness","firmware ready","DMA/LLT preparation"};
    scr_clear();scr_setXY(0,0);
    scr_printf("RTL8192CU PHYSICAL BRINGUP v0.3\n");
    scr_printf("Target 0BDA:8178 | Register + firmware stage\n\n");
    scr_printf("Result: %ld  stage: %lu (%s)\n",(long)boot.result,(unsigned long)boot.stage,
        boot.stage<11?stage[boot.stage]:"invalid");
    scr_printf("SYS_CFG: %08lX\nFirmware version: %lu\n",(unsigned long)boot.sys_cfg,(unsigned long)boot.firmware_version);
    scr_printf("Uploaded: %lu / %lu bytes\n",(unsigned long)boot.uploaded_bytes,(unsigned long)boot.firmware_bytes);
    scr_printf("Last register: %04lX value: %08lX\n",(unsigned long)boot.last_address,(unsigned long)boot.last_value);
    scr_printf("USB transfers: %lu polls: %lu\n\n",(unsigned long)boot.transfers,(unsigned long)boot.polls);
    if(!boot.result&&boot.stage==RTL_BOOT_COMPLETE)scr_printf("Firmware ready bit observed.\n");
    else scr_printf("Bringup stopped. Error is NOT WiFi success.\n");
    scr_printf("\nRF calibration / scan / WPA2 / DHCP / TCP:\nNOT IMPLEMENTED IN THIS ELF\n");
    scr_printf("\nNo vehicle relays are controlled.\nUnplug/replug adapter to repeat.\n");
}
int main(void){
    unsigned int last=0;int tick;
    init_scr();scr_setCursor(0);scr_printf("RTL8192CU physical firmware bringup v0.3\nInitialising IOP...\n");
    SifInitRpc(0);
    for(tick=0;tick<500&&!SifIopReset("",0);tick++)DelayThread(10000);
    if(tick==500){stop("IOP reset request timed out");return 1;}
    for(tick=0;tick<500&&!SifIopSync();tick++)DelayThread(10000);
    if(tick==500){stop("IOP reset sync timed out");return 1;}
    SifInitRpc(0);SifLoadFileInit();SifInitIopHeap();sbv_patch_enable_lmb();sbv_patch_disable_prefix_check();
    if(load(usbd_irx,size_usbd_irx)<0||load(rtl_probe_irx,size_rtl_probe_irx)<0){stop("USB/bringup module failed");return 1;}
    memset(&client,0,sizeof(client));
    for(tick=0;tick<100;tick++){
        if(SifBindRpc(&client,RTL_RPC_ID,SIF_RPC_M_NOWAIT)<0||wait_rpc()<0){stop("RPC bind failed");return 1;}
        if(client.server)break;
        DelayThread(10000);
    }
    if(!client.server){stop("RPC server missing");return 1;}
    while(1){
        if(SifCallRpc(&client,RTL_RPC_STATUS,SIF_RPC_M_NOWAIT,&report,sizeof(report),&report,sizeof(report),0,0)<0||wait_rpc()<0){stop("USB status failed");return 1;}
        if(report.magic!=RTL_RPC_MAGIC){stop("Invalid USB status");return 1;}
        if(report.state==RTL_READY&&report.generation!=last){
            scr_clear();scr_setXY(0,0);scr_printf("Uploading bundled Realtek firmware...\nThis does not yet connect to WiFi.\n");
            memset(&boot,0,sizeof(boot));
            if(SifCallRpc(&client,RTL_RPC_BOOT,SIF_RPC_M_NOWAIT,&boot,sizeof(boot),&boot,sizeof(boot),0,0)<0){stop("Bringup request failed");return 1;}
            for(tick=0;tick<12000&&SifCheckStatRpc(&client);tick++)DelayThread(10000);
            if(tick==12000){stop("Bringup RPC timed out; buffer retained");return 1;}
            if(boot.magic!=RTL_BOOT_MAGIC){stop("Invalid bringup response");return 1;}
            last=report.generation;draw_boot();
        }else if(report.state!=RTL_READY){
            last=0;scr_clear();scr_setXY(0,0);
            scr_printf("RTL8192CU physical bringup v0.3\nUSB state: %u result: %d\n",report.state,report.usb_result);
            scr_printf("Waiting for adapter 0BDA:8178...\n");
            for(tick=0;tick<(int)report.seen_count&&tick<RTL_SEEN_MAX;tick++)scr_printf("Seen: %04X:%04X\n",report.seen_vid[tick],report.seen_pid[tick]);
            scr_printf("No WiFi connection is claimed.\n");
        }
        DelayThread(500000);
    }
    return 0;
}
