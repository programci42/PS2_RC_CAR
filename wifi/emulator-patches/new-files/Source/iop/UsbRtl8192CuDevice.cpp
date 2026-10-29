// SPDX-License-Identifier: MIT
#include <winsock2.h>
#include <ws2tcpip.h>
#include "UsbRtl8192CuDevice.h"
#include "UsbRtlPhysical.h"
#include "IopBios.h"
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iterator>
#include <algorithm>
using namespace Iop;
static uint16 BE16(const uint8*p){return (uint16(p[0])<<8)|p[1];}
static uint16 LE16(const uint8*p){return p[0]|(uint16(p[1])<<8);}
static void PutBE(uint8*p,uint16 v){p[0]=v>>8;p[1]=v;}
static uint16 Checksum(const uint8*p,size_t n){uint32 s=0;while(n>1){s+=BE16(p);p+=2;n-=2;}if(n)s+=uint32(*p)<<8;while(s>>16)s=(s&65535)+(s>>16);return uint16(~s);}
static std::string Env(const char*n,const char*d=""){auto v=std::getenv(n);return v?v:d;}
static constexpr uint8 AP[6]={2,0x50,0x53,0x32,0,1};
static constexpr uint8 SNAP[8]={0xaa,0xaa,3,0,0,0,8,0};
CRtl8192CuUsbDevice::CRtl8192CuUsbDevice(CIopBios&bios,uint8*ram):m_bios(bios),m_ram(ram)
{
 m_dns=Env("PLAY_RTL_DNS_SERVER");m_bindIp=Env("PLAY_RTL_ETH_IP");
 m_trace=Env("PLAY_RTL_TRACE");m_fault=Env("PLAY_RTL_FAULT");
 if(Env("PLAY_RTL_PHYSICAL")=="1"){
  m_physical=std::make_unique<CPhysicalRtlUsb>();
  Trace("PHYSICAL_USB_CAPTURE 0BDA:8178 no-model no-Ethernet-translation");return;
 }
 m_bootModel=Env("PLAY_RTL_BOOT_MODEL")=="1";
 if(m_bootModel){
  std::ifstream f(Env("PLAY_RTL_FIRMWARE"),std::ios::binary);
  m_bootFirmware.assign(std::istreambuf_iterator<char>(f),{});
  if(m_bootFirmware.size()!=13540)throw std::runtime_error("Exact RTL92C reference firmware required for boot register model");
  m_bootRegs.assign(65536,0);m_bootRam.assign(32768,0);
  m_bootRegs[0xf3]=m_fault=="boot-chip"?0:8;m_bootRegs[4]=m_fault=="boot-autoload"?0:2;
  m_bootRegs[0]=1;m_bootRegs[1]=2;m_bootRegs[0x600]=0x40;
  m_bootRegs[0xfe66]=m_fault=="boot-endpoints"?0:0x32;
  Trace("MODEL_INIT profile=power-firmware-register-model firmware_8051_execution=NO physical_wifi=NO");return;
 }
 WSADATA wsa{};if(WSAStartup(MAKEWORD(2,2),&wsa))throw std::runtime_error("Winsock initialization failed");
 if(m_dns.empty()||m_bindIp.empty())throw std::runtime_error("Explicit Ethernet source IP and DNS server are required");
 SOCKET s=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
 if(s==INVALID_SOCKET)throw std::runtime_error("UDP socket failed");
 sockaddr_in local{};local.sin_family=AF_INET;
 sockaddr_in remote{};remote.sin_family=AF_INET;remote.sin_port=htons(53);
 if(inet_pton(AF_INET,m_bindIp.c_str(),&local.sin_addr)!=1||inet_pton(AF_INET,m_dns.c_str(),&remote.sin_addr)!=1
  ||bind(s,reinterpret_cast<sockaddr*>(&local),sizeof(local))||connect(s,reinterpret_cast<sockaddr*>(&remote),sizeof(remote)))
 {closesocket(s);throw std::runtime_error("Ethernet UDP bind/connect failed");}
 u_long nonblock=1;if(ioctlsocket(s,FIONBIO,&nonblock)){closesocket(s);throw std::runtime_error("Nonblocking socket setup failed");}
 m_socket=s;Trace("MODEL_INIT usb=0bda:8178 profile=restricted-80211-data ethernet_source="+m_bindIp+" dns="+m_dns);
}
CRtl8192CuUsbDevice::~CRtl8192CuUsbDevice(){Trace(Summary());if(m_socket!=~uintptr_t(0))closesocket(SOCKET(m_socket));if(!m_bootModel&&!m_physical)WSACleanup();}
void CRtl8192CuUsbDevice::Trace(const std::string&s)const{if(!m_trace.empty()){std::ofstream f(m_trace,std::ios::app);f<<s<<"\n";}}
std::string CRtl8192CuUsbDevice::Summary()const{
 if(m_physical){std::ostringstream s;s<<"PHYSICAL_USB_SUMMARY control="<<m_physical->controls<<" bulk_out="<<m_physical->tx<<" bulk_in="<<m_physical->rx;return s.str();}
 std::ostringstream s;s<<"MODEL_SUMMARY control="<<m_control<<" usb_bulk_out="<<m_usbTx<<" usb_bulk_in="<<m_usbRx
 <<" ethernet_udp_tx="<<m_wireTx<<" ethernet_udp_rx="<<m_wireRx<<" ethernet_tx_bytes="<<m_wireTxBytes<<" ethernet_rx_bytes="<<m_wireRxBytes<<" rejected="<<m_rejected
 <<" boot_ram_bytes="<<m_bootBytes;return s.str();
}
bool CRtl8192CuUsbDevice::BootControl(uint8 type,uint16 a,uint16 n,uint8*data){
 if(!n||a+uint32(n)>65536)return false;
 if(type==0xc0&&(n==1||n==2||n==4)){memcpy(data,m_bootRegs.data()+a,n);return true;}
 if(type!=0x40||n>196)return false;
 if(a>=0x1000&&a+uint32(n)<=0x2000){
  if(m_fault=="boot-usb"||!(m_bootRegs[0x80]&1))return false;
  uint32 off=(m_bootRegs[0x82]&7)*4096+a-0x1000;
  memcpy(m_bootRam.data()+off,data,n);m_bootBytes+=n;return true;
 }
 bool allowed=(a==0&&(n==1||n==2))||(a==2&&n==2)||(a==4&&n==2)||
  ((a==0x11||a==0x1c||a==0x21||a==0x81||a==0x82||a==0x1cf||a==0x600||a==0xfe10)&&n==1)||
  (a==0x80&&(n==1||n==4))||(a==0x100&&n==2)||
  ((a==0x1e0||a==0x200)&&n==4)||
  ((a==0x214||a==0x424||a==0x425||a==0x45d||a==0x114||a==0x209||a==0x104)&&n==1)||
  ((a==0x10c||a==0x116)&&n==2);
 if(!allowed)return false;
 uint8 old=m_bootRegs[0x80];memcpy(m_bootRegs.data()+a,data,n);
 if(a==4&&(data[1]&1)&&m_fault!="boot-power")m_bootRegs[5]&=~1;
 if(a==0x600&&m_fault=="boot-mac")m_bootRegs[a]|=0x80;
 if(a==0x1cf)m_bootRegs[3]&=~4;
 if(a==0x1e0&&m_fault!="boot-llt")m_bootRegs[0x1e3]&=0x3f;
 if(a==0x80){
  m_bootRegs[a]=(data[0]&~4)|(old&4);if(data[0]&4)m_bootRegs[a]&=~4;
  bool exact=m_bootBytes==m_bootFirmware.size()-32&&std::equal(m_bootFirmware.begin()+32,m_bootFirmware.end(),m_bootRam.begin());
  if((old&1)&&!(data[0]&1)&&exact&&m_fault!="boot-checksum"){
   m_bootRegs[a]|=4;Trace("BOOT_FIRMWARE_RAM exact_reference_match=YES bytes="+std::to_string(m_bootBytes));
  }
  if((data[0]&2)&&exact&&m_fault!="boot-ready"){m_bootRegs[a]|=0x40;Trace("BOOT_READY_BIT model_only=YES 8051_execution=NO");}
 }
 return true;
}
void CRtl8192CuUsbDevice::OnLldRegistered(){
 if(m_physical){m_descriptors=m_bios.GetSysmem()->AllocateMemory(0x1200,0,0);
  if(!m_descriptors)throw std::runtime_error("Physical descriptor memory allocation failed");
  memcpy(m_ram+m_descriptors,m_physical->device.data(),m_physical->device.size());
  memcpy(m_ram+m_descriptors+32,m_physical->config.data(),m_physical->config.size());
  Trace("PHYSICAL_USB_ATTACH actual_descriptor_bytes="+std::to_string(m_physical->config.size()));return;
 }
 m_descriptors=m_bios.GetSysmem()->AllocateMemory(0x100,0,0);
 if(!m_descriptors)throw std::runtime_error("RTL USB descriptor allocation failed");
 static const uint8 dev[18]={18,1,0,2,0,0,0,64,0xda,0x0b,0x78,0x81,0,2,0,0,0,1};
 static const uint8 config[39]={9,2,39,0,1,1,0,0x80,250,9,4,0,0,3,255,255,255,0,7,5,0x81,2,64,0,0,7,5,2,2,64,0,0,7,5,3,2,64,0,0};
 memcpy(m_ram+m_descriptors,dev,sizeof(dev));memcpy(m_ram+m_descriptors+32,config,sizeof(config));
 Trace("USB_ATTACH VID=0BDA PID=8178 IN=81 OUT=02,03 fullspeed=64");
}
uint32 CRtl8192CuUsbDevice::ScanStaticDescriptor(uint32 id,uint32 prev,uint32 type){
 if(id!=DEVICE_ID||!m_descriptors)return 0;
 if(type==1)return m_descriptors;
 if(type==2)return m_descriptors+32;
 if(m_physical){for(uint32 off=32;off<32+m_physical->config.size();){uint32 ptr=m_descriptors+off;uint8 len=m_ram[ptr];if(len<2||off+len>32+m_physical->config.size())break;if(ptr>prev&&m_ram[ptr+1]==type)return ptr;off+=len;}return 0;}
 for(uint32 off=41;off<m_descriptors+71-m_descriptors;){
  uint32 ptr=m_descriptors+off;uint8 len=m_ram[ptr];
  if(len<2||off+len>71)break;
  if(m_ram[ptr+1]==type&&(!prev||ptr>prev))return ptr;
  off+=len;
 }
 return 0;
}
int32 CRtl8192CuUsbDevice::OpenPipe(uint32 id,uint32 ptr){
 if(id!=DEVICE_ID)return -1;if(!ptr)return 0;
 if(m_physical){if(ptr<m_descriptors+32||ptr+7>m_descriptors+32+m_physical->config.size()||m_ram[ptr+1]!=5)return -1;return m_ram[ptr+2];}
 if(ptr<m_descriptors+50||ptr+7>m_descriptors+71||m_ram[ptr+1]!=5)return -1;
 uint8 ep=m_ram[ptr+2];return ep==0x81||ep==2||ep==3?ep:-1;
}
int32 CRtl8192CuUsbDevice::TransferPipe(uint32 pipe,uint32 buffer,uint32 size,uint32 option,uint32 cb,uint32 arg){
 uint16 ep=pipe>>16;
 if((pipe&65535)!=DEVICE_ID||buffer>0x200000||size>0x200000-buffer)return -1;
 if(m_physical){
  if(!ep){if(option>0x200000-8)return -1;const uint8*r=m_ram+option;
   if(LE16(r+6)!=size)return -1;
   int n=m_physical->Control(r[0],r[1],LE16(r+2),LE16(r+4),m_ram+buffer,(uint16)size);
   Trace("PHYSICAL_CONTROL request="+std::to_string(r[1])+" address="+std::to_string(LE16(r+2))+" count="+std::to_string(n));
   m_bios.TriggerCallback(cb,n<0?1:0,n<0?0:n,arg);return 0;
  }
  if(ep&0x80){if(m_rxCb)return -1;m_rxPtr=buffer;m_rxSize=size;m_rxCb=cb;m_rxArg=arg;m_physical->StartRx((uint8)ep);return 0;}
  if(!m_trace.empty()){std::ofstream f(m_trace+".tx.bin",std::ios::binary);f.write(reinterpret_cast<const char*>(m_ram+buffer),size);}
  int n=m_physical->Send((uint8)ep,m_ram+buffer,(int)size);m_bios.TriggerCallback(cb,n<0?1:0,n<0?0:n,arg);return 0;
 }
 if(!ep){
  if(option>0x200000-8)return -1;const uint8*r=m_ram+option;int result=1,count=0;m_control++;
  uint16 value=LE16(r+2),index=LE16(r+4),len=LE16(r+6);
  if(r[0]==0&&r[1]==9&&value==1&&!len){m_configured=true;result=0;Trace("USB_SET_CONFIGURATION 1");}
  else if(m_bootModel&&r[1]==5&&!index&&len==size){
   if(BootControl(r[0],value,len,m_ram+buffer)){result=0;count=size;}
  }
  else if(!m_bootModel&&r[0]==0xc0&&r[1]==5&&!index&&len==size&&(len==1||len==2||len==4)){
   uint32 reg=0;bool known=true;
   switch(value){case 0:reg=0x16;break;case 2:reg=0x2003;break;case 0x80:reg=0;break;case 0xf0:reg=0x10000;break;
    case 0xfff0:reg=0x4c545256;m_lab=true;Trace("LAB_CAPABILITY VRTL: explicit preassociated unencrypted test profile");break;default:known=false;}
   if(known){for(uint32 i=0;i<size;i++)m_ram[buffer+i]=reg>>(8*i);result=0;count=size;}
  }
  if(result)m_rejected++;
  m_bios.TriggerCallback(cb,result,count,arg);return 0;
 }
 if(!m_configured||!m_lab){m_rejected++;m_bios.TriggerCallback(cb,1,0,arg);return 0;}
 if(ep==2||ep==3){bool ok=SendWifi(m_ram+buffer,size);if(ok)m_usbTx++;else m_rejected++;m_bios.TriggerCallback(cb,ok?0:1,ok?size:0,arg);return 0;}
 if(ep==0x81){if(m_rxCb)return -1;m_rxPtr=buffer;m_rxSize=size;m_rxCb=cb;m_rxArg=arg;CompleteRx();return 0;}
 return -1;
}
bool CRtl8192CuUsbDevice::SendWifi(const uint8*p,uint32 n){
 if(n<32+24+8+28+12||n>2048||p[2]!=32||(p[3]&0x8c)!=0x8c||LE16(p)+32!=n)return false;
 uint16 xor16=0;for(int i=0;i<32;i+=2)xor16^=LE16(p+i);if(xor16)return false;
 const uint8*w=p+32;uint32 wn=n-32;if(LE16(w)!=0x0108||memcmp(w+24,SNAP,8))return false;
 const uint8*ip=w+32;if(ip[0]!=0x45||ip[9]!=17||Checksum(ip,20)||BE16(ip+2)!=wn-32)return false;
 const uint8 source[4]={10,0,2,15},target[4]={10,0,2,3};
 if(memcmp(ip+12,source,4)||memcmp(ip+16,target,4))return false;
 const uint8*udp=ip+20;uint16 un=BE16(udp+4);if(un<20||un!=BE16(ip+2)-20||BE16(udp+2)!=53||un-8>512||m_outstanding)return false;
 m_guestPort=BE16(udp);memcpy(m_guestMac.data(),w+10,6);m_dnsId=BE16(udp+8);
 Trace("USB_BULK_OUT bytes="+std::to_string(n)+" dns_id="+std::to_string(m_dnsId));
 if(!m_trace.empty()){std::ofstream f(m_trace+".tx.bin",std::ios::binary);f.write(reinterpret_cast<const char*>(p),n);}
 if(m_fault=="drop"){Trace("FAULT_DROP: no Ethernet request sent");return true;}
 int sent=send(SOCKET(m_socket),reinterpret_cast<const char*>(udp+8),un-8,0);
 if(sent!=un-8){Trace("ETHERNET_SEND_ERROR "+std::to_string(WSAGetLastError()));return false;}
 m_wireTx++;m_wireTxBytes+=sent;m_outstanding=true;Trace("ETHERNET_UDP_TX bytes="+std::to_string(sent)+" dns_id="+std::to_string(m_dnsId));return true;
}
void CRtl8192CuUsbDevice::ReceiveDns(const uint8*d,uint32 n){
 m_wireRx++;m_wireRxBytes+=n;Trace("ETHERNET_UDP_RX bytes="+std::to_string(n)+" dns_id="+std::to_string(n>=2?BE16(d):0));
 uint32 wireless=24+8+20+8+n;m_rx.assign(24+wireless,0);
 m_rx[0]=wireless;m_rx[1]=wireless>>8;
 uint8*w=m_rx.data()+24;w[0]=8;w[1]=2;memcpy(w+4,m_guestMac.data(),6);memcpy(w+10,AP,6);memcpy(w+16,AP,6);memcpy(w+24,SNAP,8);
 uint8*ip=w+32;ip[0]=0x45;PutBE(ip+2,20+8+n);ip[8]=64;ip[9]=17;
 const uint8 source[4]={10,0,2,3},target[4]={10,0,2,15};memcpy(ip+12,source,4);memcpy(ip+16,target,4);PutBE(ip+10,Checksum(ip,20));
 uint8*udp=ip+20;PutBE(udp,53);PutBE(udp+2,m_guestPort);PutBE(udp+4,8+n);memcpy(udp+8,d,n);
 if(m_fault=="corrupt"&&n>=2){udp[8]^=1;Trace("FAULT_CORRUPT DNS transaction ID");}
 CompleteRx();
}
void CRtl8192CuUsbDevice::CompleteRx(){
 if(!m_rxCb||m_rx.empty())return;auto cb=m_rxCb,arg=m_rxArg;uint32 n=m_rx.size();m_rxCb=0;
 if(n>m_rxSize){m_rejected++;m_bios.TriggerCallback(cb,1,0,arg);}else{
  if(!m_trace.empty()){std::ofstream f(m_trace+".rx.bin",std::ios::binary);f.write(reinterpret_cast<const char*>(m_rx.data()),n);}
  if(m_physical&&!m_trace.empty()&&n>=24){unsigned off=24+(m_rx[2]&15)*8+(m_rx[3]&3);if(off+24+8+99<=n){const uint8*w=m_rx.data()+off;unsigned h=w[0]==0x88?26:24;if(off+h+8+99<=n&&w[h+6]==0x88&&w[h+7]==0x8e){std::ofstream f(m_trace+".eapol-in.bin",std::ios::binary|std::ios::app);uint32 size=n;f.write(reinterpret_cast<const char*>(&size),4);f.write(reinterpret_cast<const char*>(m_rx.data()),n);}}}
  memcpy(m_ram+m_rxPtr,m_rx.data(),n);m_usbRx++;Trace("USB_BULK_IN bytes="+std::to_string(n));m_bios.TriggerCallback(cb,0,n,arg);
 }m_rx.clear();
}
void CRtl8192CuUsbDevice::CountTicks(uint32){
 if(m_physical){if(m_rxCb&&m_physical->Receive(m_rx))CompleteRx();return;}
 if(!m_outstanding)return;uint8 bytes[2048];int n=recv(SOCKET(m_socket),reinterpret_cast<char*>(bytes),sizeof(bytes),0);
 if(n>0){m_outstanding=false;ReceiveDns(bytes,n);}else if(n<0&&WSAGetLastError()!=WSAEWOULDBLOCK){m_outstanding=false;Trace("ETHERNET_RECEIVE_ERROR "+std::to_string(WSAGetLastError()));}
}
