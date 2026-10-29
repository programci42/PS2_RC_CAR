/* SPDX-License-Identifier: MIT */
#define NEWLIB_PORT_AWARE
#include <tamtypes.h>
#include <kernel.h>
#include <delaythread.h>
#include <sifrpc.h>
#include <iopcontrol.h>
#include <loadfile.h>
#include <iopheap.h>
#include <sbv_patches.h>
#include <debug.h>
#include <string.h>
#include "rtl_core.h"
#include "rtl_rpc.h"
extern unsigned char usbd_irx[],rtl_probe_irx[];
extern unsigned int size_usbd_irx,size_rtl_probe_irx;
static SifRpcClientData_t client __attribute__((aligned(64)));
static RtlReport report __attribute__((aligned(64)));
static void stop(const char *text) {
    scr_printf("\nSTOP: %s\nNo WiFi success is claimed.\n",text);
    SleepThread();
}
static int wait_rpc(void) {
    int tick;
    for(tick=0;tick<1000;tick++) {
        if(!SifCheckStatRpc(&client))return 0;
        DelayThread(1000);
    }
    return -1;
}
static int load(void *data,unsigned int size) {
    int result=0,id=SifExecModuleBuffer(data,size,0,0,&result);
    return id<0||result!=0?-1:0;
}
static void draw(void) {
    static const char *names[4]={"SYS_ISO_CTRL 0000","SYS_FUNC_EN 0002","MCUFWDL     0080","SYS_CFG     00F0"};
    unsigned int i;
    scr_clear();scr_setXY(0,0);
    scr_printf("RTL8192CU USB DIAGNOSTIC v0.1\n");
    scr_printf("Target 0BDA:8178 | REAL USB PATH\n");
    scr_printf("READ-ONLY REGISTERS / NO WIFI CONNECTION\n\n");
    scr_printf("State: %u  generation: %u\n",report.state,report.generation);
    scr_printf("0=waiting 1=configuring 2=USB ready\n");
    scr_printf("3=config failed 4=descriptor failed\n");
    scr_printf("USB result: %d  descriptor: %s\n",report.usb_result,report.state==RTL_WAITING?"NO TARGET":rtl_error(report.parse_result));
    scr_printf("Seen IDs (USB enumeration only):\n");
    for(i=0;i<report.seen_count&&i<RTL_SEEN_MAX;i++)scr_printf(" %04X:%04X\n",report.seen_vid[i],report.seen_pid[i]);
    if(!report.seen_count)scr_printf(" (none)\n");
    if(report.state==RTL_READY) {
        scr_printf("Config %u interface %u requested %u mA\n",report.configuration,report.interface_no,report.power_ma);
        scr_printf("Requested mA is NOT a power measurement.\n");
        scr_printf("IN %02X packet %u | OUT count %u\n",report.bulk_in,report.bulk_in_packet,report.out_count);
        for(i=0;i<report.out_count&&i<3;i++)scr_printf(" OUT %02X packet %u\n",report.bulk_out[i],report.bulk_out_packet[i]);
        for(i=0;i<4;i++) {
            if(report.register_result[i]==0&&report.registers_attempted)
                scr_printf("%s = %08X\n",names[i],report.register_value[i]);
            else scr_printf("%s : %s\n",names[i],rtl_error(report.register_result[i]));
        }
    }
    scr_printf("\nFirmware upload, RF, WPA2, DHCP: NOT IMPLEMENTED\n");
    scr_printf("Hot-plug target to repeat register probe.\n");
}
int main(void) {
    int tick;
    init_scr();scr_setCursor(0);
    scr_printf("RTL8192CU hardware USB probe v0.1\nIndependent test - no vehicle relays\nInitialising IOP...\n");
    SifInitRpc(0);
    for(tick=0;tick<500&&!SifIopReset("",0);tick++)DelayThread(10000);
    if(tick==500){stop("IOP reset request timed out");return 1;}
    for(tick=0;tick<500&&!SifIopSync();tick++)DelayThread(10000);
    if(tick==500){stop("IOP reset sync timed out");return 1;}
    SifInitRpc(0);SifLoadFileInit();SifInitIopHeap();sbv_patch_enable_lmb();sbv_patch_disable_prefix_check();
    if(load(usbd_irx,size_usbd_irx)<0){stop("USBD module failed");return 1;}
    if(load(rtl_probe_irx,size_rtl_probe_irx)<0){stop("RTL diagnostic module failed");return 1;}
    memset(&client,0,sizeof(client));
    for(tick=0;tick<100;tick++) {
        if(SifBindRpc(&client,RTL_RPC_ID,SIF_RPC_M_NOWAIT)<0){stop("RPC bind start failed");return 1;}
        if(wait_rpc()<0){stop("RPC bind reply timeout");return 1;}
        if(client.server)break;
        DelayThread(10000);
    }
    if(!client.server){stop("Diagnostic RPC server missing");return 1;}
    while(1) {
        memset(&report,0,sizeof(report));
        if(SifCallRpc(&client,RTL_RPC_READ,SIF_RPC_M_NOWAIT,&report,sizeof(report),&report,sizeof(report),0,0)<0){stop("RPC request rejected");return 1;}
        if(wait_rpc()<0){stop("RPC reply timed out; buffer retained");return 1;}
        if(report.magic!=RTL_RPC_MAGIC||report.version!=1){stop("Invalid RPC response");return 1;}
        draw();DelayThread(1000000);
    }
    return 0;
}
