#define RTL_DRIVER_HOST_TEST
#define _start rtl_module_start
#include "../iop/rtl_probe.c"
#include <assert.h>
static UsbDeviceDescriptor fake_device;
static unsigned char config[]={9,2,39,0,1,1,0,0x80,250,9,4,0,0,3,255,255,255,0,
    7,5,129,2,64,0,0,7,5,2,2,64,0,0,7,5,3,2,64,0,0};
static int open_result,submit_result,completion_result,short_read,read_calls,close_calls;
static int pending_mode,disconnect_on_delay;
static void (*pending_callback)(int,int,void*);
static void *pending_arg;
static int pending_count;
static void *sceUsbdScanStaticDescriptor(int id,void*prev,int type){(void)id;(void)prev;return type==1?(void*)&fake_device:config;}
static int sceUsbdOpenPipe(int id,void*p){(void)id;(void)p;return open_result;}
static int sceUsbdClosePipe(int id){(void)id;close_calls++;return 0;}
static int sceUsbdControlTransfer(int ep,int type,int req,int value,int index,int len,void*data,void(*cb)(int,int,void*),void*arg){
    int i;(void)ep;(void)value;
    assert(index==0);
    if(req==5){
        assert(type==0xc0&&len>=1&&len<=4);read_calls++;
        for(i=0;i<len;i++)((unsigned char*)data)[i]=(unsigned char)(0x12+i*0x22);
    }else assert(req==9&&type==0&&len==0);
    if(submit_result)return submit_result;
    if(pending_mode){pending_callback=cb;pending_arg=arg;pending_count=len;}
    else cb(completion_result,short_read&&len?len-1:len,arg);
    return 0;
}
static void DelayThread(int us){(void)us;if(disconnect_on_delay){disconnect_on_delay=0;disconnect(device);}}
static void finish_pending(void){void(*cb)(int,int,void*)=pending_callback;assert(cb);pending_callback=0;cb(0,pending_count,pending_arg);}
static void reset(void){
    memset(&report,0,sizeof(report));report.magic=RTL_RPC_MAGIC;report.version=1;reset_registers();
    device=pipe=-1;config_pending=transfer_pending=0;
    fake_device.idVendor=RTL_VID;fake_device.idProduct=RTL_PID;
    open_result=5;submit_result=completion_result=short_read=read_calls=close_calls=pending_mode=disconnect_on_delay=0;
    pending_callback=0;config[14]=255;register_calls=unregister_calls=reattach_id=0;
}
int main(void){
    RtlReport reply;RtlIo io={control,0};uint32_t value;unsigned int generation;int i,cases=0;
    reset();assert(rtl_module_start(0,0)==0);cases++;
    reset();fake_device.idVendor=0x1234;assert(!probe(1)&&report.seen_count==1);cases++;
    reset();assert(probe(1)&&connect(1)==0&&report.state==RTL_READY&&device==1);assert(!probe(2));
    rpc(RTL_RPC_READ,&reply,sizeof(reply));assert(reply.registers_attempted&&read_calls==4);
    for(i=0;i<4;i++)assert(reply.register_result[i]==0);
    assert(reply.register_value[3]==0x78563412);rpc(RTL_RPC_READ,&reply,sizeof(reply));assert(read_calls==4);cases++;
    generation=report.generation;disconnect(1);assert(report.generation>generation&&report.state==RTL_WAITING&&device<0&&pipe<0);
    for(i=0;i<4;i++){assert(report.register_value[i]==0&&report.register_result[i]==RTL_E_DISCONNECTED);}cases++;
    assert(probe(2)&&connect(2)==0);rpc(RTL_RPC_READ,&reply,sizeof(reply));assert(read_calls==8);cases++;
    reset();open_result=-1;assert(connect(1)==1&&report.state==RTL_CONFIG_FAILED&&device<0);cases++;
    reset();submit_result=7;assert(connect(1)==1&&report.state==RTL_CONFIG_FAILED&&close_calls==1&&!config_pending);cases++;
    reset();completion_result=7;assert(connect(1)==0&&report.state==RTL_CONFIG_FAILED);cases++;
    reset();config[14]=3;assert(connect(1)==1&&report.state==RTL_DESC_FAILED);cases++;
    reset();connect(1);submit_result=7;value=0x1234;assert(rtl_read_register(&io,0xf0,4,&value)==RTL_E_IO&&value==0x1234&&!transfer_pending);cases++;
    reset();connect(1);completion_result=7;assert(rtl_read_register(&io,0xf0,4,&value)==RTL_E_IO&&!transfer_pending);cases++;
    reset();connect(1);short_read=1;value=0x1234;assert(rtl_read_register(&io,0xf0,4,&value)==RTL_E_SHORT&&value==0x1234);cases++;
    reset();connect(1);pending_mode=1;value=0x1234;assert(rtl_read_register(&io,0xf0,4,&value)==RTL_E_TIMEOUT&&transfer_pending&&value==0x1234);
    assert(rtl_read_register(&io,0xf0,4,&value)==RTL_E_BUSY&&read_calls==1);
    finish_pending();pending_mode=0;assert(rtl_read_register(&io,0xf0,4,&value)==0&&value==0x78563412);cases++;
    reset();connect(1);pending_mode=1;rpc(RTL_RPC_READ,&reply,sizeof(reply));assert(read_calls==1&&reply.register_result[0]==RTL_E_TIMEOUT);
    for(i=1;i<4;i++){assert(reply.register_result[i]==RTL_E_BUSY);}finish_pending();cases++;
    reset();connect(1);pending_mode=1;disconnect_on_delay=1;value=0x1234;
    assert(rtl_read_register(&io,0xf0,4,&value)==RTL_E_DISCONNECTED&&value==0x1234);
    assert(!probe(2));finish_pending();assert(probe(2));pending_mode=0;assert(connect(2)==0);cases++;
    reset();pending_mode=1;connect(1);assert(report.state==RTL_CONFIGURING&&config_pending);
    disconnect(1);assert(!probe(2));finish_pending();assert(report.state==RTL_WAITING&&!config_pending&&probe(2));cases++;
    reset();completion_result=7;connect(1);assert(report.state==RTL_CONFIG_FAILED&&device==1);
    completion_result=0;reattach_id=1;generation=report.generation;rpc(RTL_RPC_RECOVER,&reply,sizeof(reply));
    assert(reply.state==RTL_READY&&reply.generation>generation&&device==1&&close_calls==1&&register_calls==1&&unregister_calls==1);cases++;
    reset();reattach_id=2;rpc(RTL_RPC_RECOVER,&reply,sizeof(reply));assert(reply.state==RTL_READY&&device==2);cases++;
    reset();pending_mode=1;connect(1);assert(config_pending);recover_usb();
    assert(!config_pending&&!transfer_pending&&device<0&&report.state==RTL_WAITING&&probe(2));cases++;
    printf("IOP lifecycle scenarios=%d failures=0 (mock USB/scheduler, actual production callbacks)\n",cases);
    return 0;
}
