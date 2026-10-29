#include "PS2VM.h"
#include "AppConfig.h"
#include "PathUtils.h"
#include "StdStream.h"
#include "iop/IopBios.h"
#include "gs/GSH_Null.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <atomic>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdlib>
// Drives the emulator's normal pad interface; no Windows keyboard injection.
// Enabled only for the synthetic WiFi UI integration test.
class WifiScriptedPad final:public CPadHandler {
 using B=PS2::CControllerInfo;
 std::vector<B::BUTTON> sequence;unsigned int frame=0;
public:
 WifiScriptedPad(){
  sequence={B::DPAD_DOWN,B::DPAD_UP,B::CROSS};
  const std::vector<std::string> rows={"1234567890-=","qwertyuiop[]","asdfghjkl;'\\","zxcvbnm,./`"};
  size_t row=0,col=0;
  auto bad=std::getenv("PLAY_WIFI_PAD_BAD_PASSWORD");
  for(char ch:std::string(bad&&std::string(bad)=="1"?"badpass1":"password")){
   size_t targetRow=0,targetCol=0;
   for(size_t i=0;i<rows.size();i++){auto p=rows[i].find(ch);if(p!=std::string::npos){targetRow=i;targetCol=p;break;}}
   while(row<targetRow){sequence.push_back(B::DPAD_DOWN);row++;col=std::min(col,rows[row].size()-1);}
   while(row>targetRow){sequence.push_back(B::DPAD_UP);row--;col=std::min(col,rows[row].size()-1);}
   while(col<targetCol){sequence.push_back(B::DPAD_RIGHT);col++;}
   while(col>targetCol){sequence.push_back(B::DPAD_LEFT);col--;}
   sequence.push_back(B::CROSS);
  }
  sequence.push_back(B::R2);for(int i=0;i<20;i++)sequence.push_back(B::MAX_BUTTONS);sequence.push_back(B::R1);
 }
 void Update(uint8*ram)override{
  auto active=B::MAX_BUTTONS;
  if(frame>=60){unsigned int tick=frame-60,step=tick/8;if(step<sequence.size()&&tick%8<4)active=sequence[step];}
  for(auto*listener:m_interfaces){
   for(unsigned int i=0;i<B::MAX_BUTTONS;i++){
    auto button=static_cast<B::BUTTON>(i);
    if(B::IsAxis(button))listener->SetAxisState(0,button,127,ram);
    else listener->SetButtonState(0,button,button==active,ram);
   }
  }
  frame++;
 }
};
fs::path CAppConfig::GetBasePath()const
{
 const fs::path base("rtl-vm-data");Framework::PathUtils::EnsurePathExists(base);return base;
}
int main(int argc,char**argv)
{
 if(argc!=3){std::cerr<<"usage: play-rtl8192cu-nettest ELF stdout-log\n";return 2;}
 std::atomic<bool> finished{false};
 CPS2VM vm;vm.Initialize();vm.CreateGSHandler(CGSH_Null::GetFactoryFunction());
 if(auto p=std::getenv("PLAY_WIFI_PAD_TEST");p&&std::string(p)=="1")vm.CreatePadHandler([](){return new WifiScriptedPad();});
 auto exitConnection=vm.m_ee->m_os->OnRequestExit.Connect([&](){finished=true;});
 vm.m_ee->m_os->BootFromFile(fs::path(argv[1]));
 auto bios=dynamic_cast<CIopBios*>(vm.m_iop->m_bios.get());
 auto* guestStdout=new Framework::CStdStream(argv[2],"wb");
 bios->GetIoman()->SetFileStream(Iop::CIoman::FID_STDOUT,guestStdout);
 vm.Resume();
 int limit=1500;if(auto physical=std::getenv("PLAY_RTL_PHYSICAL");physical&&std::string(physical)=="1")limit=6000;
 const auto keepAliveEnv=std::getenv("PLAY_HTTP_KEEP_ALIVE");
 const bool keepAlive=keepAliveEnv&&std::string(keepAliveEnv)=="1";
 for(uint64_t i=0;!finished&&(keepAlive||i<static_cast<uint64_t>(limit));i++){std::this_thread::sleep_for(std::chrono::milliseconds(10));if(i%10==0)guestStdout->Flush();}
 vm.Pause();vm.DestroyPadHandler();vm.DestroyGSHandler();vm.Destroy();
 std::ifstream in(argv[2]);std::ostringstream text;text<<in.rdbuf();std::cout<<text.str();
 return text.str().find("PHYSICAL_JOIN_TEST PASS ")!=std::string::npos||text.str().find("PHYSICAL_RADIO_TEST PASS ")!=std::string::npos||text.str().find("PHYSICAL_PROTOCOL_TEST PASS ")!=std::string::npos||text.str().find("USB_NET_TEST PASS rounds=5")!=std::string::npos||text.str().find("BOOT_MODEL_TEST PASS path=")!=std::string::npos||text.str().find("WPA_CRYPTO_TEST PASS checks=")!=std::string::npos||text.str().find("SCAN_PARSER_TEST PASS checks=")!=std::string::npos||text.str().find("WIFI_FLOW_TEST PASS synthetic-state-and-packets")!=std::string::npos||text.str().find("WIFI_PAD_TEST PASS actual-pad-path")!=std::string::npos||text.str().find("WIFI_PAD_REJECT_TEST PASS no-profile-no-IP")!=std::string::npos?0:1;
}
