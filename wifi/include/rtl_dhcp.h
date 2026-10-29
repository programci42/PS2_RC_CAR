/* SPDX-License-Identifier: MIT */
#ifndef RTL_DHCP_H
#define RTL_DHCP_H
#include <stdint.h>
#include <stddef.h>
enum { RTL_DHCP_DISCOVER=1, RTL_DHCP_REQUEST, RTL_DHCP_BOUND, RTL_DHCP_FAILED, RTL_DHCP_RENEW, RTL_DHCP_REBIND };
typedef struct {
    uint8_t mac[6],ip[4],mask[4],gateway[4],server[4],dns[4],preferred[4];
    uint8_t server_mac[6];
    uint32_t xid,deadline,lease_started,lease_seconds,t1,t2;unsigned state,attempts;
} RtlDhcp;
void rtl_dhcp_start(RtlDhcp *,const uint8_t mac[6],uint32_t random_xid,uint32_t now_ms);
/* Complete Ethernet frames, no FCS. tx returns 0 until retry is due. */
int rtl_dhcp_tx(RtlDhcp *,uint32_t now_ms,uint8_t *,size_t);
/* 1=valid offer/request pending, 2=bound, -1=malformed/unrelated, -2=NAK. */
int rtl_dhcp_rx(RtlDhcp *,uint32_t now_ms,const uint8_t *,size_t);
int rtl_dhcp_decline(const RtlDhcp*,uint32_t,uint8_t*,size_t);
int rtl_dhcp_has_lease(const RtlDhcp*);
#endif
