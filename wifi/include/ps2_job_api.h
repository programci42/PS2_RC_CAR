#ifndef PS2_JOB_API_H
#define PS2_JOB_API_H
/* Freestanding EE o32 ABI. No libc, exceptions, global constructors or SDK calls.
   C++: extern "C" int run(const Ps2JobApi *api). Assembly: exported run. */
typedef struct {unsigned version;void(*print)(const char*);void(*number)(int);unsigned(*millis)(void);void(*yield_ms)(unsigned);} Ps2JobApi;
#endif
