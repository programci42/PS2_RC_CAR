/* SPDX-License-Identifier: MIT */
#ifndef RTL_TKIP_GUARD_H
#define RTL_TKIP_GUARD_H
#include <stdint.h>
typedef struct {uint32_t first_at,blocked_at;unsigned seen,blocked;} RtlTkipGuard;
int rtl_tkip_guard_blocked(RtlTkipGuard *,uint32_t now);
/* Two verified-ICV/Michael failures inside 60 seconds block for 60 seconds. */
int rtl_tkip_guard_failure(RtlTkipGuard *,uint32_t now);
#endif
