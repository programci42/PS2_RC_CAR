#ifndef WIFI_SEED_H
#define WIFI_SEED_H
#include <stdint.h>
void wifi_seed_init(int mc_ready,int argc,char **argv);
int wifi_seed_nonce(const char *unused,uint8_t nonce[32]);
int wifi_seed_read_known(uint8_t *out,unsigned capacity);
int wifi_seed_known_path(char*out,unsigned capacity);
int wifi_storage_usb_init(void);
#endif
