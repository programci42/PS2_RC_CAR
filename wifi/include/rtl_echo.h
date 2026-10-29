/* SPDX-License-Identifier: MIT */
#ifndef RTL_ECHO_H
#define RTL_ECHO_H
#include <stddef.h>
#include <stdint.h>
enum {RTL_ECHO_ARP=1,RTL_ECHO_ICMP=2};
typedef struct {unsigned int kind;uint8_t remote[4];uint16_t identifier,sequence;unsigned int payload_bytes;} RtlEchoEvent;
/* Ethernet without FCS, after authenticated WiFi decapsulation. IPv4 echo
 * without IP options or fragmentation. Does not implement DHCP/TCP/WPA2.
 * Returns reply length, or zero for ignored/malformed input. A constructed
 * reply is NOT a successful USB transmission; count TX only on completion. */
int rtl_echo_reply(const uint8_t own_mac[6],const uint8_t own_ip[4],
    const uint8_t *frame,size_t size,uint8_t *reply,size_t capacity,RtlEchoEvent *event);
#endif
