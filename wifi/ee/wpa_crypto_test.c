/* SPDX-License-Identifier: MIT */
#define NEWLIB_PORT_AWARE
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <debug.h>
#define main crypto_selftest
#include "../tests/wpa_crypto.c"
#undef main
int main(void){
    int result;
    SifInitRpc(0);init_scr();scr_setCursor(0);
    scr_printf("WPA2 CRYPTO PRIMITIVE TEST\nPublic vectors only. No WiFi connection.\n");
    result=crypto_selftest();scr_printf("\n%s\n",result?"FAILED":"PASSED");return result;
}
