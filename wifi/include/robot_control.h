#ifndef ROBOT_CONTROL_H
#define ROBOT_CONTROL_H
#include <stdint.h>
enum {ROBOT_STOP,ROBOT_FORWARD,ROBOT_BACK,ROBOT_LEFT,ROBOT_RIGHT};
typedef struct {unsigned left,right,deadms,mode;unsigned ready,pad,web,halt,desired,applied,owner;uint32_t web_at,off_at,web_seq;unsigned seq_seen,maintenance,program_mode,program_mask,timed,system_locked;uint32_t timed_until;void(*error_sink)(void*,const char*);void*error_context;} RobotControl;
int robot_timed(RobotControl*,unsigned,unsigned,uint32_t);
void robot_init(RobotControl *);
int robot_web(void *,const char *,uint32_t);
void robot_tick(RobotControl *,unsigned pad,unsigned connected,unsigned network,uint32_t);
unsigned robot_mask(const RobotControl *,unsigned direction);
#endif
