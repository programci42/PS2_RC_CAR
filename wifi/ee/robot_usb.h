/* Existing role.elf HID protocol, independent asynchronous RPC client. */
#include "seda_relay_rpc.h"
#include "robot_control.h"
static SifRpcClientData_t robot_rpc __attribute__((aligned(64)));
static SedaRelayRpcPacket robot_packet __attribute__((aligned(64)));
extern unsigned char seda_relay_irx[];extern unsigned int size_seda_relay_irx;
static unsigned robot_bound,robot_pending,robot_index,robot_reset=1,robot_last_pad,robot_sync,robot_fault_reported,robot_module_error;
static volatile unsigned robot_recover_wanted;
static unsigned robot_startup_signal_sent;
static void robot_note_error(const char*s){if(robot.error_sink)robot.error_sink(robot.error_context,s);}
static uint32_t robot_poll_at,robot_deadline;
static void robot_usb_init(int load_saved){int result=0;robot_init(&robot);
    /* role.elf's saved group/mode/dead-time settings, read-only. */
    if(load_saved){char config[256]={0};int fd=open("mc0:/SYS-CONF/ROLECFG.TXT",O_RDONLY),n;
     if(fd>=0){n=read(fd,config,255);close(fd);if(n>0){const char*p;
       p=strstr(config,"leftgroup=");if(p)robot.left=atoi(p+10);
       p=strstr(config,"rightgroup=");if(p)robot.right=atoi(p+11);
       p=strstr(config,"deadms=");if(p)robot.deadms=atoi(p+7);
       p=strstr(config,"mode=");if(p)robot.mode=atoi(p+5);
     }}if(robot.left>3||robot.right>3||robot.left==robot.right){robot.left=0;robot.right=1;}
     if(robot.deadms>1000)robot.deadms=1000;
     if(robot.mode>=SEDA_RELAY_MODE_COUNT)robot.mode=9;}
    memset(&robot_rpc,0,sizeof(robot_rpc));
    if(SifExecModuleBuffer(seda_relay_irx,size_seda_relay_irx,0,0,&result)<0||result<0){robot_module_error=1;return;}
    if(SifBindRpc(&robot_rpc,SEDA_RELAY_RPC_ID,SIF_RPC_M_NOWAIT)>=0)robot_pending=3;
}
static void robot_rpc_start(unsigned command,uint32_t now){
    robot_sync=0;
    if(SifCallRpc(&robot_rpc,command,SIF_RPC_M_NOWAIT,&robot_packet,sizeof(robot_packet),&robot_packet,sizeof(robot_packet),0,0)<0){robot_note_error("Role USB RPC istegi baslatilamadi.");robot.ready=0;robot_reset=1;return;}
    robot_pending=command;robot_deadline=now+2000;
}
static void robot_usb_tick(unsigned pad,unsigned connected,unsigned online,uint32_t now){unsigned i;
    if(robot_last_pad&&!connected){robot.web=0;robot.halt=1;}robot_last_pad=connected;
    robot.maintenance=wifi_files_busy(&file_service);robot_tick(&robot,pad,connected,online,now);
    http.relay_ready=robot.ready&&!robot_reset;http.owner=robot.owner;http.mask=robot.applied;http.left=robot.left;http.right=robot.right;
    http.file_blocked=robot.applied||robot.desired||robot.web||robot.pad;
    if(robot_pending){
        if(SifCheckStatRpc(&robot_rpc)){if(robot_pending!=3&&(int32_t)(now-robot_deadline)>=0){robot.ready=0;robot.desired=0;if(!robot_fault_reported){robot_note_error("Role USB RPC zaman asimi.");robot_fault_reported=1;}}return;}robot_fault_reported=0;
        if(robot_pending==3){robot_bound=robot_rpc.server!=0;robot_pending=0;robot_poll_at=now+250;return;}
        if(robot_pending==SEDA_RELAY_RPC_SEND_COMMAND){
            if(robot_packet.result){robot_note_error("Relay command USB transfer failed.");robot.ready=0;robot_reset=1;robot_index=0;robot.applied=0;robot_sync=0;}
            else if(robot_packet.command>=17){robot.applied&=~(1u<<(robot_packet.command-17));robot.off_at=now;if(robot_reset&&++robot_index==8){robot_reset=0;robot_index=0;}}
            else robot.applied|=1u<<(robot_packet.command-1);
        }else {robot.ready=robot_packet.status==SEDA_RELAY_STATUS_READY;
            if(!robot.ready){robot_reset=1;robot_index=0;robot.applied=0;robot_sync=0;}}
        robot_pending=0;
    }
    if(!robot_bound){
        if((int32_t)(now-robot_poll_at)>=0){robot_poll_at=now+250;if(SifBindRpc(&robot_rpc,SEDA_RELAY_RPC_ID,SIF_RPC_M_NOWAIT)>=0)robot_pending=3;}
        return;
    }
    if(robot_recover_wanted&&!robot.ready&&!robot.applied&&!robot.desired){robot_recover_wanted=0;memset(&robot_packet,0,sizeof(robot_packet));robot_reset=1;robot_index=0;robot_rpc_start(SEDA_RELAY_RPC_RECOVER,now);return;}
    if((int32_t)(now-robot_poll_at)>=0){memset(&robot_packet,0,sizeof(robot_packet));robot_poll_at=now+250;robot_rpc_start(SEDA_RELAY_RPC_GET_STATUS,now);return;}
    if(!robot.ready)return;
    if(!robot_startup_signal_sent&&!robot_reset&&!robot.applied&&!robot.desired){robot_startup_signal_sent=1;if(robot.left!=0&&robot.right!=0){memset(&robot_packet,0,sizeof(robot_packet));robot_packet.mode=robot.mode;robot_rpc_start(SEDA_RELAY_RPC_READY_SIGNAL,now);return;}}
    memset(&robot_packet,0,sizeof(robot_packet));robot_packet.mode=robot.mode;
    /* Reset/off transitions always finish before any new relay is enabled. */
    if(robot_reset){robot_packet.command=17+robot_index;robot_rpc_start(1,now);return;}
    for(i=0;i<8;i++)if((robot.applied&~robot.desired)&(1u<<i)){robot_packet.command=17+i;robot_rpc_start(1,now);return;}
    if((uint32_t)(now-robot.off_at)<(robot.deadms<100?100:robot.deadms))return;
    for(i=0;i<8;i++)if((robot.desired&~robot.applied)&(1u<<i)){robot_packet.command=1+i;robot_rpc_start(1,now);return;}
    robot_sync=1;
}
