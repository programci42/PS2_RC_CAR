/* SPDX-License-Identifier: MIT */
#ifndef SELFTEST_H
#define SELFTEST_H
typedef struct { int checks,failures,first_failure; } RtlTestReport;
RtlTestReport rtl_selftest(void);
#endif
