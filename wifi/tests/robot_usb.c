/* Runs the production EE USB scheduler with asynchronous public RPC doubles. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "robot_control.h"
#include "robot_program.h"
#include "wifi_files.h"
#include "seda_relay_rpc.h"
#define SIF_RPC_M_NOWAIT 1
typedef struct {int server;} SifRpcClientData_t;
static RobotControl robot;
static WifiFiles file_service;
static struct {unsigned relay_ready,owner,mask,left,right,file_blocked;} http;
unsigned char seda_relay_irx[1];unsigned int size_seda_relay_irx=1;
static unsigned card=2,mask=255,calls,checks,pending,now,off_time[8],fail_next,binds;
static SedaRelayRpcPacket*reply;
static int SifExecModuleBuffer(void*a,unsigned b,int c,void*d,int*e){(void)a;(void)b;(void)c;(void)d;*e=0;return 1;}
static int SifBindRpc(SifRpcClientData_t*c,unsigned id,int mode){(void)mode;if(id!=SEDA_RELAY_RPC_ID)exit(1);c->server=++binds>1;return 0;}
#define U(x) do{checks++;if(!(x)){printf("ROBOT_USB_TEST FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static int SifCallRpc(SifRpcClientData_t*c,unsigned cmd,int mode,void*send,unsigned ns,void*recv,unsigned nr,void*cb,void*arg){
 (void)c;(void)mode;(void)send;(void)ns;(void)nr;(void)cb;(void)arg;U(!pending);pending=cmd;reply=recv;calls++;return 0;}
static int SifCheckStatRpc(SifRpcClientData_t*c){unsigned i;(void)c;
 if(pending){reply->status=card;reply->result=fail_next?-2:0;
  if(pending==1&&!fail_next){unsigned k=reply->command;U(reply->mode==9);
   if(k>=17&&k<=24){mask&=~(1u<<(k-17));off_time[k-17]=now;}
   else {U(k>=1&&k<=8);U(now-off_time[k-1]>=robot.deadms);mask|=1u<<(k-1);}
   for(i=0;i<4;i++)U(((mask>>(2*i))&3)!=3||k>=17);
  }pending=0;fail_next=0;
 }return 0;
}
#include "../ee/robot_usb.h"
static void run(unsigned pad,unsigned connected,unsigned net,unsigned ms){unsigned end=now+ms;for(;now<end;now+=10)robot_usb_tick(pad,connected,net,now);}
static void run_program(RobotProgram*p,unsigned buttons,unsigned ms){unsigned end=now+ms;for(;now<end;now+=10){robot.program_mask=rp_tick(p,buttons,!robot.halt,robot.applied,robot_sync&&!robot_pending&&!robot_reset,now);robot_usb_tick(buttons,1,1,now);}}
int main(void){unsigned before;robot_usb_init(0);run(0,0,1,1000);U(binds>=2&&robot.ready&&!robot_reset&&!mask);
 U(robot_web(&robot,"forward?s=1",now)==0);run(0,0,1,300);U(mask==5);
 run(ROBOT_BACK,1,1,500);U(mask==10&&!robot.web);
 U(robot_web(&robot,"stop?s=2",now)==0);run(ROBOT_BACK,1,1,300);U(!mask);
 run(0,1,1,100);run(ROBOT_LEFT,1,1,500);U(mask==6);run(0,0,1,300);U(!mask);
 U(robot_web(&robot,"right?s=3",now)==0);run(0,0,1,300);U(mask==9);run(0,0,1,1300);U(!mask);
 before=calls;card=0;run(ROBOT_FORWARD,1,0,600);U(!robot.ready&&calls>before);
 card=2;run(0,1,1,600);U(robot.ready&&!mask);fail_next=1;run(ROBOT_FORWARD,1,1,400);run(0,0,1,500);U(!mask);
 {RobotProgram p;unsigned off;rp_default(&p,0,1);robot.program_mode=1;run_program(&p,0,300);run_program(&p,0x4000,500);U(mask==5);run_program(&p,0,300);U(!mask);
  memset(&p,0,sizeof(p));p.config.right=1;p.config.key[0].count=6;{uint8_t op[]={0,2,1,10,1,3};memcpy(p.config.key[0].op,op,6);}run_program(&p,0x4000,50);U(mask==1);run_program(&p,0,300);U(!mask);
  run_program(&p,0x4000,700);U(mask==2);off=off_time[0];U(now-off>=100);run_program(&p,0,300);U(!mask);}
 printf("ROBOT_USB_TEST PASS checks=%u actual-EE-scheduler RPC=mock startup-off-break-before-make-pad-web-disconnect-fault-custom-sequence-wait-release\n",checks);return 0;
}
