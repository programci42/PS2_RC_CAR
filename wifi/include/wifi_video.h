/* PS2SDK debug console SetVideoMode writes 0xff62. Keep a software value;
   never reconstruct the output configuration from a GS register readback. */
#ifndef WIFI_VIDEO_H
#define WIFI_VIDEO_H
#include <stdint.h>
static inline uint64_t wifi_video_pmode(unsigned enabled){return enabled?UINT64_C(0xff62):UINT64_C(0xff60);}
#endif
