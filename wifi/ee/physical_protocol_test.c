#define NEWLIB_PORT_AWARE
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#define RTL_PROTOCOL_EE
#define main protocol_test
#include "../tests/physical_protocol.c"
#undef main
int main(void){SifInitRpc(0);return protocol_test();}
