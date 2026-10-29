/* Descriptor-only diagnostics: never SET_CONFIGURATION or reset another LDD. */
/* Cached descriptors only: no USB control transfers and no wait loops. */
static void inventory_presence(UsbInventoryDevice*out,unsigned vid,unsigned pid){unsigned id;memset(out,0,sizeof(*out));
 for(id=1;id<USB_INVENTORY_MAX;id++){const unsigned char*d=(const unsigned char*)sceUsbdScanStaticDescriptor(id,0,USB_DT_DEVICE);if(!d)continue;out->config_total++;
  if(!out->present&&(unsigned)(d[8]|(d[9]<<8))==vid&&(unsigned)(d[10]|(d[11]<<8))==pid){out->id=id;out->present=1;memcpy(out->descriptor,d,18);}}
}
static unsigned char inventory_string_buffer[256] __attribute__((aligned(64)));
static volatile int inventory_pending,inventory_result,inventory_bytes;
static void inventory_done(int r,int n,void*a){(void)a;inventory_result=r;inventory_bytes=n;inventory_pending=0;}
static int inventory_control(int ep,unsigned type,unsigned request,unsigned value,unsigned index,unsigned length){int r,t;
 if(inventory_pending)return -1001;
 inventory_pending=1;inventory_result=-1000;inventory_bytes=0;
 memset(inventory_string_buffer,0,sizeof(inventory_string_buffer));
 r=sceUsbdControlTransfer(ep,type,request,value,index,length,inventory_string_buffer,inventory_done,0);
 if(r){inventory_pending=0;return r;}
 for(t=0;t<150&&inventory_pending;t++)DelayThread(1000);
 return inventory_pending?-1000:inventory_result;
}
static int inventory_read(int ep,unsigned index,unsigned language){return inventory_control(ep,0x80,6,0x300|index,language,255);}
/* Power only; enumeration/reset and change-bit ownership remain with USBD. */
static void inventory_hub_power(UsbInventoryDevice*out){unsigned id=out->id,j,force=out->driver_flags==0x80000000u;int ep;UsbDeviceDescriptor*d;UsbConfigDescriptor*c;
 memset(out,0,sizeof(*out));
 for(;id<USB_INVENTORY_MAX;id++){d=sceUsbdScanStaticDescriptor(id,0,USB_DT_DEVICE);if(d&&((unsigned char*)d)[4]==9)break;}
 out->id=id;if(id>=USB_INVENTORY_MAX)return;out->present=1;ep=sceUsbdOpenPipe(id,0);if(ep<0){out->hub_result=ep;return;}
 out->location_result=inventory_control(ep,0x80,8,0,0,1);
 if(!out->location_result&&inventory_bytes==1){out->config_total=inventory_string_buffer[0];
  if(force&&!out->config_total){c=sceUsbdScanStaticDescriptor(id,d,USB_DT_CONFIG);if(c){out->location_result=inventory_control(ep,0,9,((unsigned char*)c)[5],0,0);if(!out->location_result)DelayThread(100000);}}}
 out->hub_result=inventory_control(ep,0xa0,6,0x2900,0,64);
 if(!out->hub_result&&inventory_bytes>=3){out->hub_port_count=inventory_string_buffer[2];
  for(j=0;j<out->hub_port_count&&j<8;j++){int r=inventory_control(ep,0xa3,0,0,j+1,4);out->hub_port_result[j]=r;
   if(!r&&inventory_bytes==4){unsigned status=inventory_string_buffer[0]|(inventory_string_buffer[1]<<8);memcpy(out->hub_ports[j],inventory_string_buffer,4);
    if(force&&!(status&256)&&!(status&8)){out->hub_port_result[j]=inventory_control(ep,0x23,3,8,j+1,0);if(!out->hub_port_result[j])out->driver_flags|=1u<<j;}}}}
 sceUsbdClosePipe(ep);
}
static int inventory_string(int ep,unsigned index,unsigned lang,char*out){unsigned i,k=0,limit;int r;
 if(!index)return 1;
 r=inventory_read(ep,index,lang);if(r)return r;
 if(inventory_bytes<2||inventory_string_buffer[1]!=3)return -1002;
 limit=inventory_string_buffer[0];if(limit>(unsigned)inventory_bytes)limit=inventory_bytes;
 for(i=2;i+1<limit&&k+3<96;i+=2){unsigned c=inventory_string_buffer[i]|(inventory_string_buffer[i+1]<<8);
  if(c>=0xd800&&c<=0xdfff)c='?';
  if(!c)break;
  if(c<128)out[k++]=c<32?'?':c;
  else if(c<2048){out[k++]=0xc0|(c>>6);out[k++]=0x80|(c&63);}
  else{out[k++]=0xe0|(c>>12);out[k++]=0x80|((c>>6)&63);out[k++]=0x80|(c&63);}}
 out[k]=0;return 0;
}
static void inventory_device(UsbInventoryDevice*out){unsigned id=out->id,j,lang=0x409;int ep;
 UsbDeviceDescriptor*d;UsbConfigDescriptor*c;
 memset(out,0,sizeof(*out));out->id=id;
 if(id>=USB_INVENTORY_MAX)return;
 d=sceUsbdScanStaticDescriptor(id,0,USB_DT_DEVICE);if(!d)return;
 out->present=1;memcpy(out->descriptor,d,18);out->location_result=sceUsbdGetDeviceLocation(id,out->location);
 c=sceUsbdScanStaticDescriptor(id,d,USB_DT_CONFIG);
 if(c){out->config_total=c->wTotalLength;out->config_length=out->config_total>USB_CONFIG_MAX?USB_CONFIG_MAX:out->config_total;memcpy(out->config,c,out->config_length);}
 ep=sceUsbdOpenPipe(id,0);
 if(ep<0){for(j=0;j<3;j++)out->string_result[j]=ep;return;}
 if(!inventory_read(ep,0,0)&&inventory_bytes>=4)lang=inventory_string_buffer[2]|(inventory_string_buffer[3]<<8);
 out->string_result[0]=inventory_string(ep,out->descriptor[14],lang,out->manufacturer);
 out->string_result[1]=inventory_string(ep,out->descriptor[15],lang,out->product);
 out->string_result[2]=inventory_string(ep,out->descriptor[16],lang,out->serial);
 if(out->descriptor[4]==9){
  out->hub_result=inventory_control(ep,0xa0,6,0x2900,0,64);
  if(!out->hub_result&&inventory_bytes>=3){out->hub_length=inventory_bytes>64?64:inventory_bytes;memcpy(out->hub_descriptor,inventory_string_buffer,out->hub_length);out->hub_port_count=out->hub_descriptor[2];
   for(j=0;j<out->hub_port_count&&j<8;j++){out->hub_port_result[j]=inventory_control(ep,0xa3,0,0,j+1,4);if(!out->hub_port_result[j]&&inventory_bytes==4)memcpy(out->hub_ports[j],inventory_string_buffer,4);else if(!out->hub_port_result[j])out->hub_port_result[j]=-1002;}}
 }
 /* USBD owns the default endpoint; ClosePipe on it is a no-op. */
 sceUsbdClosePipe(ep);
}
