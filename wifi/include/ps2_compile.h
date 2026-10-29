#ifndef PS2_COMPILE_H
#define PS2_COMPILE_H
#include <stdint.h>
/* Device-local, bounded C++ command subset and MIPS assembler; no host service. */
int ps2_compile(const char*,int assembly,uint8_t*,unsigned,uint32_t base,char*,unsigned);
#endif
