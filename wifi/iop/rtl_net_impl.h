/* SPDX-License-Identifier: MIT -- only compiled into the virtual USB test IRX. */
#include "rtl_net_packet.h"
static unsigned char net_tx[2048] __attribute__((aligned(64))),net_rx[2048] __attribute__((aligned(64)));
static volatile int net_pending,net_result,net_count;
static unsigned int net_sequence;
static void net_done(int result,int count,void*arg){(void)arg;net_result=result;net_count=count;net_pending=0;}
static UsbEndpointDescriptor *net_endpoint(unsigned int address){
 UsbDeviceDescriptor*d=sceUsbdScanStaticDescriptor(device,0,USB_DT_DEVICE);
 UsbConfigDescriptor*c=d?sceUsbdScanStaticDescriptor(device,d,USB_DT_CONFIG):0;
 unsigned int offset;unsigned char*p=(unsigned char*)c;
 if(!c||c->wTotalLength>4096)return 0;
 for(offset=c->bLength;offset+2<=c->wTotalLength;){
  unsigned int len=p[offset];if(len<2||offset+len>c->wTotalLength)return 0;
  if(p[offset+1]==USB_DT_ENDPOINT&&len>=7&&p[offset+2]==address)return (UsbEndpointDescriptor*)(p+offset);
  offset+=len;
 }
 return 0;
}
static int net_transfer(int ep,void*data,int bytes){
 int tick,ret;if(net_pending)return RTL_E_BUSY;net_pending=1;net_result=RTL_E_BUSY;net_count=0;
 ret=sceUsbdBulkTransfer(ep,data,bytes,net_done,0);
 if(ret!=USB_RC_OK){net_pending=0;return RTL_E_IO;}
 for(tick=0;tick<4000&&net_pending;tick++)DelayThread(1000);
 if(net_pending)return RTL_E_TIMEOUT;
 return net_result==USB_RC_OK?net_count:RTL_E_IO;
}
static RtlNetReport net_dns(void){
 RtlNetReport r;RtlIo io={control,0};uint32_t signature=0;int in=-1,out=-1,bytes,ret;
 unsigned int port;UsbEndpointDescriptor *ie,*oe;
 memset(&r,0,sizeof(r));r.magic=RTL_NET_MAGIC;r.sequence=++net_sequence;r.result=RTL_E_DISCONNECTED;
 if(report.state!=RTL_READY)return r;
 ret=rtl_read_register(&io,0xfff0,4,&signature);
 if(ret||signature!=0x4c545256){r.result=RTL_E_ARGUMENT;return r;}
 ie=net_endpoint(report.bulk_in);oe=net_endpoint(report.bulk_out[0]);
 if(!ie||!oe){r.result=RTL_E_DESCRIPTOR;return r;}
 in=sceUsbdOpenPipe(device,ie);out=sceUsbdOpenPipe(device,oe);
 if(in<0||out<0){r.result=RTL_E_IO;goto done;}
 r.dns_id=0x5400+(r.sequence&255);port=49152+(r.sequence&1023);
 bytes=rtl_net_query(net_tx,sizeof(net_tx),r.dns_id,port);
 if(bytes<0){r.result=RTL_E_ARGUMENT;goto done;}
 ret=net_transfer(out,net_tx,bytes);if(ret!=bytes){r.result=ret<0?ret:RTL_E_SHORT;goto done;}r.tx_bytes=ret;
 ret=net_transfer(in,net_rx,sizeof(net_rx));if(ret<0){r.result=ret;goto done;}r.rx_bytes=ret;
 r.result=rtl_net_response(net_rx,r.rx_bytes,r.dns_id,port,&r.dns_answers);
 done:
 /* No buffer reuse after a timeout; the guest ends the test immediately. */
 if(!net_pending){if(in>=0)sceUsbdClosePipe(in);if(out>=0)sceUsbdClosePipe(out);}
 return r;
}
