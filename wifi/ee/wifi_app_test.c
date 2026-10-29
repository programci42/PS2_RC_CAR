/* SPDX-License-Identifier: MIT */
#define NEWLIB_PORT_AWARE
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <stdio.h>
extern int wifi_app_selftest(void),echo_packet_selftest(void);
int main(void){SifInitRpc(0);if(wifi_app_selftest()||echo_packet_selftest())return 1;printf("WIFI_FLOW_TEST PASS synthetic-state-and-packets physical-network=NO\n");return 0;}
