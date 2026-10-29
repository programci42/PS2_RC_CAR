#ifndef USBD_WATCHDOG_STUB_H
#define USBD_WATCHDOG_STUB_H
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t u32;typedef uint8_t u8;
#define BIT(n) (1u<<(n))
#define PORT_CONNECTION 0
#define PORT_ENABLE 1
#define PORT_LOW_SPEED 9
#define PORT_RESET 4
#define C_PORT_CONNECTION 16
#define C_PORT_FLAGS 0x1f0000u
#define OHCI_INT_SF 4
#define DEVICE_NOTCONNECTED 1
#define DEVICE_CONNECTED 3
#define DEVICE_RESETDELAYED 4
#define DEVICE_RESETPENDING 5
#define DEVICE_RESETCOMPLETE 6
#define DEVICE_READY 8
#define dbg_printf(...) ((void)0)
typedef void(*TimerCallback)(void*);
typedef struct UsbdTimer {struct UsbdTimer*m_next,*m_prev;int m_isActive;unsigned m_delayCount;TimerCallback m_callbackProc;void*m_callbackArg;} UsbdTimerCbStruct_t;
typedef struct UsbdDevice {struct UsbdDevice*m_parent,*m_next,*m_childListStart;unsigned m_deviceStatus,m_resetFlag,m_attachedToPortNo,m_isLowSpeedDevice;UsbdTimerCbStruct_t m_timer;} UsbdDevice_t;
typedef struct {unsigned HcRhPortStatus[2],HcInterruptEnable,HcInterruptDisable;} FakeOhci;
typedef struct {unsigned m_delayResets;UsbdDevice_t*m_deviceTreeRoot;UsbdTimerCbStruct_t*m_timerListStart,*m_timerListEnd;FakeOhci*m_ohciRegs;} FakePool;
extern FakePool*memPool;
int addTimerCallback(UsbdTimerCbStruct_t*,TimerCallback,void*,int);
int cancelTimerCallback(UsbdTimerCbStruct_t*);
void handleTimerList(void);
void hubResetDevice(UsbdDevice_t*);
int checkDelayedResets(UsbdDevice_t*);
void hubResetDevicePort(UsbdDevice_t*);
void hubRestartStatusPoll(UsbdDevice_t*);
void flushPort(UsbdDevice_t*);
void*openDeviceEndpoint(UsbdDevice_t*,void*,unsigned);
int hubTimedSetFuncAddress(UsbdDevice_t*);
#endif
