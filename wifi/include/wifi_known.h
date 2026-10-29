#ifndef WIFI_KNOWN_H
#define WIFI_KNOWN_H
#include "wifi_app.h"
#include "wifi_files.h"
/* First matching, supported AP in CSV row order; exact case-sensitive SSID.
 * Returns 1 with an already-derived PMK, 0 if none match. Never logs secrets. */
int wifi_known_select(const uint8_t *,size_t,const RtlScanList *,unsigned *,uint8_t pmk[32]);
int wifi_known_mark(const uint8_t *,size_t,const RtlScanList *,uint8_t flags[RTL_SCAN_CAPACITY]);
int wifi_known_config(const uint8_t*,size_t,const uint8_t*,unsigned,uint8_t last[4],uint8_t manual[4]);
int wifi_known_update(const uint8_t*,size_t,const uint8_t*,unsigned,const uint8_t last[4],const uint8_t manual[4],uint8_t*,unsigned);
int wifi_known_save(WifiFiles*,const char*,const uint8_t*,unsigned,const uint8_t last[4],const uint8_t manual[4]);
#endif
