/* SPDX-License-Identifier: MIT */
#ifndef RTL_SUPPLICANT_H
#define RTL_SUPPLICANT_H
#include <stdint.h>
#include <stddef.h>
enum { RTL_WPA_WAIT_M1=1, RTL_WPA_WAIT_M3, RTL_WPA_WAIT_M4_TX, RTL_WPA_ONLINE };
typedef struct {
    unsigned state, rsn_length, last_m3_length, gtk_index, installs,group_cipher;
    uint8_t pmk[32], ap[6], sta[6], snonce[32], anonce[32], ptk[64];
    uint8_t replay[8], rsn[257], gtk[32], last_m3[512];
    uint8_t group_replay[8], last_group[512]; unsigned last_group_length;
    uint64_t tx_pn, rx_pn[17], group_pn[17],request_sequence;
} RtlSupplicant;
/* Caller obtains a fresh unpredictable SNonce for EVERY new association.
 * No default RNG or deterministic fallback exists in this layer. */
int rtl_supplicant_start(RtlSupplicant *,const uint8_t pmk[32],const uint8_t ap[6],
    const uint8_t sta[6],const uint8_t fresh_nonce[32],const uint8_t *ap_rsn,size_t rsn_len);
/* Complete EAPOL frame in/out, not Ethernet. Positive result is reply length;
 * zero means ignored packet; negative means invalid/authentication failure.
 * M4 USB completion must be reported before the controlled port opens. */
int rtl_supplicant_rx(RtlSupplicant *,const uint8_t *,size_t,uint8_t *,size_t);
void rtl_supplicant_tx_done(RtlSupplicant *,int success);
void rtl_supplicant_clear(RtlSupplicant *);
int rtl_supplicant_station_rsn(const uint8_t *ap_rsn,size_t length,uint8_t out[22]);
int rtl_wifi_unprotect(RtlSupplicant *,const uint8_t *,size_t,uint8_t *,size_t);
int rtl_supplicant_mic_report(RtlSupplicant *,uint8_t *,size_t);
/* 24-byte non-QoS ToDS/FromDS header only. CCMP packet includes 8-byte header
 * and 8-byte MIC. Receive accepts QoS (26 bytes), tracks per-TID replay. */
int rtl_ccmp_protect(RtlSupplicant *,const uint8_t header[24],const uint8_t *,size_t,uint8_t *,size_t);
int rtl_ccmp_unprotect(RtlSupplicant *,const uint8_t *,size_t,uint8_t *,size_t);
#endif
