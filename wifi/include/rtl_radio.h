/* SPDX-License-Identifier: MIT */
#ifndef RTL_RADIO_H
#define RTL_RADIO_H
#include "rtl_bringup.h"
enum { RTL_RADIO_BAD_ID=-31,RTL_RADIO_BAD_MAC=-32,
       RTL_RADIO_BAD_POWER=-33,RTL_RADIO_TRUNCATED_ROM=-34 };
enum { RTL_RADIO_ROM=1, RTL_RADIO_MAC, RTL_RADIO_BB, RTL_RADIO_RF,
       RTL_RADIO_CALIBRATE, RTL_RADIO_CHANNEL, RTL_RADIO_READY };
typedef struct {
    uint32_t stage, transfers, last_address, channel;
    int32_t result;
    uint8_t mac[6], rom[128], pa_setting, board;
    uint32_t rf_channel[2];
} RtlRadioReport;
/* EFUSE requests are read triggers only; programming bit is always forbidden. */
int rtl_radio_write_allowed(unsigned address, unsigned length, const uint8_t *data);
int rtl_radio_init(RtlIo *, RtlDelayUs, void *, RtlRadioReport *);
int rtl_radio_channel(RtlIo *, RtlDelayUs, void *, RtlRadioReport *, unsigned channel);
/* NULL returns the receive filter to survey mode. */
int rtl_radio_bssid(RtlIo *,RtlDelayUs,void *,RtlRadioReport *,const uint8_t bssid[6]);
#endif
