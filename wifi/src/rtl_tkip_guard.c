#include "rtl_tkip_guard.h"
int rtl_tkip_guard_blocked(RtlTkipGuard*g,uint32_t now){
    if(!g)return 0;
    if(g->blocked&&(uint32_t)(now-g->blocked_at)>=60000){g->blocked=0;g->seen=0;}
    return !!g->blocked;
}
int rtl_tkip_guard_failure(RtlTkipGuard*g,uint32_t now){
    if(!g)return 0;
    if(rtl_tkip_guard_blocked(g,now))return 2;
    if(g->seen&&(uint32_t)(now-g->first_at)<60000){g->blocked=1;g->blocked_at=now;return 2;}
    g->seen=1;g->first_at=now;return 1;
}
