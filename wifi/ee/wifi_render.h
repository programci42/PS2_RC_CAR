/* SPDX-License-Identifier: MIT */
#ifndef WIFI_RENDER_H
#define WIFI_RENDER_H
#include "wifi_app.h"
void wifi_render_reset(void);
void wifi_draw(const WifiApp *app,int preview,int pad_connected);
void wifi_draw_drive(const WifiApp *,uint32_t buttons,int connected,unsigned ready,unsigned left,unsigned right,unsigned applied,unsigned owner,unsigned desired);
#endif
