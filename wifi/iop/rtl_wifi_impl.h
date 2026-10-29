/* SPDX-License-Identifier: MIT. Physical bulk transport, no virtual signature. */
static RtlRadioReport radio_report;
static int wireless_in=-1,wireless_out=-1;
static int wireless_data_out=-1;
static unsigned wireless_generation;
static uint8_t wireless_rx[4096] __attribute__((aligned(64)));
static uint8_t wireless_tx[2496] __attribute__((aligned(64)));
static volatile int wireless_rx_pending,wireless_rx_ready,wireless_rx_count,wireless_rx_result;
static unsigned wireless_rx_offset,wireless_rx_packets;
static volatile int wireless_tx_pending,wireless_tx_count,wireless_tx_result;
static UsbEndpointDescriptor *wireless_endpoint(unsigned address){
    UsbDeviceDescriptor*d=sceUsbdScanStaticDescriptor(device,0,USB_DT_DEVICE);
    UsbConfigDescriptor*c=d?sceUsbdScanStaticDescriptor(device,d,USB_DT_CONFIG):0;
    unsigned off;uint8_t*p=(uint8_t*)c;if(!c||c->wTotalLength>4096)return 0;
    for(off=c->bLength;off+2<=c->wTotalLength;){unsigned n=p[off];
        if(n<2||off+n>c->wTotalLength)return 0;
        if(p[off+1]==USB_DT_ENDPOINT&&n>=7&&p[off+2]==address)return (UsbEndpointDescriptor*)(p+off);
        off+=n;
    }return 0;
}
static void wireless_disconnect(void){
    if(wireless_in>=0)sceUsbdClosePipe(wireless_in);
    if(wireless_out>=0)sceUsbdClosePipe(wireless_out);
    if(wireless_data_out>=0&&wireless_data_out!=wireless_out)sceUsbdClosePipe(wireless_data_out);
    wireless_data_out=-1;
    wireless_in=wireless_out=-1;wireless_rx_ready=0;wireless_rx_offset=wireless_rx_packets=0;
    /* Pending flags are cleared only by completion, never reuse owned storage. */
}
static void wireless_received(int result,int count,void *arg){
    (void)arg;wireless_rx_result=result;wireless_rx_count=count;
    wireless_rx_offset=0;
    wireless_rx_packets=count>=24&&wireless_rx[10]?wireless_rx[10]:1;
    wireless_rx_ready=1;wireless_rx_pending=0;
}
static void wireless_sent(int result,int count,void *arg){
    (void)arg;wireless_tx_result=result;wireless_tx_count=count;wireless_tx_pending=0;
}
static int wireless_receive_start(void){
    int result;if(wireless_rx_pending||wireless_rx_ready)return 0;
    wireless_rx_pending=1;
    result=sceUsbdBulkTransfer(wireless_in,wireless_rx,sizeof(wireless_rx),wireless_received,0);
    if(result!=USB_RC_OK){wireless_rx_pending=0;return RTL_E_IO;}return 0;
}
static void *wireless_rpc(int command,void *buffer,int size){
    RtlIo io={control,0};RtlWire *w=buffer;unsigned gen=report.generation;int n,t;
    if(command==RTL_RPC_RADIO){
        memset(&radio_report,0,sizeof(radio_report));
        if(boot_generation!=gen||boot_report.result||boot_report.stage!=RTL_BOOT_COMPLETE)
            radio_report.result=RTL_E_DISCONNECTED;
        else if(wireless_rx_pending||wireless_tx_pending)radio_report.result=RTL_E_BUSY;
        else{
            wireless_disconnect();
            rtl_radio_init(&io,boot_delay,0,&radio_report);
            if(!radio_report.result){
                UsbEndpointDescriptor *in=wireless_endpoint(report.bulk_in),*out=wireless_endpoint(report.bulk_out[0]);
                if(!in||!out)radio_report.result=RTL_E_DESCRIPTOR;
                /* PS2SDK OpenPipe caps unaligned bulk OUT at 62 bytes.
                 * These DMA buffers are aligned(64): preserve the actual
                 * endpoint packet size or long WLAN frames are cut short. */
                else{wireless_in=sceUsbdOpenPipeAligned(device,in);wireless_out=sceUsbdOpenPipeAligned(device,out);
                    if(report.out_count>1){UsbEndpointDescriptor *data=wireless_endpoint(report.bulk_out[report.out_count-1]);
                        wireless_data_out=data?sceUsbdOpenPipeAligned(device,data):-1;
                    }else wireless_data_out=wireless_out;
                    if(wireless_in<0||wireless_out<0||wireless_data_out<0)radio_report.result=RTL_E_IO;
                    else {wireless_generation=gen;radio_report.result=wireless_receive_start();}}
            }
        }
        if(gen!=report.generation)radio_report.result=RTL_E_DISCONNECTED;
        memcpy(buffer,&radio_report,sizeof(radio_report));return buffer;
    }
    if(size!=(int)sizeof(*w)){if(size>=4)w->result=RTL_E_ARGUMENT;return buffer;}
    w->result=RTL_E_DISCONNECTED;
    if(report.state!=RTL_READY||gen!=wireless_generation||wireless_in<0||wireless_out<0)return buffer;
    w->generation=gen;
    if(command==RTL_RPC_CHANNEL){
        w->result=rtl_radio_channel(&io,boot_delay,0,&radio_report,w->channel);
        if(!w->result)w->result=rtl_radio_bssid(&io,boot_delay,0,&radio_report,w->length==6?w->bytes:0);
        return buffer;
    }
    if(command==RTL_RPC_RX){
        w->length=0;w->result=0;w->quality=0;
        if(wireless_rx_ready){
            if(wireless_rx_result==USB_RC_OK){
                /* Keep every encapsulated frame, including a management
                 * response following a beacon in the same USB transfer. */
                while(wireless_rx_packets&&wireless_rx_offset<(unsigned)wireless_rx_count){
                    unsigned consumed=0;
                    w->quality=rtl_wire_quality(wireless_rx+wireless_rx_offset,wireless_rx_count-wireless_rx_offset);
                    n=rtl_wire_rx_next(wireless_rx+wireless_rx_offset,
                        wireless_rx_count-wireless_rx_offset,w->bytes,sizeof(w->bytes),&consumed);
                    if(!consumed){wireless_rx_packets=0;break;}
                    wireless_rx_offset+=consumed;wireless_rx_packets--;
                    if(n>0){w->length=n;break;}
                }
            }else w->result=RTL_E_IO;
            if(w->result||!wireless_rx_packets||wireless_rx_offset>=(unsigned)wireless_rx_count)wireless_rx_ready=0;
        }
        if(!w->result)w->result=wireless_receive_start();
        return buffer;
    }
    if(command==RTL_RPC_TX){
        unsigned data=(w->bytes[0]&0x0c)==8,epindex=data?report.out_count-1:0;
        w->result=RTL_E_BUSY;if(wireless_tx_pending)return buffer;
        n=rtl_wire_tx_rate(w->bytes,w->length,wireless_tx,sizeof(wireless_tx),report.bulk_out_packet[epindex],w->quality);
        if(n<0){w->result=RTL_E_ARGUMENT;return buffer;}
        wireless_tx_pending=1;wireless_tx_count=0;
        t=sceUsbdBulkTransfer(data?wireless_data_out:wireless_out,wireless_tx,n,wireless_sent,0);
        if(t!=USB_RC_OK){wireless_tx_pending=0;w->result=RTL_E_IO;return buffer;}
        for(t=0;t<200&&wireless_tx_pending;t++)DelayThread(1000);
        w->result=wireless_tx_pending?RTL_E_TIMEOUT:(wireless_tx_result?RTL_E_IO:(wireless_tx_count==n?0:RTL_E_SHORT));
        if(gen!=report.generation)w->result=RTL_E_DISCONNECTED;
    }
    return buffer;
}
