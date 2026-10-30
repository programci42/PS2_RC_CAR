#include "robot_control.h"
#include <string.h>
#include <stdlib.h>
void robot_init(RobotControl*r){memset(r,0,sizeof(*r));r->right=1;r->deadms=100;r->mode=9;}
unsigned robot_mask(const RobotControl*r,unsigned d){unsigned l=0,q=0;if(r->left>3||r->right>3||r->left==r->right)return 0;
    if(d==ROBOT_FORWARD)l=q=1;
    if(d==ROBOT_BACK)l=q=2;
    if(d==ROBOT_LEFT){l=2;q=1;}if(d==ROBOT_RIGHT){l=1;q=2;}
    return (l<<(r->left*2))|(q<<(r->right*2));
}
int robot_web(void*ctx,const char*input,uint32_t now){RobotControl*r=ctx;unsigned d;char cmd[16];const char*q=strstr(input,"?s=");uint32_t seq;
    if(!q||q-input<1||q-input>15)return -3;
    memcpy(cmd,input,q-input);cmd[q-input]=0;seq=(uint32_t)strtoul(q+3,0,10);
    if(r->seq_seen&&(int32_t)(seq-r->web_seq)<=0&&strcmp(cmd,"stop"))return -2;
    if(!r->seq_seen||(int32_t)(seq-r->web_seq)>0){r->web_seq=seq;r->seq_seen=1;}
    if(!strcmp(cmd,"stop")){r->test_active=r->test_mask=0;r->web=0;r->timed=0;r->halt=1;r->desired=0;return 0;}
    if(!strncmp(cmd,"test",4)){char*end;unsigned long mask;
        if(cmd[4]<'0'||cmd[4]>'9')return -3;
        mask=strtoul(cmd+4,&end,10);if(*end||mask>255||(mask&(mask>>1)&0x55))return -3;
        if(!mask){r->test_active=r->test_mask=0;r->web=r->timed=0;r->halt=1;r->desired=0;return 0;}
        if(!r->ready||r->usb_reset||r->output_uncertain)return -1;
        if(r->pad||r->halt||r->maintenance||r->system_locked||r->web)return -2;
        r->test_mask=(unsigned)mask;r->test_at=now;r->test_active=1;return 0;}
    /* Stop is a sequence barrier, including delayed in-flight heartbeats. */
    if(!strcmp(cmd,"forward"))d=1;else if(!strcmp(cmd,"back"))d=2;else if(!strcmp(cmd,"left"))d=3;else if(!strcmp(cmd,"right"))d=4;else return -3;
    if(!r->ready)return -1;
    if(r->maintenance||r->system_locked)return -2;
    if(r->pad||r->halt)return -2;
    r->test_active=r->test_mask=0;r->timed=0;r->web=d;r->web_at=now;return 0;
}
int robot_timed(RobotControl*r,unsigned direction,unsigned seconds,uint32_t now){if(direction<1||direction>4||seconds<1||seconds>3600)return -3;if(!r->ready)return -1;if(r->pad||r->halt||r->maintenance||r->system_locked)return -2;r->test_active=r->test_mask=0;r->web=direction;r->web_at=now;r->timed=1;r->timed_until=now+seconds*1000u;return 0;}
void robot_tick(RobotControl*r,unsigned pad,unsigned connected,unsigned network,uint32_t now){
    if(r->halt||!r->ready||!network||(connected&&pad)||r->maintenance||r->system_locked||(uint32_t)(now-r->test_at)>1000)r->test_active=r->test_mask=0;
    if(r->maintenance||r->system_locked){r->web=r->timed=0;r->pad=connected?pad:0;r->halt=1;r->desired=0;r->owner=0;return;}
    r->pad=connected?pad:0;if(!network||(r->timed?(int32_t)(now-r->timed_until)>=0:(uint32_t)(now-r->web_at)>1000))r->web=r->timed=0;
    if(!r->pad)r->halt=0;
    if(r->pad){r->web=r->timed=0;r->owner=1;}else r->owner=r->web||r->test_active?2:0;
    r->desired=r->ready&&!r->halt?(r->test_active?r->test_mask:r->pad&&r->program_mode?r->program_mask:robot_mask(r,r->pad?r->pad:r->web)):0;
}
