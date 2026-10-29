// SPDX-License-Identifier: MIT. Exclusive physical USB; no network translation.
#include "UsbRtlPhysical.h"
#include <stdexcept>
#include <cstring>
#include <string>
static std::runtime_error Error(const char*where,int e){return std::runtime_error(std::string(where)+": "+libusb_error_name(e));}
CPhysicalRtlUsb::CPhysicalRtlUsb(){
 int r=libusb_init(&ctx);if(r)throw Error("libusb init",r);
 try{
  r=libusb_set_option(ctx,LIBUSB_OPTION_USE_USBDK);if(r)throw Error("UsbDk backend unavailable",r);
  libusb_device **list=nullptr;ssize_t n=libusb_get_device_list(ctx,&list);unsigned found=0;
  if(n<0)throw Error("Physical USB enumeration",(int)n);
  libusb_device*selected=nullptr;
  for(ssize_t i=0;i<n;i++){libusb_device_descriptor d{};if(!libusb_get_device_descriptor(list[i],&d)&&d.idVendor==0x0bda&&d.idProduct==0x8178){selected=list[i];found++;}}
  if(found!=1){libusb_free_device_list(list,1);throw std::runtime_error("Exactly one physical 0BDA:8178 is required");}
  r=libusb_open(selected,&handle);libusb_free_device_list(list,1);if(r)throw Error("Capture physical RTL8192CU",r);
  device.resize(18);r=libusb_control_transfer(handle,0x80,6,0x100,0,device.data(),18,2000);
  if(r!=18)throw Error("Physical device descriptor",r);
  config.resize(9);r=libusb_control_transfer(handle,0x80,6,0x200,0,config.data(),9,2000);
  if(r!=9)throw Error("Physical config descriptor",r);
  unsigned size=config[2]|(unsigned(config[3])<<8);if(size<9||size>4096)throw std::runtime_error("Invalid physical config size");
  config.resize(size);r=libusb_control_transfer(handle,0x80,6,0x200,0,config.data(),size,2000);
  if(r!=(int)size)throw Error("Physical full config descriptor",r);
  r=libusb_claim_interface(handle,interfaceNo);if(r)throw Error("Physical interface claim",r);claimed=true;
 }catch(...){if(handle)libusb_close(handle);libusb_exit(ctx);handle=nullptr;ctx=nullptr;throw;}
}
CPhysicalRtlUsb::~CPhysicalRtlUsb(){running=false;if(worker.joinable())worker.join();if(handle){if(claimed)libusb_release_interface(handle,interfaceNo);libusb_close(handle);}if(ctx)libusb_exit(ctx);}
int CPhysicalRtlUsb::Control(uint8_t type,uint8_t req,uint16_t value,uint16_t index,uint8_t*data,uint16_t n){
 controls++;return libusb_control_transfer(handle,type,req,value,index,data,n,2000);
}
int CPhysicalRtlUsb::Send(uint8_t ep,const uint8_t*data,int n){int count=0;
 int r=libusb_bulk_transfer(handle,ep,const_cast<uint8_t*>(data),n,&count,2000);if(r)return r;tx++;return count;
}
void CPhysicalRtlUsb::StartRx(uint8_t ep){
 if(running.exchange(true))return;
 worker=std::thread([this,ep](){while(running){std::vector<uint8_t>b(4096);int count=0;
  int r=libusb_bulk_transfer(handle,ep,b.data(),(int)b.size(),&count,100);
  if(!r&&count>0){b.resize(count);std::lock_guard<std::mutex>guard(lock);if(received.size()<64)received.push_back(std::move(b));}
  else if(r!=LIBUSB_ERROR_TIMEOUT&&r!=LIBUSB_ERROR_INTERRUPTED&&r!=0){running=false;break;}
 }});
}
bool CPhysicalRtlUsb::Receive(std::vector<uint8_t>&out){std::lock_guard<std::mutex>guard(lock);if(received.empty())return false;out=std::move(received.front());received.pop_front();rx++;return true;}
