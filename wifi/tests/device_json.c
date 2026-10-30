#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
static struct {unsigned pad_connected;} runtime;
static struct {unsigned ready;} robot;
static struct {int status;} robot_packet;
static unsigned char physical_mac[6];static int physical_radio_ready,pad_state;static unsigned pad_reported_mode;
static void wifi_files_native_devices(char*out,unsigned cap){snprintf(out,cap,"[]");}
#include "../ee/device_inventory.h"
int main(void){static struct {char out[28000];char canary[16];} r;unsigned i,next=1;FILE*f;
 devices_root[0]=0x301;devices_root[2]=(3u<<24)|(5u<<16)|(4u<<8)|1;devices_root_format();assert(strstr(devices_root_json,"\"lastUsbError\":5")&&strstr(devices_root_json,"\"lowSpeed\":1"));
 devices_due=0;memset(r.canary,0x55,sizeof(r.canary));devices_generation=1;
 for(i=1;i<32;i++){UsbInventoryDevice*d=&devices[i];d->id=i;d->present=1;memset(d->product,1,95);memset(d->manufacturer,'"',95);memset(d->serial,'\\',95);d->descriptor[4]=9;d->hub_port_count=8;d->config_length=d->config_total=9;d->config[0]=9;d->config[1]=2;}
 for(i=0;next&&i<32;i++){char route[64];snprintf(route,sizeof(route),"/devices?from=%u",next);assert(devices_http("GET",route,r.out,sizeof(r.out))==200);for(unsigned k=0;k<16;k++)assert(r.canary[k]==0x55);f=fopen("reports/device-json-pages.jsonl",i?"ab":"wb");assert(f);fputs(r.out,f);fputc('\n',f);fclose(f);{char*p=strstr(r.out,"\"nextId\":");assert(p);sscanf(p+9,"%u",&next);}}
 assert(!next);assert(devices_http("GET","/?usb=1",r.out,sizeof(r.out))==200);f=fopen("reports/device-json-detail.json","wb");assert(f);fputs(r.out,f);fclose(f);printf("DEVICE_JSON_TEST PASS %u pages, bounded worst-case strings/hub data/detail\n",i);return 0;}
