/* SPDX-License-Identifier: MIT */
#ifndef RTL_CORE_H
#define RTL_CORE_H
#include <stddef.h>
#include <stdint.h>
#define RTL_VID 0x0bda
#define RTL_PID 0x8178
enum { RTL_OK=0, RTL_E_ARGUMENT=-1, RTL_E_DESCRIPTOR=-2, RTL_E_INTERFACE=-3,
       RTL_E_IO=-4, RTL_E_SHORT=-5, RTL_E_TIMEOUT=-6, RTL_E_DISCONNECTED=-7, RTL_E_BUSY=-8 };
typedef struct {
    unsigned int configuration, interface_no, power_ma;
    unsigned int bulk_in, bulk_in_packet, out_count;
    unsigned int bulk_out[3], bulk_out_packet[3];
} RtlEndpoints;
typedef struct { uint8_t type, request; uint16_t value, request_index, length; } RtlRequest;
/* Backend returns actual byte count, or a negative RTL_E_* value. */
typedef int (*RtlControl)(void *context, const RtlRequest *request, uint8_t *data);
typedef struct { RtlControl control; void *context; } RtlIo;
int rtl_matches(unsigned int vid, unsigned int pid);
int rtl_parse_config(const uint8_t *bytes, size_t length, RtlEndpoints *out);
int rtl_read_register(RtlIo *io, unsigned int address, unsigned int width, uint32_t *value);
const char *rtl_error(int error);
#endif
