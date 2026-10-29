#define NEWLIB_PORT_AWARE

/*
 * PS2 Seda Elektronik USB Relay Controller
 *
 * Four physical motors in two parallel side groups. Each side is reversed
 * through a pair of SPDT relays (relay ON = COM->NO = "+",
 * relay OFF = COM->NC = "-"):
 *   Defaults: left R1/R2, right R3/R4. Each side can select any disjoint pair
 *   among R1/R2, R3/R4, R5/R6 and R7/R8 in Settings.
 *
 * Button mapping (level-triggered, held state each frame):
 *   X        -> relay 1 + relay 3  (both motors forward)
 *   TRIANGLE -> relay 2 + relay 4  (both motors reverse)
 *   SQUARE   -> relay 2 + relay 3  (turn)
 *   CIRCLE   -> relay 1 + relay 4  (turn other way)
 *
 * Normal driving interlocks the two relays of every selected pair. The
 * one-time startup check deliberately energises all eight, then stops them.
 * If the buttons driving both
 * sides of a motor are held at once, that motor's relays are forced OFF
 * instead of picking one, so opposing button presses never fight over
 * the same motor.
 *
 * The ELF embeds usbd.irx and a small Seda HID IOP driver. The IOP driver
 * detects VID 0x178E / PID 0x07D9 and sends Seda's HID report:
 * report[0] = 0, report[1] = 1, report[4] = command.
 */

#include <tamtypes.h>
#include <kernel.h>
#include <delaythread.h>
#include <sifrpc.h>
#include <iopcontrol.h>
#include <loadfile.h>
#include <iopheap.h>
#include <sbv_patches.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libpad.h>
#include <libmc.h>
#include <debug.h>

#include "seda_relay_rpc.h"
#include "relay_policy.h"

#define PAD_PORT 0
#define PAD_SLOT 0
#define SETTINGS_DIR "mc0:SYS-CONF"
#define SETTINGS_FILE "mc0:SYS-CONF/ROLECFG.TXT"
#define PASSWORD_LEN 12
#define PROGRAM_MAX_STEPS 128
#define PROGRAM_FILE "mc0:SYS-CONF/ROLEPRG.TXT"
/* Relay dead time: minimum all-off gap inserted before energising a new
   direction, so opposite relays are never hot at the same instant. */
#define DEAD_TIME_DEFAULT_MS 100
#define DEAD_TIME_MAX_MS 1000
#define DEAD_TIME_STEP_MS 10
#define SETTING_COUNT 7

enum ProgramAction {
    PROGRAM_ACTION_CROSS = 0,
    PROGRAM_ACTION_CIRCLE,
    PROGRAM_ACTION_TRIANGLE,
    PROGRAM_ACTION_SQUARE,
    PROGRAM_ACTION_WAIT
};

typedef struct {
    int usb_mode;
    int vid;
    int pid;
    int auto_load_usb;
    int dead_time_ms;
    int left_group;
    int right_group;
    int language;
} AppSettings;

enum UiState {
    UI_NORMAL = 0,
    UI_PASSWORD,
    UI_SETTINGS,
    UI_HEX_INPUT
};

extern unsigned char usbd_irx[];
extern unsigned int size_usbd_irx;
extern unsigned char seda_relay_irx[];
extern unsigned int size_seda_relay_irx;

static char pad_buf[256] __attribute__((aligned(64)));
static u8 relay_state[RELAY_COUNT] = {0};
static SifRpcClientData_t relay_rpc __attribute__((aligned(64)));
static SedaRelayRpcPacket relay_rpc_packet __attribute__((aligned(64)));
static int relay_rpc_ready = 0;
static int last_relay_result = 0;
static int last_relay_status = 0;
static int last_control_result = 0;
static int last_interrupt_result = 0;
static int usb_init_attempted = 0;
static int usb_send_mode = SEDA_RELAY_MODE_INT_PAYLOAD8;
static int mc_ready = 0;
static int settings_loaded = 0;
static int selected_setting = 0;
static int ui_state = UI_NORMAL;
static int password_pos = 0;
static int password_failed = 0;
static int hex_target = 0;
static int hex_pos = 0;
static int hex_cursor = 0;
static char hex_buffer[5] = "0000";
static u8 program_steps[PROGRAM_MAX_STEPS];
static int program_step_count = 0;
static int program_recording = 0;
static AppSettings settings = {
    SEDA_RELAY_MODE_INT_PAYLOAD8,
    0x178e,
    0x07d9,
    0,
    DEAD_TIME_DEFAULT_MS,
    0, 1, 0
};

#include "ui_locale.h"

static int startup_test_state = 0; /* 0: waiting, 1: complete, -1: failed */
static int drive_inhibited = 1;
static int relay_fault = 0;

/* Set when relays were switched off; the next energise waits the dead time. */
static int dead_time_pending = 0;

static const u8 relay_on_command[RELAY_COUNT] = {
    1,
    2,
    3,
    4, 5, 6, 7, 8
};

static const u8 relay_off_command[RELAY_COUNT] = {
    17,
    18,
    19,
    20, 21, 22, 23, 24
};

static const char *usb_send_mode_names[SEDA_RELAY_MODE_COUNT] = {
    "AUTO",
    "CTRL OUT IF LEN9 IDX4",
    "CTRL FEATURE IF LEN9 IDX4",
    "CTRL OUT DEV LEN9 IDX4",
    "INT LEN9 IDX4",
    "INT LEN8 IDX4",
    "INT LEN65 IDX4",
    "CTRL OUT IF LEN65 IDX4",
    "CTRL OUT IF LEN9 IDX5",
    "INT PAYLOAD8 NO RID",
    "CTRL OUT PAYLOAD8 NO RID",
    "CTRL FEATURE PAYLOAD8 NO RID"
};

static const u16 settings_password[PASSWORD_LEN] = {
    PAD_CROSS, PAD_CROSS, PAD_CROSS,
    PAD_SQUARE, PAD_SQUARE, PAD_SQUARE,
    PAD_TRIANGLE, PAD_TRIANGLE, PAD_TRIANGLE,
    PAD_CIRCLE, PAD_CIRCLE, PAD_CIRCLE
};

static const char hex_keys[] = "0123456789ABCDEF";

static int relay_get_status(void);
static int relays_all_off(void);

static const char *program_action_name(int action)
{
    switch (action) {
    case PROGRAM_ACTION_CROSS: return T("X / FORWARD");
    case PROGRAM_ACTION_CIRCLE: return T("CIRCLE / TURN");
    case PROGRAM_ACTION_TRIANGLE: return T("TRIANGLE / REVERSE");
    case PROGRAM_ACTION_SQUARE: return T("SQUARE / TURN");
    case PROGRAM_ACTION_WAIT: return T("R1 / WAIT");
    default: return T("UNKNOWN");
    }
}

static void log_printf(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vprintf(ui_translate(format, settings.language), args);
    va_end(args);

    va_start(args, format);
    scr_vprintf(ui_translate(format, settings.language), args);
    va_end(args);
}

static void cls(void)
{
    scr_clear();
    scr_setXY(0, 0);
}

static void print_header(void)
{
    log_printf("PS2 USB RELAY CONTROLLER\n");
    log_printf("Seda 8 Relay HID - hold mode\n");
    log_printf("----------------------------\n");
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}

static int parse_hex4(const char *text, int fallback)
{
    int i;
    int value = 0;

    for (i = 0; i < 4; i++) {
        int h = hex_value(text[i]);
        if (h < 0) {
            return fallback;
        }

        value = (value << 4) | h;
    }

    return value;
}

static void hex4_to_string(int value, char *out)
{
    static const char digits[] = "0123456789ABCDEF";
    int i;

    for (i = 0; i < 4; i++) {
        out[3 - i] = digits[value & 0x0f];
        value >>= 4;
    }

    out[4] = '\0';
}

static int parse_decimal_after(const char *text, const char *key, int fallback)
{
    const char *p = strstr(text, key);

    if (!p) {
        return fallback;
    }

    return atoi(p + strlen(key));
}

static int parse_hex_after(const char *text, const char *key, int fallback)
{
    const char *p = strstr(text, key);

    if (!p) {
        return fallback;
    }

    return parse_hex4(p + strlen(key), fallback);
}

static void apply_settings(void)
{
    if (settings.usb_mode < 0 || settings.usb_mode >= SEDA_RELAY_MODE_COUNT) {
        settings.usb_mode = SEDA_RELAY_MODE_INT_PAYLOAD8;
    }

    settings.vid &= 0xffff;
    settings.pid &= 0xffff;
    settings.auto_load_usb = settings.auto_load_usb ? 1 : 0;
    normalize_groups(&settings.left_group, &settings.right_group);
    settings.language = settings.language == 1 ? 1 : 0;

    if (settings.dead_time_ms < 0) {
        settings.dead_time_ms = 0;
    }
    if (settings.dead_time_ms > DEAD_TIME_MAX_MS) {
        settings.dead_time_ms = DEAD_TIME_MAX_MS;
    }

    usb_send_mode = settings.usb_mode;
}

static int initialize_memory_card(void)
{
    int ret;

    if (mc_ready) {
        return 0;
    }

    ret = SifLoadModule("rom0:XMCMAN", 0, NULL);
    if (ret < 0) {
        SifLoadModule("rom0:MCMAN", 0, NULL);
    }

    ret = SifLoadModule("rom0:XMCSERV", 0, NULL);
    if (ret < 0) {
        SifLoadModule("rom0:MCSERV", 0, NULL);
    }

    if (mcInit(MC_TYPE_XMC) < 0) {
        if (mcInit(MC_TYPE_MC) < 0) {
            mc_ready = 0;
            return -1;
        }
    }

    mc_ready = 1;
    return 0;
}

static int load_settings_from_mc(void)
{
    int fd;
    int size;
    char buffer[256];

    if (initialize_memory_card() < 0) {
        return -1;
    }

    fd = open(SETTINGS_FILE, O_RDONLY);
    if (fd < 0) {
        apply_settings();
        return -1;
    }

    memset(buffer, 0, sizeof(buffer));
    size = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (size <= 0) {
        apply_settings();
        return -1;
    }

    settings.usb_mode = parse_decimal_after(buffer, "mode=", settings.usb_mode);
    settings.vid = parse_hex_after(buffer, "vid=", settings.vid);
    settings.pid = parse_hex_after(buffer, "pid=", settings.pid);
    settings.auto_load_usb = parse_decimal_after(buffer, "autoload=", settings.auto_load_usb);
    settings.dead_time_ms = parse_decimal_after(buffer, "deadms=", settings.dead_time_ms);
    settings.left_group = parse_decimal_after(buffer, "leftgroup=", settings.left_group);
    settings.right_group = parse_decimal_after(buffer, "rightgroup=", settings.right_group);
    settings.language = parse_decimal_after(buffer, "language=", settings.language);
    apply_settings();
    settings_loaded = 1;

    return 0;
}

static int save_settings_to_mc(void)
{
    int fd;
    int len;
    char buffer[192];
    char vid[5];
    char pid[5];

    if (initialize_memory_card() < 0) {
        return -1;
    }

    mkdir(SETTINGS_DIR, 0777);

    hex4_to_string(settings.vid, vid);
    hex4_to_string(settings.pid, pid);

    len = snprintf(buffer, sizeof(buffer),
                   "ROLECFG v2\nmode=%d\nvid=%s\npid=%s\nautoload=%d\ndeadms=%d\nleftgroup=%d\nrightgroup=%d\nlanguage=%d\n",
                   settings.usb_mode, vid, pid, settings.auto_load_usb,
                   settings.dead_time_ms, settings.left_group, settings.right_group, settings.language);

    fd = open(SETTINGS_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        return -2;
    }

    if (write(fd, buffer, len) != len) {
        close(fd);
        return -3;
    }

    close(fd);
    settings_loaded = 1;
    return 0;
}

static void show_idle_screen(const char *message)
{
    int relay_status = relay_get_status();

    cls();
    print_header();

    if (message != NULL) {
        log_printf("%s\n\n", T(message));
    }

    log_printf("USB init: %s\n", T(usb_init_attempted ? "attempted" : "not loaded"));
    log_printf("USB relay status: %d\n\n", relay_status);
    log_printf("USB mode: %d %s\n\n", usb_send_mode, usb_send_mode_names[usb_send_mode]);
    log_printf("Settings: %s\n", T(settings_loaded ? "loaded from MC" : "defaults"));
    log_printf("VID/PID : %04X/%04X\n", settings.vid, settings.pid);
    log_printf("Dead time: %d ms\n\n", settings.dead_time_ms);
    log_printf("Left motors: R%d & R%d   Right: R%d & R%d\n",settings.left_group*2+1,settings.left_group*2+2,settings.right_group*2+1,settings.right_group*2+2);
    log_printf("X -> R%d+R%d : forward\n",settings.left_group*2+1,settings.right_group*2+1);
    log_printf("Triangle -> R%d+R%d : reverse\n",settings.left_group*2+2,settings.right_group*2+2);
    log_printf("Square -> R%d+R%d : turn left\n",settings.left_group*2+2,settings.right_group*2+1);
    log_printf("Circle -> R%d+R%d : turn right\n",settings.left_group*2+1,settings.right_group*2+2);
    log_printf("Startup 100ms: %s\n",ui_translate(startup_test_state>0?"complete":startup_test_state<0?"failed - drive blocked":"waiting for USB",settings.language));
    log_printf("Opposing pair held -> that motor OFF\n\n");
    log_printf("L1     : load USB relay driver\n");
    log_printf("L2     : run program saved on memory card\n");
    log_printf("R1     : settings\n");
    log_printf("SELECT: emergency stop\n");
    log_printf("START : record program\n");
    log_printf("START+SELECT: PS2 main menu\n");
}

static void show_password_screen(void)
{
    cls();
    print_header();
    log_printf("SETTINGS PASSWORD\n\n");
    log_printf("Enter sequence:\n");
    log_printf("X X X, Square Square Square,\n");
    log_printf("Triangle Triangle Triangle,\n");
    log_printf("Circle Circle Circle\n\n");
    log_printf("Input: %d/%d\n", password_pos, PASSWORD_LEN);

    if (password_failed) {
        log_printf("Wrong password. Try again.\n");
    }

    log_printf("\nSTART : confirm\n");
    log_printf("SELECT: cancel\n");
}

static void show_settings_screen(const char *message)
{
    char marker;

    cls();
    print_header();
    log_printf("SETTINGS\n\n");

    if (message) {
        log_printf("%s\n\n", T(message));
    }

    marker = selected_setting == 0 ? '>' : ' ';
    log_printf("%c USB mode : %d %s\n", marker, settings.usb_mode, usb_send_mode_names[settings.usb_mode]);

    marker = selected_setting == 1 ? '>' : ' ';
    log_printf("%c VID      : %04X\n", marker, settings.vid);

    marker = selected_setting == 2 ? '>' : ' ';
    log_printf("%c PID      : %04X\n", marker, settings.pid);

    marker = selected_setting == 3 ? '>' : ' ';
    log_printf("%c Dead time: %d ms\n", marker, settings.dead_time_ms);

    marker = selected_setting == 4 ? '>' : ' ';
    log_printf("%c Left motors : R%d & R%d\n", marker, settings.left_group*2+1, settings.left_group*2+2);
    marker = selected_setting == 5 ? '>' : ' ';
    log_printf("%c Right motors: R%d & R%d\n", marker, settings.right_group*2+1, settings.right_group*2+2);
    marker = selected_setting == 6 ? '>' : ' ';
    log_printf("%c Language: %s\n", marker, settings.language ? "TURKCE" : "ENGLISH");
    log_printf("Other motor's group is skipped automatically.\n");

    log_printf("\nUP/DOWN    : select\n");
    log_printf("LEFT/RIGHT : change\n");
    log_printf("X          : edit VID/PID\n");
    log_printf("START      : save\n");
    log_printf("SELECT     : exit\n");
}

static void show_hex_input_screen(void)
{
    int i;

    cls();
    print_header();
    log_printf("HEX INPUT - %s\n\n", hex_target == 1 ? "VID" : "PID");
    log_printf("Value: %s  pos %d/4\n\n", hex_buffer, hex_pos);

    for (i = 0; i < 16; i++) {
        log_printf("%c%c%c", i == hex_cursor ? '[' : ' ', hex_keys[i], i == hex_cursor ? ']' : ' ');
        if ((i % 4) == 3) {
            log_printf("\n");
        } else {
            log_printf(" ");
        }
    }

    log_printf("\nD-PAD : move\n");
    log_printf("X     : enter digit\n");
    log_printf("O     : backspace\n");
    log_printf("START : accept\n");
    log_printf("SELECT: cancel\n");
}

static void die(const char *message)
{
    log_printf("%s\n", T(message));
    SleepThread();
}

static int load_embedded_irx(const char *name, void *data, u32 size)
{
    int mod_result = 0;
    int ret;

    ret = SifExecModuleBuffer(data, size, 0, NULL, &mod_result);
    log_printf("%s load: ret=%d mod=%d\n", name, ret, mod_result);

    return ret;
}

static int load_embedded_irx_args(const char *name, void *data, u32 size, const char *args)
{
    int mod_result = 0;
    int ret;
    int arg_len = args ? (int)strlen(args) + 1 : 0;

    ret = SifExecModuleBuffer(data, size, arg_len, (char *)args, &mod_result);
    log_printf("%s load: ret=%d mod=%d\n", name, ret, mod_result);

    return ret;
}

static int bind_relay_rpc(void)
{
    int i;
    int ret;

    memset(&relay_rpc, 0, sizeof(relay_rpc));

    for (i = 0; i < 120; i++) {
        ret = SifBindRpc(&relay_rpc, SEDA_RELAY_RPC_ID, 0);
        if (ret < 0) {
            log_printf("Seda RPC bind failed: %d\n", ret);
            return -1;
        }

        if (relay_rpc.server != NULL) {
            relay_rpc_ready = 1;
            log_printf("Seda relay RPC ready\n");
            return 0;
        }

        nopdelay();
    }

    log_printf("Seda relay RPC not found\n");
    return -1;
}

static int initialize_usb_relay(void)
{
    char args[32];
    char vid[5];
    char pid[5];

    if (relay_rpc_ready) {
        return 0;
    }

    usb_init_attempted = 1;

    load_embedded_irx("usbd.irx", usbd_irx, size_usbd_irx);

    hex4_to_string(settings.vid, vid);
    hex4_to_string(settings.pid, pid);
    snprintf(args, sizeof(args), "vid=%s pid=%s", vid, pid);
    log_printf("USB target: VID=%s PID=%s\n", vid, pid);

    if (load_embedded_irx_args("seda_relay.irx", seda_relay_irx, size_seda_relay_irx, args) < 0) {
        log_printf("Seda relay IRX could not be loaded\n");
        return -1;
    }

    return bind_relay_rpc();
}

static int relay_get_status(void)
{
    int ret;

    if (!relay_rpc_ready) {
        return SEDA_RELAY_STATUS_DISCONNECTED;
    }

    memset(&relay_rpc_packet, 0, sizeof(relay_rpc_packet));
    ret = SifCallRpc(&relay_rpc,
                     SEDA_RELAY_RPC_GET_STATUS,
                     0,
                     &relay_rpc_packet,
                     sizeof(relay_rpc_packet),
                     &relay_rpc_packet,
                     sizeof(relay_rpc_packet),
                     NULL,
                     NULL);

    if (ret < 0) {
        relay_rpc_ready = 0;
        return SEDA_RELAY_STATUS_DISCONNECTED;
    }

    return relay_rpc_packet.status;
}

static void load_pad_modules(void)
{
    int ret;

    ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
    if (ret < 0) {
        ret = SifLoadModule("rom0:XSIO2MAN", 0, NULL);
        if (ret < 0) {
            die("Failed to load SIO2MAN/XSIO2MAN");
        }
    }

    ret = SifLoadModule("rom0:PADMAN", 0, NULL);
    if (ret < 0) {
        ret = SifLoadModule("rom0:XPADMAN", 0, NULL);
        if (ret < 0) {
            die("Failed to load PADMAN/XPADMAN");
        }
    }
}

static void wait_pad_ready(void)
{
    int state;
    int last_state = -1;
    char state_string[16];

    while (1) {
        state = padGetState(PAD_PORT, PAD_SLOT);

        if (state == PAD_STATE_STABLE || state == PAD_STATE_FINDCTP1) {
            if (last_state != -1) {
                log_printf("Controller ready\n");
            }
            return;
        }

        if (state != last_state) {
            padStateInt2String(state, state_string);
            log_printf("Controller state: %s\n", state_string);
            last_state = state;
        }
    }
}

static int relay_send_command(u8 command)
{
    int ret;

    if (!relay_rpc_ready) {
        last_relay_result = SEDA_RELAY_ERR_NOT_READY;
        last_relay_status = SEDA_RELAY_STATUS_DISCONNECTED;
        last_control_result = SEDA_RELAY_ERR_NOT_READY;
        last_interrupt_result = SEDA_RELAY_ERR_NOT_READY;
        return last_relay_result;
    }

    memset(&relay_rpc_packet, 0, sizeof(relay_rpc_packet));
    relay_rpc_packet.command = command;
    relay_rpc_packet.mode = usb_send_mode;

    ret = SifCallRpc(&relay_rpc,
                     SEDA_RELAY_RPC_SEND_COMMAND,
                     0,
                     &relay_rpc_packet,
                     sizeof(relay_rpc_packet),
                     &relay_rpc_packet,
                     sizeof(relay_rpc_packet),
                     NULL,
                     NULL);

    if (ret < 0) {
        relay_rpc_ready = 0;
        last_relay_result = ret;
        last_relay_status = SEDA_RELAY_STATUS_DISCONNECTED;
        last_control_result = ret;
        last_interrupt_result = ret;
        return ret;
    }

    last_relay_result = relay_rpc_packet.result;
    last_relay_status = relay_rpc_packet.status;
    last_control_result = relay_rpc_packet.control_result;
    last_interrupt_result = relay_rpc_packet.interrupt_result;

    return last_relay_result;
}

static void show_relay_event(int relay, int enabled, u8 command)
{
    cls();
    print_header();
    log_printf("Relay  : %d\n", relay + 1);
    log_printf("State  : %s\n", T(enabled ? "ON" : "OFF"));
    log_printf("Command: %u\n\n", command);
    log_printf("USB mode=%d %s\n", usb_send_mode, usb_send_mode_names[usb_send_mode]);
    log_printf("USB result=%d status=%d\n", last_relay_result, last_relay_status);
    log_printf("ctrl=%d int=%d\n\n", last_control_result, last_interrupt_result);
    log_printf("L1     : load USB relay driver\n");
    log_printf("L2     : run program saved on memory card\n");
    log_printf("R1     : settings\n");
    log_printf("SELECT: emergency stop\n");
    log_printf("START : record program\n");
    log_printf("START+SELECT: PS2 main menu\n");
}

static void enter_password_screen(void)
{
    relays_all_off();
    drive_inhibited = 1;
    ui_state = UI_PASSWORD;
    password_pos = 0;
    password_failed = 0;
    show_password_screen();
}

static void enter_settings_screen(void)
{
    ui_state = UI_SETTINGS;
    selected_setting = 0;
    show_settings_screen(NULL);
}

static void enter_hex_input(int target)
{
    hex_target = target;
    hex_pos = 0;
    hex_cursor = 0;

    if (hex_target == 1) {
        hex4_to_string(settings.vid, hex_buffer);
    } else {
        hex4_to_string(settings.pid, hex_buffer);
    }

    memset(hex_buffer, '0', 4);
    hex_buffer[4] = '\0';
    ui_state = UI_HEX_INPUT;
    show_hex_input_screen();
}

static int password_button_to_expected(u32 pressed_buttons)
{
    if (pressed_buttons & PAD_CROSS) {
        return PAD_CROSS;
    }

    if (pressed_buttons & PAD_SQUARE) {
        return PAD_SQUARE;
    }

    if (pressed_buttons & PAD_TRIANGLE) {
        return PAD_TRIANGLE;
    }

    if (pressed_buttons & PAD_CIRCLE) {
        return PAD_CIRCLE;
    }

    return 0;
}

static void handle_password_input(u32 pressed_buttons)
{
    int button;

    if (pressed_buttons & PAD_SELECT) {
        ui_state = UI_NORMAL;
        show_idle_screen("Settings canceled");
        return;
    }

    if (pressed_buttons & PAD_START) {
        if (password_pos == PASSWORD_LEN && !password_failed) {
            enter_settings_screen();
        } else {
            password_pos = 0;
            password_failed = 1;
            show_password_screen();
        }

        return;
    }

    button = password_button_to_expected(pressed_buttons);
    if (button == 0) {
        return;
    }

    if (password_pos >= PASSWORD_LEN || button != settings_password[password_pos]) {
        password_failed = 1;
    }

    if (password_pos < PASSWORD_LEN) {
        password_pos++;
    }

    show_password_screen();
}

static void handle_settings_input(u32 pressed_buttons)
{
    if (pressed_buttons & PAD_SELECT) {
        apply_settings();
        ui_state = UI_NORMAL;
        show_idle_screen("Settings closed");
        return;
    }

    if (pressed_buttons & PAD_START) {
        int ret;

        apply_settings();
        ret = save_settings_to_mc();
        show_settings_screen(ret == 0 ? "Settings saved to memory card" : "Settings save failed");
        return;
    }

    if (pressed_buttons & PAD_UP) {
        selected_setting--;
        if (selected_setting < 0) {
            selected_setting = SETTING_COUNT - 1;
        }
        show_settings_screen(NULL);
        return;
    }

    if (pressed_buttons & PAD_DOWN) {
        selected_setting++;
        if (selected_setting > SETTING_COUNT - 1) {
            selected_setting = 0;
        }
        show_settings_screen(NULL);
        return;
    }

    if (pressed_buttons & PAD_LEFT) {
        if (selected_setting == 0) {
            settings.usb_mode--;
            if (settings.usb_mode < 0) {
                settings.usb_mode = SEDA_RELAY_MODE_COUNT - 1;
            }
        } else if (selected_setting == 1) {
            settings.vid = (settings.vid - 1) & 0xffff;
        } else if (selected_setting == 2) {
            settings.pid = (settings.pid - 1) & 0xffff;
        } else if (selected_setting == 3) {
            settings.dead_time_ms -= DEAD_TIME_STEP_MS;
        } else if (selected_setting == 4) {
            settings.left_group = next_group(settings.left_group, settings.right_group, -1);
        } else if (selected_setting == 5) {
            settings.right_group = next_group(settings.right_group, settings.left_group, -1);
        } else if (selected_setting == 6) {
            settings.language = !settings.language;
        }

        apply_settings();
        show_settings_screen(NULL);
        return;
    }

    if (pressed_buttons & PAD_RIGHT) {
        if (selected_setting == 0) {
            settings.usb_mode++;
            if (settings.usb_mode >= SEDA_RELAY_MODE_COUNT) {
                settings.usb_mode = 0;
            }
        } else if (selected_setting == 1) {
            settings.vid = (settings.vid + 1) & 0xffff;
        } else if (selected_setting == 2) {
            settings.pid = (settings.pid + 1) & 0xffff;
        } else if (selected_setting == 3) {
            settings.dead_time_ms += DEAD_TIME_STEP_MS;
        } else if (selected_setting == 4) {
            settings.left_group = next_group(settings.left_group, settings.right_group, 1);
        } else if (selected_setting == 5) {
            settings.right_group = next_group(settings.right_group, settings.left_group, 1);
        } else if (selected_setting == 6) {
            settings.language = !settings.language;
        }

        apply_settings();
        show_settings_screen(NULL);
        return;
    }

    if (pressed_buttons & PAD_CROSS) {
        if (selected_setting == 1 || selected_setting == 2) {
            enter_hex_input(selected_setting);
        }
    }
}

static void handle_hex_input(u32 pressed_buttons)
{
    if (pressed_buttons & PAD_SELECT) {
        ui_state = UI_SETTINGS;
        show_settings_screen("Edit canceled");
        return;
    }

    if (pressed_buttons & PAD_LEFT) {
        hex_cursor = (hex_cursor + 15) & 15;
        show_hex_input_screen();
        return;
    }

    if (pressed_buttons & PAD_RIGHT) {
        hex_cursor = (hex_cursor + 1) & 15;
        show_hex_input_screen();
        return;
    }

    if (pressed_buttons & PAD_UP) {
        hex_cursor = (hex_cursor + 12) & 15;
        show_hex_input_screen();
        return;
    }

    if (pressed_buttons & PAD_DOWN) {
        hex_cursor = (hex_cursor + 4) & 15;
        show_hex_input_screen();
        return;
    }

    if (pressed_buttons & PAD_CIRCLE) {
        if (hex_pos > 0) {
            hex_pos--;
            hex_buffer[hex_pos] = '0';
        }
        show_hex_input_screen();
        return;
    }

    if (pressed_buttons & PAD_CROSS) {
        if (hex_pos < 4) {
            hex_buffer[hex_pos] = hex_keys[hex_cursor];
            hex_pos++;
        }
        show_hex_input_screen();
        return;
    }

    if (pressed_buttons & PAD_START) {
        int value;

        if (hex_pos < 4) {
            show_hex_input_screen();
            return;
        }

        value = parse_hex4(hex_buffer, 0);
        if (hex_target == 1) {
            settings.vid = value;
        } else {
            settings.pid = value;
        }

        apply_settings();
        ui_state = UI_SETTINGS;
        show_settings_screen("Hex value updated");
    }
}

static int relay_set(int relay, int enabled)
{
    u8 command;
    int result;

    if (relay < 0 || relay >= RELAY_COUNT) {
        return SEDA_RELAY_ERR_BAD_COMMAND;
    }

    if (relay_state[relay] == (enabled ? 1 : 0)) {
        return SEDA_RELAY_OK;
    }

    command = enabled ? relay_on_command[relay] : relay_off_command[relay];
    result = relay_send_command(command);
    if (result == SEDA_RELAY_OK) {
        relay_state[relay] = enabled ? 1 : 0;
    } else {
        relay_fault = 1;
    }
    show_relay_event(relay, enabled, command);
    return result;
}

static int relays_all_off(void)
{
    int i;
    int was_on = 0;
    int all_ok = 1;

    for (i = 0; i < RELAY_COUNT; i++) {
        if (relay_state[i]) {
            was_on = 1;
        }
    }

    for (i = 0; i < RELAY_COUNT; i++) {
        int result = relay_send_command(relay_off_command[i]);
        if (result == SEDA_RELAY_OK) {
            relay_state[i] = 0;
        } else {
            all_ok = 0;
        }
    }

    /* Something was energised: the next energise must wait the dead time. */
    if (was_on) {
        dead_time_pending = 1;
    }
    relay_fault = !all_ok;
    return all_ok ? SEDA_RELAY_OK : SEDA_RELAY_ERR_USB;
}

/* The documented SPDT wiring brakes each motor when BOTH terminals are on +.
   This single startup check intentionally energises every pair, outside the
   normal direction interlock. No unverified broadcast HID commands are used. */
static void startup_relay_check(void)
{
    int i, success = 1;
    if (startup_test_state != 0 || relay_get_status() != SEDA_RELAY_STATUS_READY) return;
    if (relays_all_off() != SEDA_RELAY_OK) {
        startup_test_state = -1; drive_inhibited = 1; return;
    }
    for (i = 0; i < RELAY_COUNT; i++) {
        if (relay_send_command(relay_on_command[i]) != SEDA_RELAY_OK) { success = 0; break; }
        relay_state[i] = 1;
    }
    if (success) DelayThread(STARTUP_PULSE_MS * 1000);
    if (relays_all_off() != SEDA_RELAY_OK) success = 0;
    startup_test_state = success ? 1 : -1;
    drive_inhibited = 1;
    show_idle_screen(success ? "Startup relay check complete" : "Startup relay check failed; drive blocked");
}

/* Blocks for the configured dead time if relays were just switched off.
   Called only right before a relay is energised, so simply adding a relay
   to an already running direction costs nothing. */
static void relay_dead_time_wait(void)
{
    if (!dead_time_pending) {
        return;
    }

    dead_time_pending = 0;

    if (settings.dead_time_ms > 0) {
        DelayThread(settings.dead_time_ms * 1000);
    }
}

static void apply_motor_relays(int motor_a_fwd, int motor_a_rev, int motor_b_fwd, int motor_b_rev)
{
    int desired[RELAY_COUNT];
    int turned_off = 0;
    int off_failed = 0;
    int i;

    if (startup_test_state != 1 || relay_fault) return;
    motor_relay_mask(desired, settings.left_group, settings.right_group,
                     motor_a_fwd, motor_a_rev, motor_b_fwd, motor_b_rev);

    for (i = 0; i < RELAY_COUNT; i++) {
        if (relay_state[i] && !desired[i]) {
            if (relay_set(i, 0) != SEDA_RELAY_OK) {
                off_failed = 1;
            }
            turned_off = 1;
        }
    }

    /* Never energise an opposite direction while an OFF command is unconfirmed. */
    if (off_failed) {
        relays_all_off();
        drive_inhibited = 1;
        return;
    }

    /* Complete the all-off interval immediately after a live relay release.
       This also covers the overlap case X+TRIANGLE: both sides turn off in one
       frame, then releasing one button cannot energise the opposite side on
       the very next frame without the configured dead time having elapsed. */
    if (turned_off) {
        dead_time_pending = 1;
        relay_dead_time_wait();
    }

    for (i = 0; i < RELAY_COUNT; i++) {
        if (!relay_state[i] && desired[i]) {
            /* Wait once, only when a relay really has to be energised. */
            relay_dead_time_wait();
            if (relay_set(i, 1) != SEDA_RELAY_OK) { relays_all_off(); drive_inhibited = 1; return; }
        }
    }

}

static char program_action_to_char(int action)
{
    switch (action) {
    case PROGRAM_ACTION_CROSS:    return 'X';
    case PROGRAM_ACTION_CIRCLE:   return 'O';
    case PROGRAM_ACTION_TRIANGLE: return 'T';
    case PROGRAM_ACTION_SQUARE:   return 'S';
    default:                      return 'W';
    }
}

static int program_char_to_action(char c)
{
    switch (c) {
    case 'X': case 'x': return PROGRAM_ACTION_CROSS;
    case 'O': case 'o': return PROGRAM_ACTION_CIRCLE;
    case 'T': case 't': return PROGRAM_ACTION_TRIANGLE;
    case 'S': case 's': return PROGRAM_ACTION_SQUARE;
    case 'W': case 'w': return PROGRAM_ACTION_WAIT;
    default:            return -1;
    }
}

/* Writes the recorded sequence to the memory card as plain text:
   ROLEPRG v1
   steps=XXOTSW                                                      */
static int save_program_to_mc(void)
{
    int fd;
    int len;
    int i;
    char buffer[PROGRAM_MAX_STEPS + 32];

    if (program_step_count <= 0) {
        return -1;
    }

    if (initialize_memory_card() < 0) {
        return -2;
    }

    mkdir(SETTINGS_DIR, 0777);

    len = snprintf(buffer, sizeof(buffer), "ROLEPRG v1\nsteps=");
    for (i = 0; i < program_step_count && len < (int)sizeof(buffer) - 2; i++) {
        buffer[len++] = program_action_to_char(program_steps[i]);
    }
    buffer[len++] = '\n';

    fd = open(PROGRAM_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        return -3;
    }

    if (write(fd, buffer, len) != len) {
        close(fd);
        return -4;
    }

    close(fd);
    return 0;
}

/* Reads the saved sequence back into program_steps. Returns the step count,
   or a negative value on failure. */
static int load_program_from_mc(void)
{
    int fd;
    int size;
    int i;
    int count = 0;
    const char *cursor;
    char buffer[PROGRAM_MAX_STEPS + 64];

    if (initialize_memory_card() < 0) {
        return -1;
    }

    fd = open(PROGRAM_FILE, O_RDONLY);
    if (fd < 0) {
        return -2;
    }

    memset(buffer, 0, sizeof(buffer));
    size = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (size <= 0) {
        return -3;
    }

    cursor = strstr(buffer, "steps=");
    if (cursor == NULL) {
        return -4;
    }
    cursor += 6;

    for (i = 0; cursor[i] != '\0' && count < PROGRAM_MAX_STEPS; i++) {
        int action;

        if (cursor[i] == '\n' || cursor[i] == '\r') {
            break;
        }
        if (cursor[i] == ',' || cursor[i] == ' ') {
            continue;
        }

        action = program_char_to_action(cursor[i]);
        if (action < 0) {
            return -5;
        }

        program_steps[count++] = (u8)action;
    }

    if (count == 0) {
        return -6;
    }

    program_step_count = count;
    return count;
}

static void show_program_screen(const char *message)
{
    int i;
    int first;

    cls();
    print_header();
    log_printf("PROGRAM MODE - motors are OFF\n");
    log_printf("-----------------------------\n");
    if (message) {
        log_printf("%s\n\n", ui_translate(message, settings.language));
    }
    log_printf("Recorded steps: %d / %d\n", program_step_count, PROGRAM_MAX_STEPS);
    first = program_step_count > 12 ? program_step_count - 12 : 0;
    for (i = first; i < program_step_count; i++) {
        log_printf("%3d. %s\n", i + 1, program_action_name(program_steps[i]));
    }
    log_printf("\nX/CIRCLE/TRIANGLE/SQUARE: add movement\n");
    log_printf("R1: add 1 second wait\n");
    log_printf("L1: save to memory card\n");
    log_printf("L2: load from memory card\n");
    log_printf("START: run sequence   SELECT: cancel\n");
}

static void program_add_step(int action)
{
    if (program_step_count >= PROGRAM_MAX_STEPS) {
        show_program_screen("Memory full; press START to run");
        return;
    }

    program_steps[program_step_count++] = (u8)action;
    show_program_screen("Step recorded");
}

static void apply_program_action(int action)
{
    switch (action) {
    case PROGRAM_ACTION_CROSS:
        apply_motor_relays(1, 0, 1, 0);
        break;
    case PROGRAM_ACTION_CIRCLE:
        apply_motor_relays(1, 0, 0, 1);
        break;
    case PROGRAM_ACTION_TRIANGLE:
        apply_motor_relays(0, 1, 0, 1);
        break;
    case PROGRAM_ACTION_SQUARE:
        apply_motor_relays(0, 1, 1, 0);
        break;
    default:
        relays_all_off();
        break;
    }
}

/* Wait in short slices so SELECT can always stop an executing program. */
static int program_wait_seconds(int seconds)
{
    struct padButtonStatus buttons;
    int ticks = seconds * 100;
    int i;

    for (i = 0; i < ticks; i++) {
        int state = padGetState(PAD_PORT, PAD_SLOT);
        if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) {
            relays_all_off();
            return -1;
        }
        if (padRead(PAD_PORT, PAD_SLOT, &buttons) != 0) {
            u32 held = 0xffff ^ buttons.btns;
            if (held & PAD_SELECT) {
                relays_all_off();
                return -1;
            }
        }
        DelayThread(10000);
    }
    return 0;
}

static void run_program(void)
{
    int i = 0;

    program_recording = 0;
    if (startup_test_state != 1 || relay_fault) {
        show_idle_screen("Drive blocked: startup check incomplete");
        return;
    }
    if (relays_all_off() != SEDA_RELAY_OK) {
        show_idle_screen("Program blocked: relays could not be confirmed OFF");
        return;
    }
    cls();
    print_header();
    if (program_step_count == 0) {
        show_idle_screen("Program empty; nothing executed");
        return;
    }

    while (i < program_step_count) {
        int action = program_steps[i];
        int duration = 1;

        /* Consecutive equal commands stay continuously ON, e.g. X,X = 2 s. */
        while (i + duration < program_step_count && program_steps[i + duration] == action) {
            duration++;
        }

        cls();
        print_header();
        log_printf("RUNNING PROGRAM\n\n");
        log_printf("Step %d-%d / %d\n", i + 1, i + duration, program_step_count);
        log_printf("Action: %s\n", program_action_name(action));
        log_printf("Duration: %d second(s)\n\n", duration);
        log_printf("SELECT: emergency stop\n");

        apply_program_action(action);
        if (program_wait_seconds(duration) < 0) {
            program_step_count = 0;
            show_idle_screen("Program aborted; OFF commands sent");
            return;
        }
        if (relays_all_off() != SEDA_RELAY_OK) {
            program_step_count = 0;
            show_idle_screen("Program stopped: relay OFF failed");
            return;
        }
        i += duration;
    }

    program_step_count = 0;
    show_idle_screen("Program complete; all relays OFF");
}

static void enter_program_mode(void)
{
    relays_all_off();
    program_step_count = 0;
    program_recording = 1;
    show_program_screen("Recording started");
}

static void reset_iop_for_clean_boot(void)
{
    SifInitRpc(0);
    while (!SifIopReset("", 0)) {
    }
    while (!SifIopSync()) {
    }
    SifInitRpc(0);
    SifLoadFileInit();
    SifInitIopHeap();
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
}

static void return_to_ps2_menu(void)
{
    relays_all_off();
    cls();
    print_header();
    log_printf("Returning to PS2 main menu...\n");
    SifExitRpc();
    while (!SifIopReset("", 0)) {
    }
    while (!SifIopSync()) {
    }
    SifInitRpc(0);
    LoadExecPS2("rom0:OSDSYS", 0, NULL);
}

static int initialize_pad(void)
{
    int ret;

    load_pad_modules();

    if (padInit(0) != 1) {
        log_printf("padInit failed\n");
        return -1;
    }

    ret = padPortOpen(PAD_PORT, PAD_SLOT, pad_buf);
    if (ret == 0) {
        log_printf("padPortOpen failed\n");
        return -1;
    }

    wait_pad_ready();

    return 0;
}

int main(int argc, char *argv[])
{
    struct padButtonStatus buttons;
    u32 current_buttons = 0;
    u32 previous_buttons = 0;
    u32 pressed_buttons;
    int disconnected = 0;

    (void)argc;
    (void)argv;

    reset_iop_for_clean_boot();
    init_scr();

    print_header();
    log_printf("Starting pad first...\n\n");

    if (initialize_pad() < 0) {
        die("Controller initialization failed");
    }

    if (load_settings_from_mc() == 0) {
        show_idle_screen("Settings loaded");
    } else {
        show_idle_screen("Using default settings");
    }

    /* The requested power-on check requires USB even with a legacy autoload=0. */
    initialize_usb_relay();
    show_idle_screen("Waiting for USB startup check");

    while (1) {
        int state = padGetState(PAD_PORT, PAD_SLOT);
        static int startup_poll = 0;
        DelayThread(10000);
        if (startup_test_state == 0 && ui_state == UI_NORMAL && !program_recording && ++startup_poll >= 25) {
            startup_poll = 0;
            startup_relay_check();
        }

        if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) {
            if (!disconnected) {
                log_printf("Controller disconnected or not stable\n");
                relays_all_off();
                previous_buttons = 0;
                disconnected = 1;
            }
            continue;
        }

        disconnected = 0;

        if (padRead(PAD_PORT, PAD_SLOT, &buttons) == 0) {
            continue;
        }

        current_buttons = 0xffff ^ buttons.btns;
        pressed_buttons = current_buttons & ~previous_buttons;

        if ((current_buttons & (PAD_START | PAD_SELECT)) == (PAD_START | PAD_SELECT)) {
            return_to_ps2_menu();
            previous_buttons = current_buttons;
            continue;
        }

        if (ui_state == UI_PASSWORD) {
            handle_password_input(pressed_buttons);
            previous_buttons = current_buttons;
            continue;
        }

        if (ui_state == UI_SETTINGS) {
            handle_settings_input(pressed_buttons);
            previous_buttons = current_buttons;
            continue;
        }

        if (ui_state == UI_HEX_INPUT) {
            handle_hex_input(pressed_buttons);
            previous_buttons = current_buttons;
            continue;
        }

        if (program_recording) {
            if (pressed_buttons & PAD_SELECT) {
                relays_all_off();
                program_recording = 0;
                program_step_count = 0;
                show_idle_screen("Program canceled; all relays OFF");
            } else if (pressed_buttons & PAD_START) {
                run_program();
            } else if (pressed_buttons & PAD_L1) {
                int ret = save_program_to_mc();
                if (ret == 0) {
                    show_program_screen("Program saved to memory card");
                } else if (ret == -1) {
                    show_program_screen("Nothing recorded yet");
                } else {
                    show_program_screen("Memory card save failed");
                }
            } else if (pressed_buttons & PAD_L2) {
                int ret = load_program_from_mc();
                if (ret > 0) {
                    show_program_screen("Program loaded from memory card");
                } else {
                    show_program_screen("No valid program on memory card");
                }
            } else {
                if (pressed_buttons & PAD_CROSS) program_add_step(PROGRAM_ACTION_CROSS);
                if (pressed_buttons & PAD_CIRCLE) program_add_step(PROGRAM_ACTION_CIRCLE);
                if (pressed_buttons & PAD_TRIANGLE) program_add_step(PROGRAM_ACTION_TRIANGLE);
                if (pressed_buttons & PAD_SQUARE) program_add_step(PROGRAM_ACTION_SQUARE);
                if (pressed_buttons & PAD_R1) program_add_step(PROGRAM_ACTION_WAIT);
            }

            previous_buttons = current_buttons;
            continue;
        }

        if (pressed_buttons & PAD_SELECT) {
            relays_all_off();
            drive_inhibited = 1;
            show_idle_screen("Emergency stop; release movement buttons");
            previous_buttons = current_buttons;
            continue;
        }

        if (pressed_buttons & PAD_START) {
            enter_program_mode();
            previous_buttons = current_buttons;
            continue;
        }

        if (pressed_buttons & PAD_R1) {
            enter_password_screen();
            previous_buttons = current_buttons;
            continue;
        }

        /* One button recall: load the saved sequence and run it at once. */
        if (pressed_buttons & PAD_L2) {
            int ret = load_program_from_mc();

            if (ret > 0) {
                run_program();
            } else {
                show_idle_screen("No valid program on memory card");
            }

            previous_buttons = current_buttons;
            continue;
        }

        if (pressed_buttons & PAD_L1) {
            cls();
            print_header();
            log_printf("Loading USB relay driver...\n");

            if (initialize_usb_relay() < 0) {
                log_printf("USB relay driver unavailable\n");
            } else {
                log_printf("USB relay driver loaded\n");
                log_printf("USB relay status: %d\n", relay_get_status());
            }

            if (startup_test_state < 0 && relays_all_off() == SEDA_RELAY_OK) startup_test_state = 0;
            startup_relay_check();

            log_printf("\nPress relay buttons to test\n");
            previous_buttons = current_buttons;
            continue;
        }

        {
            int cross = (current_buttons & PAD_CROSS) != 0;
            int square = (current_buttons & PAD_SQUARE) != 0;
            int triangle = (current_buttons & PAD_TRIANGLE) != 0;
            int circle = (current_buttons & PAD_CIRCLE) != 0;

            int motor_a_fwd_src = cross || circle;
            int motor_a_rev_src = triangle || square;
            int motor_b_fwd_src = cross || square;
            int motor_b_rev_src = triangle || circle;

            if (drive_inhibited) {
                if (!(cross || square || triangle || circle)) drive_inhibited = 0;
            } else {
                apply_motor_relays(motor_a_fwd_src, motor_a_rev_src, motor_b_fwd_src, motor_b_rev_src);
            }
        }

        previous_buttons = current_buttons;
    }

    return 0;
}
