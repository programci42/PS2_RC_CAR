#ifndef WIFI_RUNTIME_H
#define WIFI_RUNTIME_H
#include "wifi_files.h"
#include "robot_control.h"
#include "robot_program.h"
#define WIFI_LOG_LIMIT (555u*1024u)
typedef struct {WifiFiles*files;RobotControl*robot;RobotProgram program;char settings[WF_PATH],log[WF_PATH];
 uint32_t log_bytes,checked_at,flush_at;unsigned log_warning,log_failed,config_error,queued,dropped,heap_bytes,ram_bytes;
 char queue[16][224];int64_t epoch;uint32_t epoch_at;unsigned clock_valid;
 unsigned battery_set,battery_capacity,battery_motors,battery_base_mw,battery_motor_ma,battery_eff;
 uint32_t battery_uwh,battery_at,battery_saved_at,battery_power_mw,battery_motor_100ms_uwh;
 uint64_t battery_fraction;unsigned battery_dirty,reload_pending;uint32_t reload_at;
 unsigned rx_kbps,tx_kbps,signal_quality,signal_valid;char last_request[72];
 unsigned system_action,last_channel;uint32_t system_due,system_requested;
 unsigned display_enabled,display_requested,display_toggles,pad_connected,pad_buttons;
 unsigned pad_thread,pad_ticks,pad_max_gap_ms;
 unsigned pad_port_open,pad_reopens,pad_init_stage,pad_bytes,pad_raw,pad_dma;int pad_state;
} WifiRuntime;
void wifi_runtime_init(WifiRuntime*,WifiFiles*,RobotControl*,uint32_t);
void wifi_runtime_clock(WifiRuntime*,int64_t,uint32_t);
void wifi_runtime_error(WifiRuntime*,uint32_t,const char*);
void wifi_runtime_tick(WifiRuntime*,uint32_t);
int wifi_runtime_api(void*,const char*,const char*,const char*,const uint8_t*,unsigned,uint32_t,char*,unsigned);
int wifi_runtime_replace(WifiFiles*,const char*,const void*,unsigned);
void wifi_runtime_energy(WifiRuntime*,uint32_t);
int wifi_runtime_channel(WifiRuntime*,unsigned);
#endif
