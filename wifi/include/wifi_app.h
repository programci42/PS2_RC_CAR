/* SPDX-License-Identifier: MIT */
#ifndef WIFI_APP_H
#define WIFI_APP_H
#include "rtl_scan.h"
#define WIFI_UP 0x0010u
#define WIFI_DOWN 0x0040u
#define WIFI_LEFT 0x0080u
#define WIFI_RIGHT 0x0020u
#define WIFI_X 0x4000u
#define WIFI_O 0x2000u
#define WIFI_TRIANGLE 0x1000u
#define WIFI_SQUARE 0x8000u
#define WIFI_L2 0x0100u
#define WIFI_R2 0x0200u
#define WIFI_SELECT 0x0001u
#define WIFI_START 0x0008u
#define WIFI_KNOWN_CSV 1u
#define WIFI_KNOWN_PROFILE 2u
enum {WIFI_BOOT,WIFI_SCANNING,WIFI_LIST,WIFI_PASSWORD,WIFI_DERIVING,WIFI_JOINING,WIFI_DHCP,WIFI_ONLINE,WIFI_ERROR,WIFI_IPEDIT};
enum {WIFI_DO_SCAN=1,WIFI_DO_DERIVE=2,WIFI_DO_JOIN=4,WIFI_DO_LEAVE=8,WIFI_DO_SAVE=16,WIFI_DO_KNOWN=32,WIFI_DO_IPSAVE=64};
typedef struct {uint8_t ssid[32],ssid_length,pmk[32];uint32_t generation;} WifiProfile;
typedef struct {
    unsigned int state,actions,selected,row,column,shift,reveal,use_saved,authenticated,dirty;
    unsigned int boot_flags; /* driver, USB, firmware, radio */
    uint32_t ticket,held_buttons,repeat_at,repeat_dir;
    RtlScanList scan;
    uint8_t known[RTL_SCAN_CAPACITY];
    unsigned linked,active_ap,ip_return;
    uint8_t last_ip[RTL_SCAN_CAPACITY][4],static_ip[RTL_SCAN_CAPACITY][4];
    char ip_text[16];
    uint32_t packets_rx,packets_tx,packets_failed,bytes_rx,bytes_tx;
    unsigned log_warning,log_failed,heap_bytes,ram_bytes;
    unsigned rx_kbps,tx_kbps,signal_quality,signal_valid,battery_percent,battery_set;
    char last_request[72],transfer_path[256];unsigned transfer_active,transfer_direction;
    uint32_t transfer_size,transfer_done;
    void (*error_sink)(void*,const char*);void *error_context;
    char password[64];unsigned int password_length;
    uint8_t pending_pmk[32],ip[4],mask[4],gateway[4];
    WifiProfile profile;unsigned int has_profile;
    uint32_t ping_rx,ping_tx,ping_failed,ping_pending;uint8_t ping_remote[4];uint16_t ping_id,ping_sequence;
    char message[96];
    char hub_message[96];
} WifiApp;
void wifi_app_init(WifiApp *app);
void wifi_app_driver(WifiApp *app,int result);
void wifi_app_input(WifiApp *app,uint32_t buttons,uint32_t now_ms);
void wifi_app_scan(WifiApp *app,uint32_t ticket,const RtlScanList *scan,int result);
void wifi_app_key(WifiApp *app,uint32_t ticket,const uint8_t pmk[32],int result);
void wifi_app_authenticated(WifiApp *app,uint32_t ticket);
void wifi_app_bound(WifiApp *app,uint32_t ticket,const uint8_t ip[4],const uint8_t mask[4],const uint8_t gateway[4]);
void wifi_app_failure(WifiApp *app,uint32_t ticket,const char *message);
void wifi_app_disconnected(WifiApp *app,const char *message);
void wifi_app_echo_rx(WifiApp *app,uint32_t ticket,const uint8_t remote[4],uint16_t id,uint16_t sequence);
void wifi_app_echo_tx(WifiApp *app,uint32_t ticket,int result);
void wifi_app_saved(WifiApp *app,int result);
unsigned int wifi_app_take_actions(WifiApp *app);
int wifi_ip_parse(const char *,uint8_t ip[4]);
const char *wifi_ip_key(unsigned row,unsigned column);
void wifi_pad_description(uint32_t buttons,char *out,size_t capacity);
unsigned int wifi_keyboard_columns(unsigned int row);
const char *wifi_keyboard_label(unsigned int row,unsigned int column,unsigned int shift,char out[12]);
/* A 2-slot credential journal stores a PMK, not the user's plaintext password.
 * PMKs still grant network access: keep the memory card/profile private.
 * CRC detects accidental corruption; it does not encrypt/authenticate storage. */
#define WIFI_PROFILE_BYTES 84
int wifi_profile_encode(const WifiProfile *profile,uint8_t out[WIFI_PROFILE_BYTES]);
int wifi_profile_decode(const uint8_t *data,size_t size,WifiProfile *profile);
int wifi_profile_newer(uint32_t a,uint32_t b);
#endif
