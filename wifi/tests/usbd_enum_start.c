#include <assert.h>
#include <stdio.h>
#include <string.h>
#define DEVICE_RESETCOMPLETE 6
#define USB_DIR_IN 128
#define USB_RECIP_DEVICE 0
#define USB_REQ_GET_DESCRIPTOR 6
#define USB_DT_DEVICE 1
typedef struct Dev UsbdDevice_t;typedef struct Ep UsbdEndpoint_t;typedef struct Req UsbdIoRequest_t;
struct Ep{UsbdDevice_t*m_correspDevice;};struct Req{UsbdEndpoint_t*m_correspEndpoint;unsigned m_resultCode,m_transferedBytes;};
struct Dev{unsigned m_deviceStatus;unsigned char*m_staticDeviceDescPtr;unsigned m_timer;UsbdIoRequest_t m_ioRequest;};
static unsigned transfers,kills,timers,errors;static void(*callback)(UsbdIoRequest_t*);
static void hubSetFuncAddress(void*p){(void)p;}
static void usbdTraceEnumeration(UsbdDevice_t*d,unsigned stage,unsigned result){(void)d;assert(stage==5&&result);errors++;}
static void killDevice(UsbdDevice_t*d,UsbdEndpoint_t*e){(void)d;(void)e;kills++;}
static void addTimerCallback(unsigned*t,void(*cb)(void*),void*arg,unsigned delay){(void)t;assert(cb==hubSetFuncAddress&&arg&&delay==21);timers++;}
static void doControlTransfer(UsbdEndpoint_t*e,UsbdIoRequest_t*r,unsigned type,unsigned req,unsigned value,unsigned index,unsigned len,void*buf,void(*cb)(UsbdIoRequest_t*)){assert(e&&r&&buf&&type==128&&req==6&&value==256&&!index&&len==8);transfers++;callback=cb;}
#include "../third_party/ps2sdk-usbd/enum_start.h"
int main(void){unsigned char bytes[18]={18,1,0,0,0,0,0,8};UsbdDevice_t d={0};UsbdEndpoint_t e={&d};UsbdIoRequest_t r={&e,0,8};d.m_deviceStatus=6;d.m_staticDeviceDescPtr=bytes;
lowSpeedDescriptorStart(&e);assert(transfers==1&&callback);callback(&r);assert(timers==1&&!kills);r.m_resultCode=5;callback(&r);assert(kills==1&&errors==1&&timers==1);r.m_resultCode=0;r.m_transferedBytes=7;callback(&r);assert(kills==2);r.m_transferedBytes=8;bytes[7]=64;callback(&r);assert(kills==3);bytes[7]=8;d.m_deviceStatus=1;lowSpeedDescriptorStart(&e);callback(&r);assert(transfers==1&&kills==3&&timers==1);puts("USBD_ENUM_START PASS actual helper: default-address descriptor before address, valid low-speed packet, failure/short/malformed restart, unplug callback ignored");return 0;}
