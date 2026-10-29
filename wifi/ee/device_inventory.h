#include "usb_inventory.h"
static UsbInventoryDevice devices[USB_INVENTORY_MAX];
static unsigned devices_cursor,devices_scanning,devices_generation;static uint32_t devices_due;
static char devices_cards[384]="[]";
static unsigned usb_word(const unsigned char*p){return p[0]|(p[1]<<8);}
static void device_quote(char*out,unsigned cap,const char*s){unsigned n=0;while(*s&&n+7<cap){unsigned c=(unsigned char)*s++;if(c=='"'||c=='\\'){out[n++]='\\';out[n++]=c;}else if(c<32)n+=snprintf(out+n,cap-n,"\\u%04x",c);else out[n++]=c;}out[n]=0;}
static unsigned device_json(const UsbInventoryDevice*d,char*out,unsigned cap,int detail){char a[576],b[576],c[576];unsigned n,j;
 device_quote(a,sizeof(a),d->manufacturer);device_quote(b,sizeof(b),d->product);device_quote(c,sizeof(c),d->serial);
 n=snprintf(out,cap,"{\"id\":%u,\"vid\":\"%04X\",\"pid\":\"%04X\",\"manufacturer\":\"%s\",\"product\":\"%s\",\"serial\":\"%s\",\"class\":%u,\"subclass\":%u,\"protocol\":%u,\"usbBcd\":%u,\"deviceBcd\":%u,\"maxPacket0\":%u,\"configurations\":%u,\"configuration\":%u,\"interfaces\":%u,\"powerMa\":%u,\"attributes\":%u,\"locationResult\":%d,\"path\":[%u,%u,%u,%u,%u,%u],\"stringResult\":[%d,%d,%d],\"configBytes\":%u,\"configTotal\":%u,\"mac\":",
 d->id,usb_word(d->descriptor+8),usb_word(d->descriptor+10),a,b,c,d->descriptor[4],d->descriptor[5],d->descriptor[6],usb_word(d->descriptor+2),usb_word(d->descriptor+12),d->descriptor[7],d->descriptor[17],d->config_length>=9?d->config[5]:0,d->config_length>=9?d->config[4]:0,d->config_length>=9?d->config[8]*2:0,d->config_length>=9?d->config[7]:0,d->location_result,d->location[0],d->location[1],d->location[2],d->location[3],d->location[4],d->location[5],d->string_result[0],d->string_result[1],d->string_result[2],d->config_length,d->config_total);
 if(n>=cap)return cap;
 if(physical_radio_ready&&d->driver_flags)n+=snprintf(out+n,cap-n,"\"%02X:%02X:%02X:%02X:%02X:%02X\"",physical_mac[0],physical_mac[1],physical_mac[2],physical_mac[3],physical_mac[4],physical_mac[5]);else n+=snprintf(out+n,cap-n,"null");
 if(d->descriptor[4]==9){n+=snprintf(out+n,cap-n,",\"hubResult\":%d,\"hubPortCount\":%u,\"ports\":[",d->hub_result,d->hub_port_count);for(j=0;j<d->hub_port_count&&j<8;j++){unsigned status=usb_word(d->hub_ports[j]);n+=snprintf(out+n,cap-n,"%s{\"port\":%u,\"result\":%d,\"status\":%u,\"change\":%u,\"connected\":%u,\"enabled\":%u,\"power\":%u,\"overcurrent\":%u,\"lowSpeed\":%u}",j?",":"",j+1,d->hub_port_result[j],status,usb_word(d->hub_ports[j]+2),!!(status&1),!!(status&2),!!(status&256),!!(status&8),!!(status&512));}n+=snprintf(out+n,cap-n,"]");}
 if(detail){n+=snprintf(out+n,cap-n,",\"deviceDescriptorHex\":\"");for(j=0;j<18&&n+3<cap;j++)n+=snprintf(out+n,cap-n,"%02X",d->descriptor[j]);n+=snprintf(out+n,cap-n,"\",\"configurationHex\":\"");for(j=0;j<d->config_length&&n+3<cap;j++)n+=snprintf(out+n,cap-n,"%02X",d->config[j]);n+=snprintf(out+n,cap-n,"\",\"hubDescriptorHex\":\"");for(j=0;j<d->hub_length&&n+3<cap;j++)n+=snprintf(out+n,cap-n,"%02X",d->hub_descriptor[j]);n+=snprintf(out+n,cap-n,"\"");}
 if(detail){unsigned emitted=0;n+=snprintf(out+n,cap-n,",\"interfaceEndpoints\":[");for(j=0;j+2<=d->config_length&&n+320<cap;){const unsigned char*p=d->config+j;unsigned len=p[0];if(len<2||len>d->config_length-j)break;
  if(p[1]==4&&len>=9){n+=snprintf(out+n,cap-n,"%s{\"type\":\"interface\",\"number\":%u,\"alternate\":%u,\"endpoints\":%u,\"class\":%u,\"subclass\":%u,\"protocol\":%u}",emitted++?",":"",p[2],p[3],p[4],p[5],p[6],p[7]);}
  else if(p[1]==5&&len>=7){n+=snprintf(out+n,cap-n,"%s{\"type\":\"endpoint\",\"address\":%u,\"transferType\":%u,\"maxPacket\":%u,\"interval\":%u}",emitted++?",":"",p[2],p[3]&3,usb_word(p+4),p[6]);}
  j+=len;}
  n+=snprintf(out+n,cap-n,"],\"descriptorListTruncated\":%s}",j<d->config_length?"true":"false");return n;}
 if(n+2<cap)n+=snprintf(out+n,cap-n,"}");
 return n;
}
static int devices_http(const char*method,const char*target,char*out,unsigned cap){unsigned n,i,count=0,id,from=1;
 if(!strncmp(target,"/?usb=",6)){if(strcmp(method,"GET"))return 405;if(sscanf(target+6,"%u",&id)!=1||id>=USB_INVENTORY_MAX||!devices[id].present)return 404;device_json(&devices[id],out,cap,1);return 200;}
 if(!strncmp(target,"/devices?from=",14)){if(sscanf(target+14,"%u",&from)!=1||from<1||from>=USB_INVENTORY_MAX)return 400;}
 else if(strcmp(target,"/?ls=devices")&&strcmp(target,"/devices")&&strcmp(target,"/devices/refresh"))return 0;
 if(strcmp(method,"GET"))return 405;
 if(!devices_generation||!strcmp(target,"/devices/refresh")){if(!devices_scanning){devices_cursor=1;devices_scanning=1;devices_generation++;wifi_files_native_devices(devices_cards,sizeof(devices_cards));}}
 for(i=1;i<USB_INVENTORY_MAX;i++)if(devices[i].present)count++;
 n=snprintf(out,cap,"{\"generation\":%u,\"scanning\":%u,\"count\":%u,\"scanId\":%u,\"padConnected\":%u,\"relayReady\":%u,\"relayUsbStatus\":%d,\"note\":\"USB metadata as supplied by hardware; power-only cables do not enumerate. Empty strings/MAC=null mean unavailable. /?usb=ID returns raw descriptors including interfaces and endpoints. Refresh: /devices/refresh\",\"devices\":[",devices_generation,devices_scanning,count,devices_cursor,runtime.pad_connected,robot.ready,robot_packet.status);
 count=0;for(i=from;i<USB_INVENTORY_MAX;i++)if(devices[i].present){if(n+5000>=cap)break;if(count++)out[n++]=',';n+=device_json(&devices[i],out+n,cap-n,0);}
 snprintf(out+n,cap-n,"],\"listed\":%u,\"truncated\":%s,\"nextId\":%u,\"memoryCards\":%s,\"gamepad\":{\"port\":0,\"slot\":0,\"state\":%d,\"reportedMode\":%u,\"manufacturer\":null,\"mac\":null}}",count,i<USB_INVENTORY_MAX?"true":"false",i<USB_INVENTORY_MAX?i:0,devices_cards,pad_state,pad_reported_mode);return 200;
}
