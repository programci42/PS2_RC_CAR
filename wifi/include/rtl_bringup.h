/* SPDX-License-Identifier: MIT */
#ifndef RTL_BRINGUP_H
#define RTL_BRINGUP_H
#include "rtl_core.h"
#define RTL_BOOT_MAGIC 0x424f4f54u
#define RTL_BOOT_UNSUPPORTED (-20)
#define RTL_BOOT_BAD_FIRMWARE (-21)
enum { RTL_BOOT_VALIDATE=0, RTL_BOOT_IDENTIFY, RTL_BOOT_AUTOLOAD,
       RTL_BOOT_POWER, RTL_BOOT_MAC, RTL_BOOT_RESET, RTL_BOOT_DOWNLOAD,
       RTL_BOOT_CHECKSUM, RTL_BOOT_FIRMWARE_READY, RTL_BOOT_COMPLETE,
       RTL_BOOT_DMA };
/* Fixed 32-bit fields: also used as EE/IOP RPC response. Complete means only
 * firmware readiness; calibration, scan, association and IP are separate. */
typedef struct {
    uint32_t magic,stage; int32_t result;
    uint32_t sys_cfg,firmware_version,firmware_bytes,uploaded_bytes;
    uint32_t last_address,last_value,transfers,polls;
} RtlBootReport;
typedef void (*RtlDelayUs)(void *context,unsigned int microseconds);
int rtl_boot_write_allowed(unsigned int address,unsigned int length);
int rtl_boot_firmware_valid(const uint8_t *firmware,size_t length);
int rtl_boot_run(RtlIo *io,RtlDelayUs delay,void *delay_context,
                 const uint8_t *firmware,size_t length,RtlBootReport *report);
#endif
