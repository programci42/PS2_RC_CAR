/* SPDX-License-Identifier: MIT -- runs actual IOP driver on register model. */
#define main diagnostic_main
#include "probe.c"
#undef main
#include "rtl_bringup.h"
#include <stdio.h>
static RtlBootReport boot __attribute__((aligned(64)));
int main(void){
    int i,tick;
    SifInitRpc(0);SifLoadFileInit();SifInitIopHeap();sbv_patch_enable_lmb();sbv_patch_disable_prefix_check();
    if(load(usbd_irx,size_usbd_irx)<0||load(rtl_probe_irx,size_rtl_probe_irx)<0){printf("BOOT_MODEL_TEST FAIL modules\n");return 1;}
    for(i=0;i<100;i++){
        if(SifBindRpc(&client,RTL_RPC_ID,SIF_RPC_M_NOWAIT)<0||wait_rpc()<0){printf("BOOT_MODEL_TEST FAIL bind\n");return 1;}
        if(client.server)break;
        DelayThread(10000);
    }
    if(!client.server){printf("BOOT_MODEL_TEST FAIL no server\n");return 1;}
    DelayThread(100000);
    if(SifCallRpc(&client,RTL_RPC_BOOT,SIF_RPC_M_NOWAIT,&boot,sizeof(boot),&boot,sizeof(boot),0,0)<0){printf("BOOT_MODEL_TEST FAIL start\n");return 1;}
    for(tick=0;tick<12000&&SifCheckStatRpc(&client);tick++)DelayThread(1000);
    if(tick==12000){printf("BOOT_MODEL_TEST FAIL RPC timeout\n");SleepThread();return 1;}
    printf("BOOT_MODEL result=%ld stage=%lu uploaded=%lu/%lu transfers=%lu SYS_CFG=%08lX\n",
        (long)boot.result,(unsigned long)boot.stage,(unsigned long)boot.uploaded_bytes,
        (unsigned long)boot.firmware_bytes,(unsigned long)boot.transfers,(unsigned long)boot.sys_cfg);
    if(boot.magic!=RTL_BOOT_MAGIC||boot.result||boot.stage!=RTL_BOOT_COMPLETE){printf("BOOT_MODEL_TEST FAIL\n");return 1;}
    printf("BOOT_MODEL_TEST PASS path=EE-IOP-USBD-register-model firmware-bytes=13508\n");
    printf("Register model only; the 8051 firmware is not executed. No physical WiFi tested.\n");
    return 0;
}
