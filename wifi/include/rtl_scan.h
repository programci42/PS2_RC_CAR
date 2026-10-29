/* SPDX-License-Identifier: MIT */
#ifndef RTL_SCAN_H
#define RTL_SCAN_H
#include <stddef.h>
#include <stdint.h>
#define RTL_SCAN_CAPACITY 16
enum {RTL_SEC_OPEN=0,RTL_SEC_LEGACY=1,RTL_SEC_WPA2=2,RTL_SEC_OTHER_RSN=3};
typedef struct {
    uint8_t bssid[6],ssid[32],ssid_length,channel,security,wpa2_ccmp_psk;
    uint16_t capabilities,rsn_length;
    uint8_t rsn[257]; /* complete RSN IE, including ID and length */
    int32_t signal;
    uint8_t ofdm6; /* AP advertised 6 Mbps in supported/extended rates. */
} RtlAccessPoint;
typedef struct {RtlAccessPoint ap[RTL_SCAN_CAPACITY];unsigned int count;} RtlScanList;
/* Input is a complete beacon/probe response WITHOUT FCS. No RF scan is
 * performed here. Signal is caller metadata, not a calculated power value. */
int rtl_scan_parse(const uint8_t *frame,size_t size,unsigned int rx_channel,int signal,RtlAccessPoint *out);
int rtl_scan_add(RtlScanList *list,const RtlAccessPoint *ap);
/* UI-safe byte escaping; SSIDs are binary, not NUL-terminated strings. */
int rtl_scan_label(const RtlAccessPoint *ap,char *out,size_t capacity);
/* Explain why a visible AP cannot enter the WPA2/CCMP connection flow. */
const char *rtl_scan_join_message(const RtlAccessPoint *ap);
const char *rtl_scan_security_label(const RtlAccessPoint *ap);
#endif
