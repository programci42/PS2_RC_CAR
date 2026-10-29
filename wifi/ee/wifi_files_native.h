#ifndef WIFI_FILES_NATIVE_H
#define WIFI_FILES_NATIVE_H
#include "wifi_files.h"
void wifi_files_native_init(WifiFiles*,int mc_ready,const char *running);
void wifi_files_native_devices(char*,unsigned);
int wifi_files_native_flush(void);
#endif
