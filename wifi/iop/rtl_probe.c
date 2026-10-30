/* SPDX-License-Identifier: MIT
 * RTL8192CU diagnostic IOP USB driver. Only USB configuration and register
 * reads are implemented by default. RTL_PHYSICAL_BOOT adds volatile power
 * and firmware RAM writes. Neither variant implements EEPROM writes or WLAN.
 */
#ifdef RTL_DRIVER_HOST_TEST
#include "iop_stub.h"
#else
#include <types.h>
#include <irx.h>
#include <loadcore.h>
#include <sysclib.h>
#include <stdio.h>
#include <thbase.h>
#include <intrman.h>
#include <sifrpc.h>
#include <usbd.h>
#include <usbd_macro.h>
#endif
#include "rtl_core.h"
#include "rtl_rpc.h"
#include "usb_inventory.h"
#ifndef RTL_DRIVER_HOST_TEST
extern int sceUsbdDiagnosticSnapshot(unsigned *,unsigned);
#endif
#ifdef RTL_PHYSICAL_RADIO
#include "rtl_wire.h"
static void wireless_disconnect(void);
#endif
#ifdef RTL_PHYSICAL_BOOT
#include "rtl_bringup.h"
extern unsigned char rtl_firmware_bin[];
extern unsigned int size_rtl_firmware_bin;
static RtlBootReport boot_report;
static unsigned int boot_generation;
#endif
#ifdef RTL_PHYSICAL_BOOT
IRX_ID("RTL8192CU firmware bringup",0,3);
#else
IRX_ID("RTL8192CU diagnostic USB driver",0,1);
#endif
static RtlReport report;
static void recover_usb(void);
static int device=-1,pipe=-1;
static volatile int config_pending,config_generation;
static volatile int transfer_pending,transfer_result,transfer_bytes;
/* Persistent buffer remains owned by USB after a timeout until its callback.
 * A timed-out transfer is never replaced with a new request on this buffer. */
static unsigned char transfer_buffer[
#ifdef RTL_PHYSICAL_BOOT
256
#else
64
#endif
] __attribute__((aligned(64)));
static SifRpcDataQueue_t queue __attribute__((aligned(16)));
static SifRpcServerData_t server __attribute__((aligned(16)));
static union {RtlReport report;RtlNetReport net;
UsbInventoryDevice inventory;
#ifdef RTL_PHYSICAL_RADIO
RtlWire wire;RtlRadioReport radio;
#endif
#ifdef RTL_PHYSICAL_BOOT
RtlBootReport boot;
#endif
} rpc_buffer __attribute__((aligned(64)));
static void reset_registers(void) {
    int i;report.registers_attempted=0;
    for(i=0;i<4;i++){report.register_result[i]=RTL_E_DISCONNECTED;report.register_value[i]=0;}
}
static void seen(unsigned int vid,unsigned int pid) {
    unsigned int i;
    for(i=0;i<report.seen_count;i++)if(report.seen_vid[i]==vid&&report.seen_pid[i]==pid)return;
    if(report.seen_count<RTL_SEEN_MAX){i=report.seen_count++;report.seen_vid[i]=vid;report.seen_pid[i]=pid;}
}
static int probe(int id) {
    UsbDeviceDescriptor *d=sceUsbdScanStaticDescriptor(id,0,USB_DT_DEVICE);
    if(!d)return 0;
    seen(d->idVendor,d->idProduct);
    return device<0&&!config_pending&&!transfer_pending&&rtl_matches(d->idVendor,d->idProduct);
}
static void configured(int result,int count,void *arg) {
    (void)count;(void)arg;
    if(device>=0&&(unsigned int)config_generation==report.generation){
        report.usb_result=result;report.state=result==USB_RC_OK?RTL_READY:RTL_CONFIG_FAILED;
    }
    config_pending=0;
}
static int connect(int id) {
    UsbDeviceDescriptor *d;
    UsbConfigDescriptor *c;
    RtlEndpoints endpoints;
    int ret;
    if(device>=0||config_pending||transfer_pending)return 1;
    d=sceUsbdScanStaticDescriptor(id,0,USB_DT_DEVICE);
    if(!d||!rtl_matches(d->idVendor,d->idProduct))return 1;
    report.generation++;reset_registers();report.parse_result=RTL_E_DESCRIPTOR;
    c=sceUsbdScanStaticDescriptor(id,d,USB_DT_CONFIG);
    if(!c){report.state=RTL_DESC_FAILED;return 1;}
    ret=rtl_parse_config((const uint8_t*)c,c->wTotalLength,&endpoints);
    report.parse_result=ret;
    if(ret<0){report.state=RTL_DESC_FAILED;return 1;}
    report.configuration=endpoints.configuration;report.interface_no=endpoints.interface_no;
    report.power_ma=endpoints.power_ma;report.bulk_in=endpoints.bulk_in;
    report.bulk_in_packet=endpoints.bulk_in_packet;report.out_count=endpoints.out_count;
    memcpy(report.bulk_out,endpoints.bulk_out,sizeof(report.bulk_out));
    memcpy(report.bulk_out_packet,endpoints.bulk_out_packet,sizeof(report.bulk_out_packet));
    pipe=sceUsbdOpenPipe(id,0);
    if(pipe<0){report.state=RTL_CONFIG_FAILED;report.usb_result=pipe;return 1;}
    device=id;report.state=RTL_CONFIGURING;config_generation=report.generation;config_pending=1;
    ret=sceUsbdSetConfiguration(pipe,endpoints.configuration,configured,0);
    if(ret!=USB_RC_OK){
        config_pending=0;report.state=RTL_CONFIG_FAILED;report.usb_result=ret;
        sceUsbdClosePipe(pipe);pipe=-1;device=-1;return 1;
    }
    printf("RTLPROBE: matched %04x:%04x interface=%u IN=%02x OUT=%u\n",RTL_VID,RTL_PID,endpoints.interface_no,endpoints.bulk_in,endpoints.out_count);
    return 0;
}
static int disconnect(int id) {
    if(id!=device)return 0;
#ifdef RTL_PHYSICAL_RADIO
    wireless_disconnect();
#endif
    device=-1;report.generation++;report.state=RTL_WAITING;reset_registers();
    if(pipe>=0)sceUsbdClosePipe(pipe);
    pipe=-1;
    /* Pending buffers/callback cookies are deliberately not reused here. */
    return 0;
}
static void transfer_done(int result,int count,void *arg) {
    (void)arg;transfer_result=result;transfer_bytes=count;transfer_pending=0;
}
static int control(void *context,const RtlRequest *request,uint8_t *out) {
    unsigned int generation=report.generation;
    int ret,tick;
    (void)context;
    if(device<0||report.state!=RTL_READY)return RTL_E_DISCONNECTED;
    if(transfer_pending)return RTL_E_BUSY;
    if(!request||!out||request->request!=5||request->request_index!=0||!request->length)return RTL_E_ARGUMENT;
#ifdef RTL_PHYSICAL_BOOT
    if(request->type==0x40){
#ifdef RTL_PHYSICAL_RADIO
        if(!rtl_boot_write_allowed(request->value,request->length)&&
           !rtl_radio_write_allowed(request->value,request->length,out))return RTL_E_ARGUMENT;
#else
        if(!rtl_boot_write_allowed(request->value,request->length))return RTL_E_ARGUMENT;
#endif
    }else if(request->type!=0xc0||request->length>4)return RTL_E_ARGUMENT;
#else
    if(request->type!=0xc0||request->length>4)return RTL_E_ARGUMENT;
#endif
    memset(transfer_buffer,0,sizeof(transfer_buffer));transfer_pending=1;
    if(request->type==0x40)memcpy(transfer_buffer,out,request->length);
    transfer_result=RTL_E_BUSY;transfer_bytes=0;
    ret=sceUsbdControlTransfer(pipe,request->type,request->request,request->value,request->request_index,request->length,transfer_buffer,transfer_done,0);
    if(ret!=USB_RC_OK){transfer_pending=0;report.usb_result=ret;return RTL_E_IO;}
    for(tick=0;tick<200&&transfer_pending;tick++)DelayThread(1000);
    if(generation!=report.generation||device<0)return RTL_E_DISCONNECTED;
    if(transfer_pending)return RTL_E_TIMEOUT;
    report.usb_result=transfer_result;
    if(transfer_result!=USB_RC_OK)return RTL_E_IO;
    if(transfer_bytes!=request->length)return RTL_E_SHORT;
    if(request->type==0xc0)memcpy(out,transfer_buffer,request->length);
    return transfer_bytes;
}
static void read_registers(void) {
    static const unsigned int address[4]={0x0000,0x0002,0x0080,0x00f0};
    static const unsigned int width[4]={2,2,1,4};
    RtlIo io={control,0};
    unsigned int generation=report.generation;
    int i;
    if(report.state!=RTL_READY||report.registers_attempted)return;
    report.registers_attempted=1;
    for(i=0;i<4;i++){
        uint32_t value=0;
        int ret=rtl_read_register(&io,address[i],width[i],&value);
        if(generation!=report.generation){reset_registers();return;}
        report.register_result[i]=ret;
        if(ret==RTL_OK)report.register_value[i]=value;
    }
}
#ifdef RTL_VIRTUAL_NET
#include "rtl_net_impl.h"
#endif
#ifdef RTL_PHYSICAL_BOOT
static void boot_delay(void *ctx,unsigned int microseconds){(void)ctx;DelayThread(microseconds);}
static RtlBootReport *boot_run(void){
    RtlIo io={control,0};unsigned int generation=report.generation;
    if(report.state!=RTL_READY){
        memset(&boot_report,0,sizeof(boot_report));boot_report.magic=RTL_BOOT_MAGIC;
        boot_report.result=RTL_E_DISCONNECTED;return &boot_report;
    }
    if(boot_generation==generation)return &boot_report;
    boot_generation=generation;
    rtl_boot_run(&io,boot_delay,0,rtl_firmware_bin,size_rtl_firmware_bin,&boot_report);
    if(generation!=report.generation){boot_report.result=RTL_E_DISCONNECTED;boot_report.stage=RTL_BOOT_IDENTIFY;}
    printf("RTLBOOT: result=%d stage=%u SYS_CFG=%08x uploaded=%u/%u transfers=%u\n",
        boot_report.result,boot_report.stage,boot_report.sys_cfg,boot_report.uploaded_bytes,
        boot_report.firmware_bytes,boot_report.transfers);
    return &boot_report;
}
#endif
#ifdef RTL_PHYSICAL_RADIO
#include "rtl_wifi_impl.h"
#endif
#ifndef RTL_DRIVER_HOST_TEST
#include "usb_inventory_impl.h"
#endif
static void *rpc(int command,void *buffer,int size) {
    int old;
    (void)size;
#ifndef RTL_DRIVER_HOST_TEST
    if(command==RTL_RPC_DEVICE){inventory_device(buffer);((UsbInventoryDevice*)buffer)->driver_flags=((UsbInventoryDevice*)buffer)->present&&((UsbInventoryDevice*)buffer)->id==(unsigned)device;return buffer;}
    if(command==RTL_RPC_HUB_POWER){inventory_hub_power(buffer);return buffer;}
    if(command==RTL_RPC_USB_CHECK){inventory_presence(buffer,RTL_VID,RTL_PID);return buffer;}
#endif
    if(command==RTL_RPC_RECOVER)recover_usb();
#ifdef RTL_PHYSICAL_RADIO
    if(command>=RTL_RPC_RADIO&&command<=RTL_RPC_TX)return wireless_rpc(command,buffer,size);
#endif
#ifdef RTL_VIRTUAL_NET
    if(command==RTL_RPC_NET_DNS){RtlNetReport result=net_dns();memcpy(buffer,&result,sizeof(result));return buffer;}
#endif
#ifdef RTL_PHYSICAL_BOOT
    if(command==RTL_RPC_BOOT){RtlBootReport *r=boot_run();memcpy(buffer,r,sizeof(*r));return buffer;}
#endif
    if(command==RTL_RPC_READ)read_registers();
    CpuSuspendIntr(&old);memcpy(buffer,&report,sizeof(report));CpuResumeIntr(old);
    return buffer;
}
static void rpc_thread(void *arg) {
    (void)arg;sceSifInitRpc(0);sceSifSetRpcQueue(&queue,GetThreadId());
    sceSifRegisterRpc(&server,RTL_RPC_ID,rpc,&rpc_buffer,0,0,&queue);sceSifRpcLoop(&queue);
}
static sceUsbdLddOps driver={0,0,"rtl8192cu_probe",probe,connect,disconnect,0,0,0,0,0,0};
static void recover_usb(void){
    int result;
    /* Only this LDD is detached. Never reset USBD, PADMAN or the relay card.
       ClosePipe removes OHCI endpoints; USBD retires their requests on its
       deferred timer. Keep all DMA buffers untouched during that grace period. */
    if(device>=0)disconnect(device);
#ifdef RTL_PHYSICAL_RADIO
    else wireless_disconnect();
#endif
    result=sceUsbdUnregisterLdd(&driver);
    if(result!=USB_RC_OK){report.usb_result=result;return;}
    DelayThread(1000000);
    config_pending=transfer_pending=0;
#ifdef RTL_PHYSICAL_RADIO
    wireless_rx_pending=wireless_tx_pending=wireless_rx_ready=0;
#endif
#ifdef RTL_PHYSICAL_BOOT
    boot_generation=0;memset(&boot_report,0,sizeof(boot_report));
#endif
    report.generation++;report.state=RTL_WAITING;reset_registers();
    /* SDK unregister removes the list entry but does not clear its links.
       RegisterLdd probes devices that are already attached as well. */
    driver.next=driver.prev=0;
    result=sceUsbdRegisterLdd(&driver);report.usb_result=result;
    if(result!=USB_RC_OK)report.state=RTL_CONFIG_FAILED;
}
int _start(int argc,char **argv) {
    iop_thread_t thread;int tid,result;
    (void)argc;(void)argv;
    memset(&report,0,sizeof(report));report.magic=RTL_RPC_MAGIC;report.version=1;reset_registers();
    memset(&thread,0,sizeof(thread));thread.attr=TH_C;thread.thread=rpc_thread;thread.priority=40;thread.stacksize=0x1000;
    tid=CreateThread(&thread);if(tid<0)return MODULE_NO_RESIDENT_END;
    result=sceUsbdRegisterLdd(&driver);
    if(result!=USB_RC_OK){DeleteThread(tid);return MODULE_NO_RESIDENT_END;}
    if(StartThread(tid,0)<0){sceUsbdUnregisterLdd(&driver);DeleteThread(tid);return MODULE_NO_RESIDENT_END;}
    return MODULE_RESIDENT_END;
}
