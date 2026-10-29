/* SPDX-License-Identifier: MIT */
#include <kernel.h>
#include <debug.h>
#include "selftest.h"
int main(void) {
    RtlTestReport report;
    init_scr();scr_setCursor(0);
    scr_printf("RTL8192CU PS2 CORE SELF-TEST v0.1\n");
    scr_printf("SIMULATED USB TRANSPORT\n");
    scr_printf("NO PHYSICAL WIFI ADAPTER ATTACHED\n\n");
    report=rtl_selftest();
    scr_printf("Checks : %d\nFailures: %d\nFirst failure line: %d\n\n",report.checks,report.failures,report.first_failure);
    scr_printf("%s\n\n",report.failures?"SELF-TEST FAILED":"ALL CORE CHECKS PASSED");
    scr_printf("Included in this executable:\n");
    scr_printf("- Actual IOP driver's portable core\n");
    scr_printf("- VID/PID and bulk endpoint parser\n");
    scr_printf("- Realtek vendor READ request encoding\n");
    scr_printf("- Little-endian register decoding\n");
    scr_printf("- Short/error/timeout/disconnect responses\n");
    scr_printf("- Malformed descriptor stress cases\n\n");
    scr_printf("NOT VERIFIED HERE:\n");
    scr_printf("Physical PS2 USB power and enumeration\n");
    scr_printf("IOP USB callbacks and actual register data\n");
    scr_printf("Firmware, RF, WPA2, DHCP, Internet\n\n");
    scr_printf("These results do NOT prove working WiFi.\n");
    SleepThread();return report.failures?1:0;
}
