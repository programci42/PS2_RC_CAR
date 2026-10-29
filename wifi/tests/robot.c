#include "robot_control.h"
#include <stdio.h>
#include <stdlib.h>
#define R(x) do{count++;if(!(x)){printf("ROBOT_TEST FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
int robot_test(void){RobotControl r;unsigned l,q,d,count=0;robot_init(&r);R(robot_web(&r,"forward?s=1",0)==-1);r.ready=1;
    R(robot_web(&r,"forward?s=2",0)==0);robot_tick(&r,0,0,1,1);R(r.desired==5&&r.owner==2);
    robot_tick(&r,ROBOT_BACK,1,1,2);R(r.desired==10&&r.owner==1&&!r.web);
    R(robot_web(&r,"left?s=3",3)==-2);R(robot_web(&r,"stop?s=4",4)==0);robot_tick(&r,ROBOT_BACK,1,1,5);R(!r.desired);
    robot_tick(&r,0,1,1,6);R(!r.desired);robot_tick(&r,ROBOT_LEFT,1,1,7);R(r.desired==6);
    robot_tick(&r,0,1,1,8);R(!r.desired);R(robot_web(&r,"right?s=5",9)==0);robot_tick(&r,0,0,1,10);R(r.desired==9);
    R(robot_web(&r,"forward?s=4",10)==-2);robot_tick(&r,0,0,1,1010);R(!r.desired);
    R(robot_web(&r,"forward?s=6",1011)==0);robot_tick(&r,0,0,0,1012);R(!r.desired&&!r.web);
    for(l=0;l<4;l++)for(q=0;q<4;q++)for(d=0;d<=4;d++){unsigned mask,i;r.left=l;r.right=q;mask=robot_mask(&r,d);
        if(l==q)R(!mask);else for(i=0;i<4;i++)R(((mask>>(i*2))&3)!=3);}
    robot_init(&r);r.ready=1;R(robot_web(&r,"forward?s=1",0)==0);robot_tick(&r,0,0,1,1);R(r.desired==5);
    r.maintenance=1;robot_tick(&r,ROBOT_FORWARD,1,1,2);R(!r.desired&&!r.web&&r.halt);R(robot_web(&r,"forward?s=2",3)==-2);R(!robot_web(&r,"stop?s=3",4));
    r.maintenance=0;robot_tick(&r,ROBOT_FORWARD,1,1,5);R(!r.desired);robot_tick(&r,0,1,1,6);robot_tick(&r,ROBOT_FORWARD,1,1,7);R(r.desired==5);
    robot_init(&r);r.ready=1;R(!robot_timed(&r,ROBOT_FORWARD,10,100));robot_tick(&r,0,1,1,9000);R(r.desired==5&&r.timed);robot_tick(&r,0,1,1,10100);R(!r.desired&&!r.timed);
    R(!robot_timed(&r,ROBOT_BACK,3,20000));robot_tick(&r,ROBOT_FORWARD,1,1,20001);R(!r.timed&&r.owner==1);robot_tick(&r,0,1,1,20002);
    R(!robot_timed(&r,ROBOT_LEFT,2,0xfffffff0u));robot_tick(&r,0,1,1,0x100);R(r.desired==6);robot_tick(&r,0,1,0,0x101);R(!r.desired&&!r.timed);
    R(robot_timed(&r,1,0,0)==-3&&robot_timed(&r,1,3601,0)==-3);r.system_locked=1;R(robot_timed(&r,1,1,0)==-2);robot_tick(&r,ROBOT_FORWARD,1,1,0);R(!r.desired);
    printf("ROBOT_TEST PASS checks=%u groups-pad-priority-web-stop-timeout-network-loss-stale-command-maintenance-timed\n",count);return 0;
}
