#ifndef ROBOT_PROGRAM_H
#define ROBOT_PROGRAM_H
#include <stdint.h>
#define RP_KEYS 14
#define RP_STEPS 16
#define RP_JSON_MAX 3500
typedef struct {uint8_t count,op[RP_STEPS];} RpBinding;
typedef struct {unsigned left,right;RpBinding key[RP_KEYS];} RpConfig;
typedef struct {RpConfig config;unsigned held,index,pc,target,barrier,waiting,fault;uint32_t until;} RobotProgram;
extern const char *const rp_names[RP_KEYS];
extern const uint16_t rp_buttons[RP_KEYS];
const char *rp_action(unsigned);
void rp_default(RobotProgram*,unsigned,unsigned);
int rp_parse(const char*,unsigned,RpConfig*);
int rp_json(const RpConfig*,char*,unsigned);
unsigned rp_tick(RobotProgram*,uint32_t,int,unsigned,int,uint32_t);
int rp_mask_safe(unsigned);
#endif
