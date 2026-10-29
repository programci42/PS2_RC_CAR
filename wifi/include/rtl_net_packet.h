/* SPDX-License-Identifier: MIT */
#ifndef RTL_NET_PACKET_H
#define RTL_NET_PACKET_H
#include <stdint.h>
int rtl_net_query(uint8_t *out,unsigned int capacity,unsigned int id,unsigned int port);
int rtl_net_response(const uint8_t *data,unsigned int length,unsigned int id,unsigned int port,unsigned int *answers);
#endif
