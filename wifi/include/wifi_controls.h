#ifndef WIFI_CONTROLS_H
#define WIFI_CONTROLS_H
#include "wifi_app.h"
typedef struct {unsigned drive,restart,held,display,display_changed,display_latched,reconnect,reconnect_latched;uint32_t chord_at;} WifiControls;
void wifi_controls_init(WifiControls*);
/* Returns menu input with transition buttons consumed. Network state is untouched. */
uint32_t wifi_controls_input(WifiControls*,WifiApp*,uint32_t,int,uint32_t);
#endif
