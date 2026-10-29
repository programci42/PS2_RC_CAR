/* SPDX-License-Identifier: MIT */
#ifndef RTL_WIRE_H
#define RTL_WIRE_H
#include "rtl_radio.h"
#define RTL_RPC_RADIO 5
#define RTL_RPC_CHANNEL 6
#define RTL_RPC_RX 7
#define RTL_RPC_TX 8
#define RTL_WIRE_MAX 2400
typedef struct { int32_t result;uint32_t generation,length,channel;uint8_t bytes[RTL_WIRE_MAX];uint32_t quality;uint32_t dma_padding[3]; } RtlWire;
typedef char RtlWire_dcache_line_size[(sizeof(RtlWire)%64)==0?1:-1];
unsigned rtl_wire_quality(const uint8_t *,unsigned);
int rtl_wire_tx(const uint8_t *,unsigned,uint8_t *,unsigned,unsigned packet_size);
int rtl_wire_tx_rate(const uint8_t *,unsigned,uint8_t *,unsigned,unsigned packet_size,unsigned rate);
int rtl_wire_rx(const uint8_t *,unsigned,uint8_t *,unsigned);
/* Consumed bytes include the 128-byte aggregate alignment. A rejected CRC
 * frame still advances; malformed/truncated descriptors consume zero. */
int rtl_wire_rx_next(const uint8_t *,unsigned,uint8_t *,unsigned,unsigned *consumed);
#endif
