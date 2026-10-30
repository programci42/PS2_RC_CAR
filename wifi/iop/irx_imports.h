#include <irx.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>
#include <intrman.h>
#include <sifcmd.h>
#include <sifrpc.h>
#include <usbd.h>
int sceUsbdDiagnosticSnapshot(unsigned *,unsigned);
#define I_sceUsbdDiagnosticSnapshot DECLARE_IMPORT(19, sceUsbdDiagnosticSnapshot)
