/* SPDX-License-Identifier: MIT */
#define NEWLIB_PORT_AWARE
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#define main scan_selftest
#include "../tests/scan.c"
#undef main
int main(void){SifInitRpc(0);return scan_selftest();}
