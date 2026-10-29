// SPDX-License-Identifier: MIT
#pragma once
#include <libusb.h>
#include <vector>
#include <thread>
#include <mutex>
#include <deque>
#include <atomic>
#include <cstdint>
class CPhysicalRtlUsb {
public:
 CPhysicalRtlUsb();~CPhysicalRtlUsb();
 std::vector<uint8_t> device,config;
 int Control(uint8_t,uint8_t,uint16_t,uint16_t,uint8_t*,uint16_t);
 int Send(uint8_t,const uint8_t*,int);
 void StartRx(uint8_t);bool Receive(std::vector<uint8_t>&);
 uint64_t controls=0,tx=0,rx=0;
private:
 libusb_context*ctx=nullptr;libusb_device_handle*handle=nullptr;
 int interfaceNo=0;bool claimed=false;
 std::atomic<bool>running{false};std::thread worker;std::mutex lock;
 std::deque<std::vector<uint8_t>>received;
};
