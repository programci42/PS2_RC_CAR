/* SPDX-License-Identifier: MIT */
#ifndef WIFI_STORE_H
#define WIFI_STORE_H
#include "wifi_app.h"
int wifi_store_load(const char *directory,WifiProfile *profile);
/* Writes the inactive journal slot, then verifies it by reading it back.
 * Existing good slot is preserved if writing/verification fails. */
int wifi_store_save(const char *directory,WifiProfile *profile);
#endif
