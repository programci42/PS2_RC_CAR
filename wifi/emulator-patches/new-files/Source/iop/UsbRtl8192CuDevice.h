// SPDX-License-Identifier: MIT
#pragma once
#include "UsbDevice.h"
#include <array>
#include <vector>
#include <string>
#include <memory>
class CPhysicalRtlUsb;
class CIopBios;
namespace Iop {
// Restricted RTL8192CU laboratory profile: USB control reads and unencrypted
// 802.11 data descriptors. Not a complete RF/firmware/chipset emulation.
class CRtl8192CuUsbDevice final : public CUsbDevice {
public:
 static constexpr uint16 DEVICE_ID=0x0819;
 CRtl8192CuUsbDevice(CIopBios&,uint8*);
 ~CRtl8192CuUsbDevice() override;
 uint16 GetId() const override{return DEVICE_ID;}
 const char* GetLldName()const override{return "rtl8192cu_probe";}
 void CountTicks(uint32)override;
 void OnLldRegistered()override;
 uint32 ScanStaticDescriptor(uint32,uint32,uint32)override;
 int32 OpenPipe(uint32,uint32)override;
 int32 TransferPipe(uint32,uint32,uint32,uint32,uint32,uint32)override;
 std::string Summary()const;
private:
 void Trace(const std::string&)const;
 std::unique_ptr<CPhysicalRtlUsb> m_physical;
 bool SendWifi(const uint8*,uint32);
 void ReceiveDns(const uint8*,uint32);
 void CompleteRx();
 bool BootControl(uint8,uint16,uint16,uint8*);
 CIopBios& m_bios;uint8* m_ram;uint32 m_descriptors=0;
 uintptr_t m_socket=~uintptr_t(0);
 bool m_configured=false,m_lab=false,m_outstanding=false;
 uint32 m_rxPtr=0,m_rxSize=0,m_rxCb=0,m_rxArg=0;
 uint16 m_guestPort=0,m_dnsId=0;
 std::array<uint8,6> m_guestMac{};
 std::vector<uint8> m_rx;
 bool m_bootModel=false;
 std::vector<uint8> m_bootRegs,m_bootRam,m_bootFirmware;
 uint32 m_bootBytes=0;
 std::string m_dns,m_bindIp,m_trace,m_fault;
 uint32 m_control=0,m_usbTx=0,m_usbRx=0,m_wireTx=0,m_wireRx=0,m_wireTxBytes=0,m_wireRxBytes=0,m_rejected=0;
};
}
