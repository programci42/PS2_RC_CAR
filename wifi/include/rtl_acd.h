#ifndef RTL_ACD_H
#define RTL_ACD_H
#include <stdint.h>
typedef struct {uint8_t mac[6],ip[4];unsigned probes,announcements,conflict,ready;uint32_t deadline,random;} RtlAcd;
void rtl_acd_start(RtlAcd*,const uint8_t*,const uint8_t*,uint32_t,uint32_t);
int rtl_acd_rx(RtlAcd*,const uint8_t*,unsigned);
int rtl_acd_tx(RtlAcd*,uint32_t,uint8_t*,unsigned);
#endif
