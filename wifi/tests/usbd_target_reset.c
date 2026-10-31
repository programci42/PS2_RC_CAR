#include "usbd_watchdog_stub.h"
#include <assert.h>
#define USB_RC_BADDEV -3
typedef struct {u8 bLength,bDescriptorType,bDeviceClass;unsigned idVendor,idProduct;} UsbDeviceDescriptor;
static UsbDeviceDescriptor descriptor={18,1,0,0x178e,0x07d9};
static UsbdDevice_t root,relay,hub,peer;
static FakeOhci regs;static FakePool pool;FakePool*memPool=&pool;
static unsigned flushes,timers;
static UsbdDevice_t*fetchDeviceById(int id){return id==7?&relay:0;}
void flushPort(UsbdDevice_t*d){assert(d==&relay);flushes++;d->m_deviceStatus=DEVICE_NOTCONNECTED;}
void hubResetDevice(UsbdDevice_t*d){(void)d;}
int addTimerCallback(UsbdTimerCbStruct_t*t,TimerCallback cb,void*d,int ms){assert(t==&relay.m_timer&&cb==(TimerCallback)hubResetDevice&&d==&relay&&ms==501);timers++;return 0;}
#include "../third_party/ps2sdk-usbd/target_reset.h"
int main(void){pool.m_deviceTreeRoot=&root;pool.m_ohciRegs=&regs;relay.m_parent=&root;relay.m_attachedToPortNo=1;relay.m_staticDeviceDescPtr=&descriptor;relay.m_deviceStatus=DEVICE_READY;peer.m_deviceStatus=DEVICE_READY;
 assert(resetMatchedDevice(8,0x178e,0x07d9)==USB_RC_BADDEV);
 assert(resetMatchedDevice(7,0x0bda,0x8178)==USB_RC_BADDEV&&!flushes);
 descriptor.bDeviceClass=9;assert(resetMatchedDevice(7,0x178e,0x07d9)==USB_RC_BADDEV);descriptor.bDeviceClass=0;
 regs.HcRhPortStatus[0]=BIT(PORT_OVER_CURRENT);assert(resetMatchedDevice(7,0x178e,0x07d9)==USB_RC_BADDEV);regs.HcRhPortStatus[0]=257;
 assert(!resetMatchedDevice(7,0x178e,0x07d9)&&flushes==1&&timers==1&&relay.m_deviceStatus==DEVICE_CONNECTED&&peer.m_deviceStatus==DEVICE_READY);
 assert(resetMatchedDevice(7,0x178e,0x07d9)==USB_RC_BADDEV&&flushes==1);
 relay.m_deviceStatus=DEVICE_READY;relay.m_parent=&hub;assert(!resetMatchedDevice(7,0x178e,0x07d9)&&flushes==2);
 relay.m_deviceStatus=DEVICE_READY;relay.m_childListStart=&peer;assert(resetMatchedDevice(7,0x178e,0x07d9)==USB_RC_BADDEV&&flushes==2);
 puts("USBD_TARGET_RESET_TEST PASS actual helper identity/leaf/overcurrent guards root-and-hub-child retired once peer preserved");return 0;}
