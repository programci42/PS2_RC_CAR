#include <types.h>
#include <irx.h>
#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>
#include <thsemap.h>
#include <sifcmd.h>
#include <sifrpc.h>
#include <usbd.h>
#include <usbd_macro.h>

#include "../seda_relay_rpc.h"

#define MODNAME "Seda USB HID relay driver"

IRX_ID(MODNAME, 1, 0);

#define SEDA_VID 0x178e
#define SEDA_PID 0x07d9
#define SEDA_HID_PACKET_SIZE 9
#define SEDA_HID_FALLBACK_PACKET_SIZE 65

typedef struct {
    int dev_id;
    int control_ep;
    int interrupt_out_ep;
    int interface_no;
    int status;
    int configured;
    int transfer_result;
    int transfer_bytes;
    int interrupt_out_packet_size;
    u8 report[SEDA_HID_FALLBACK_PACKET_SIZE] __attribute__((aligned(64)));
} SedaRelayDevice;

static SedaRelayDevice relay_dev;
static int lock_sema = -1;
static int done_sema = -1;

static SifRpcDataQueue_t rpc_queue __attribute__((aligned(16)));
static SifRpcServerData_t rpc_server __attribute__((aligned(16)));
static SedaRelayRpcPacket rpc_buffer __attribute__((aligned(16)));
static int target_vid = SEDA_VID;
static int target_pid = SEDA_PID;

static int seda_probe(int dev_id);
static int seda_connect(int dev_id);
static int seda_disconnect(int dev_id);
static void rpc_thread(void *arg);
static void *rpc_handler(int command, void *buffer, int size);

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

static int parse_hex4(const char *s, int fallback)
{
    int i;
    int value = 0;

    for (i = 0; i < 4; i++) {
        int h = hex_value(s[i]);
        if (h < 0) {
            return fallback;
        }

        value = (value << 4) | h;
    }

    return value;
}

static void parse_module_args(int argc, char *argv[])
{
    char *args = (char *)argv;
    int i;

    if (argc <= 0 || args == NULL) {
        return;
    }

    for (i = 0; i + 8 < argc; i++) {
        if (args[i] == 'v' && args[i + 1] == 'i' && args[i + 2] == 'd' && args[i + 3] == '=') {
            target_vid = parse_hex4(&args[i + 4], target_vid);
        }

        if (args[i] == 'p' && args[i + 1] == 'i' && args[i + 2] == 'd' && args[i + 3] == '=') {
            target_pid = parse_hex4(&args[i + 4], target_pid);
        }
    }
}

static sceUsbdLddOps seda_driver = {
    NULL,
    NULL,
    "sedaRelay",
    seda_probe,
    seda_connect,
    seda_disconnect,
    0,
    0,
    0,
    0,
    0,
    NULL
};

static void transfer_done(int result, int bytes, void *arg)
{
    (void)arg;
    relay_dev.transfer_result = result;
    relay_dev.transfer_bytes = bytes;
    SignalSema(done_sema);
}

static void config_done(int result, int bytes, void *arg)
{
    (void)bytes;
    (void)arg;

    if (result == USB_RC_OK) {
        relay_dev.configured = 1;
        relay_dev.status = SEDA_RELAY_STATUS_READY;
        printf("SEDA RELAY: USB configuration ready\n");
    } else {
        relay_dev.configured = 0;
        relay_dev.status = SEDA_RELAY_STATUS_CONNECTED;
        printf("SEDA RELAY: set configuration failed %d\n", result);
    }
}

static int wait_transfer_result(int start_result)
{
    int i;

    if (start_result != USB_RC_OK) {
        return start_result;
    }

    for (i = 0; i < 300; i++) {
        if (PollSema(done_sema) == 0) {
            return relay_dev.transfer_result;
        }

        DelayThread(10000);
    }

    printf("SEDA RELAY: USB transfer timeout\n");
    relay_dev.transfer_result = -1000;
    return relay_dev.transfer_result;
}

static void clear_done_sema(void)
{
    while (PollSema(done_sema) == 0) {
    }
}

static void fill_seda_report(int command, int command_index)
{
    memset(relay_dev.report, 0, sizeof(relay_dev.report));
    relay_dev.report[0] = 0x00;
    relay_dev.report[1] = 0x01;

    if (command_index >= 0 && command_index < SEDA_HID_FALLBACK_PACKET_SIZE) {
        relay_dev.report[command_index] = (u8)command;
    }
}

static void fill_seda_payload_report(int command)
{
    memset(relay_dev.report, 0, sizeof(relay_dev.report));
    relay_dev.report[0] = 0x01;
    relay_dev.report[3] = (u8)command;
}

static int send_control_report(int report_type, int recipient, int report_len)
{
    int start_result;

    clear_done_sema();

    start_result = sceUsbdControlTransfer(
        relay_dev.control_ep,
        USB_DIR_OUT | USB_TYPE_CLASS | recipient,
        USB_REQ_SET_REPORT,
        (report_type << 8) | relay_dev.report[0],
        recipient == USB_RECIP_INTERFACE ? relay_dev.interface_no : 0,
        report_len,
        relay_dev.report,
        transfer_done,
        &relay_dev);

    return wait_transfer_result(start_result);
}

static int send_interrupt_report(int report_len)
{
    int start_result;

    if (relay_dev.interrupt_out_ep < 0) {
        return SEDA_RELAY_ERR_NOT_READY;
    }

    clear_done_sema();

    start_result = sceUsbdInterruptTransfer(
        relay_dev.interrupt_out_ep,
        relay_dev.report,
        report_len,
        transfer_done,
        &relay_dev);

    return wait_transfer_result(start_result);
}

static UsbInterfaceDescriptor *find_hid_interface(int dev_id, UsbConfigDescriptor *conf)
{
    u8 *ptr = (u8 *)conf + conf->bLength;
    u8 *end = (u8 *)conf + conf->wTotalLength;

    while (ptr + 2 <= end) {
        u8 len = ptr[0];
        u8 type = ptr[1];

        if (len == 0 || ptr + len > end) {
            break;
        }

        if (type == USB_DT_INTERFACE) {
            UsbInterfaceDescriptor *intf = (UsbInterfaceDescriptor *)ptr;
            if (intf->bInterfaceClass == USB_CLASS_HID) {
                (void)dev_id;
                return intf;
            }
        }

        ptr += len;
    }

    return NULL;
}

static UsbEndpointDescriptor *find_interrupt_out_endpoint(UsbInterfaceDescriptor *intf, UsbConfigDescriptor *conf)
{
    int endpoints_seen = 0;
    u8 *ptr = (u8 *)intf + intf->bLength;
    u8 *end = (u8 *)conf + conf->wTotalLength;

    while (ptr + 2 <= end && endpoints_seen < intf->bNumEndpoints) {
        u8 len = ptr[0];
        u8 type = ptr[1];

        if (len == 0 || ptr + len > end) {
            break;
        }

        if (type == USB_DT_INTERFACE) {
            break;
        }

        if (type == USB_DT_ENDPOINT) {
            UsbEndpointDescriptor *ep = (UsbEndpointDescriptor *)ptr;
            endpoints_seen++;

            if (((ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_INT) &&
                ((ep->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT)) {
                return ep;
            }
        }

        ptr += len;
    }

    return NULL;
}

static int seda_probe(int dev_id)
{
    UsbDeviceDescriptor *dev;
    UsbConfigDescriptor *conf;
    UsbInterfaceDescriptor *intf;

    dev = sceUsbdScanStaticDescriptor(dev_id, NULL, USB_DT_DEVICE);
    if (!dev) {
        return 0;
    }

    if (dev->idVendor != target_vid || dev->idProduct != target_pid) {
        return 0;
    }

    conf = sceUsbdScanStaticDescriptor(dev_id, dev, USB_DT_CONFIG);
    if (!conf) {
        return 0;
    }

    intf = find_hid_interface(dev_id, conf);
    if (!intf) {
        return 0;
    }

    printf("SEDA RELAY: matched VID=%04x PID=%04x\n", dev->idVendor, dev->idProduct);
    return 1;
}

static int seda_connect(int dev_id)
{
    UsbDeviceDescriptor *dev;
    UsbConfigDescriptor *conf;
    UsbInterfaceDescriptor *intf;
    UsbEndpointDescriptor *out_ep;

    dev = sceUsbdScanStaticDescriptor(dev_id, NULL, USB_DT_DEVICE);
    if (!dev) {
        return 1;
    }

    conf = sceUsbdScanStaticDescriptor(dev_id, dev, USB_DT_CONFIG);
    if (!conf) {
        return 1;
    }

    intf = find_hid_interface(dev_id, conf);
    if (!intf) {
        return 1;
    }

    memset(&relay_dev, 0, sizeof(relay_dev));
    relay_dev.control_ep = -1;
    relay_dev.dev_id = dev_id;
    relay_dev.control_ep = sceUsbdOpenPipe(dev_id, NULL);
    if (relay_dev.control_ep < 0) {
        relay_dev.status = SEDA_RELAY_STATUS_DISCONNECTED;
        printf("SEDA RELAY: control endpoint open failed %d\n", relay_dev.control_ep);
        return 1;
    }

    relay_dev.interface_no = intf->bInterfaceNumber;
    relay_dev.status = SEDA_RELAY_STATUS_CONNECTED;

    out_ep = find_interrupt_out_endpoint(intf, conf);
    if (out_ep) {
        relay_dev.interrupt_out_ep = sceUsbdOpenPipe(dev_id, out_ep);
        relay_dev.interrupt_out_packet_size = out_ep->wMaxPacketSizeLB | ((int)out_ep->wMaxPacketSizeHB << 8);
        printf("SEDA RELAY: interrupt OUT endpoint open %d\n", relay_dev.interrupt_out_ep);
        printf("SEDA RELAY: interrupt OUT packet size %d\n", relay_dev.interrupt_out_packet_size);
    } else {
        relay_dev.interrupt_out_ep = -1;
        printf("SEDA RELAY: no interrupt OUT endpoint, using SET_REPORT\n");
    }

    sceUsbdSetPrivateData(dev_id, &relay_dev);
    sceUsbdSetConfiguration(relay_dev.control_ep, conf->bConfigurationValue, config_done, &relay_dev);

    printf("SEDA RELAY: connected, interface %d\n", relay_dev.interface_no);
    return 0;
}

static int seda_disconnect(int dev_id)
{
    if (relay_dev.dev_id == dev_id) {
        if (relay_dev.control_ep >= 0) {
            sceUsbdClosePipe(relay_dev.control_ep);
        }

        if (relay_dev.interrupt_out_ep >= 0) {
            sceUsbdClosePipe(relay_dev.interrupt_out_ep);
        }

        memset(&relay_dev, 0, sizeof(relay_dev));
        relay_dev.control_ep = -1;
        relay_dev.interrupt_out_ep = -1;
        relay_dev.status = SEDA_RELAY_STATUS_DISCONNECTED;
        printf("SEDA RELAY: disconnected\n");
    }

    return 0;
}

static int seda_send_command(int command, int mode, int *control_result, int *interrupt_result)
{
    int command_indexes[2] = {4, 5};
    int report_lengths[3] = {SEDA_HID_PACKET_SIZE, 8, SEDA_HID_FALLBACK_PACKET_SIZE};
    int report_types[2] = {2, 3};
    int recipients[2] = {USB_RECIP_INTERFACE, USB_RECIP_DEVICE};
    int ci;
    int li;
    int ti;
    int ri;
    int result;
    int final_control = SEDA_RELAY_ERR_NOT_READY;
    int final_interrupt = SEDA_RELAY_ERR_NOT_READY;
    int any_success = 0;

    if (command < 0 || command > 255) {
        return SEDA_RELAY_ERR_BAD_COMMAND;
    }

    if (relay_dev.status == SEDA_RELAY_STATUS_DISCONNECTED || relay_dev.control_ep < 0) {
        return SEDA_RELAY_ERR_NOT_READY;
    }

    if (mode < 0 || mode >= SEDA_RELAY_MODE_COUNT) {
        mode = SEDA_RELAY_MODE_AUTO;
    }

    WaitSema(lock_sema);

    if (mode == SEDA_RELAY_MODE_AUTO) {
        fill_seda_payload_report(command);
        final_interrupt = send_interrupt_report(8);
        if (final_interrupt == USB_RC_OK) {
            any_success = 1;
            goto done;
        }
    }

    if (mode == SEDA_RELAY_MODE_CTRL_OUT_IF_9) {
        fill_seda_report(command, 4);
        final_control = send_control_report(2, USB_RECIP_INTERFACE, 9);
        any_success = final_control == USB_RC_OK;
        goto done;
    }

    if (mode == SEDA_RELAY_MODE_CTRL_FEAT_IF_9) {
        fill_seda_report(command, 4);
        final_control = send_control_report(3, USB_RECIP_INTERFACE, 9);
        any_success = final_control == USB_RC_OK;
        goto done;
    }

    if (mode == SEDA_RELAY_MODE_CTRL_OUT_DEV_9) {
        fill_seda_report(command, 4);
        final_control = send_control_report(2, USB_RECIP_DEVICE, 9);
        any_success = final_control == USB_RC_OK;
        goto done;
    }

    if (mode == SEDA_RELAY_MODE_INT_9) {
        fill_seda_report(command, 4);
        final_interrupt = send_interrupt_report(9);
        any_success = final_interrupt == USB_RC_OK;
        goto done;
    }

    if (mode == SEDA_RELAY_MODE_INT_8) {
        fill_seda_report(command, 4);
        final_interrupt = send_interrupt_report(8);
        any_success = final_interrupt == USB_RC_OK;
        goto done;
    }

    if (mode == SEDA_RELAY_MODE_INT_65) {
        fill_seda_report(command, 4);
        final_interrupt = send_interrupt_report(65);
        any_success = final_interrupt == USB_RC_OK;
        goto done;
    }

    if (mode == SEDA_RELAY_MODE_CTRL_OUT_IF_65) {
        fill_seda_report(command, 4);
        final_control = send_control_report(2, USB_RECIP_INTERFACE, 65);
        any_success = final_control == USB_RC_OK;
        goto done;
    }

    if (mode == SEDA_RELAY_MODE_CTRL_OUT_IF_9_I5) {
        fill_seda_report(command, 5);
        final_control = send_control_report(2, USB_RECIP_INTERFACE, 9);
        any_success = final_control == USB_RC_OK;
        goto done;
    }

    if (mode == SEDA_RELAY_MODE_INT_PAYLOAD8) {
        fill_seda_payload_report(command);
        final_interrupt = send_interrupt_report(8);
        any_success = final_interrupt == USB_RC_OK;
        goto done;
    }

    if (mode == SEDA_RELAY_MODE_CTRL_OUT_PAYLOAD8) {
        fill_seda_payload_report(command);
        final_control = send_control_report(2, USB_RECIP_INTERFACE, 8);
        any_success = final_control == USB_RC_OK;
        goto done;
    }

    if (mode == SEDA_RELAY_MODE_CTRL_FEAT_PAYLOAD8) {
        fill_seda_payload_report(command);
        final_control = send_control_report(3, USB_RECIP_INTERFACE, 8);
        any_success = final_control == USB_RC_OK;
        goto done;
    }

    for (ci = 0; ci < 2; ci++) {
        fill_seda_report(command, command_indexes[ci]);

        for (li = 0; li < 3; li++) {
            for (ti = 0; ti < 2; ti++) {
                for (ri = 0; ri < 2; ri++) {
                    result = send_control_report(report_types[ti], recipients[ri], report_lengths[li]);
                    final_control = result;

                    if (result == USB_RC_OK) {
                        any_success = 1;
                        goto done;
                    }
                }
            }

            result = send_interrupt_report(report_lengths[li]);
            final_interrupt = result;
            if (result == USB_RC_OK) {
                any_success = 1;
            }
        }
    }

done:
    SignalSema(lock_sema);

    if (control_result) {
        *control_result = final_control;
    }

    if (interrupt_result) {
        *interrupt_result = final_interrupt;
    }

    if (any_success || final_control == USB_RC_OK || final_interrupt == USB_RC_OK) {
        return SEDA_RELAY_OK;
    }

    return SEDA_RELAY_ERR_USB;
}

static void *rpc_handler(int command, void *buffer, int size)
{
    SedaRelayRpcPacket *packet = (SedaRelayRpcPacket *)buffer;

    (void)size;

    switch (command) {
    case SEDA_RELAY_RPC_SEND_COMMAND:
        packet->result = seda_send_command(packet->command, packet->mode, &packet->control_result, &packet->interrupt_result);
        packet->status = relay_dev.status;
        break;

    case SEDA_RELAY_RPC_GET_STATUS:
        packet->result = SEDA_RELAY_OK;
        packet->status = relay_dev.status;
        packet->control_result = 0;
        packet->interrupt_result = 0;
        break;

    default:
        packet->result = SEDA_RELAY_ERR_BAD_COMMAND;
        packet->status = relay_dev.status;
        break;
    }

    return buffer;
}

static void rpc_thread(void *arg)
{
    int tid;

    (void)arg;

    sceSifInitRpc(0);
    tid = GetThreadId();
    sceSifSetRpcQueue(&rpc_queue, tid);
    sceSifRegisterRpc(&rpc_server, SEDA_RELAY_RPC_ID, rpc_handler, &rpc_buffer, NULL, NULL, &rpc_queue);
    sceSifRpcLoop(&rpc_queue);
}

int _start(int argc, char *argv[])
{
    iop_thread_t thread;
    iop_sema_t sema;
    int th;

    parse_module_args(argc, argv);
    printf("SEDA RELAY: target VID=%04x PID=%04x\n", target_vid, target_pid);

    memset(&relay_dev, 0, sizeof(relay_dev));
    relay_dev.control_ep = -1;
    relay_dev.interrupt_out_ep = -1;
    relay_dev.status = SEDA_RELAY_STATUS_DISCONNECTED;

    sema.attr = 0;
    sema.option = 0;
    sema.initial = 1;
    sema.max = 1;
    lock_sema = CreateSema(&sema);

    sema.initial = 0;
    sema.max = 1;
    done_sema = CreateSema(&sema);

    if (lock_sema < 0 || done_sema < 0) {
        return MODULE_NO_RESIDENT_END;
    }

    if (sceUsbdRegisterLdd(&seda_driver) < 0) {
        printf("SEDA RELAY: sceUsbdRegisterLdd failed\n");
        return MODULE_NO_RESIDENT_END;
    }

    thread.attr = TH_C;
    thread.thread = rpc_thread;
    thread.priority = 40;
    thread.stacksize = 0x1000;
    thread.option = 0;

    th = CreateThread(&thread);
    if (th <= 0) {
        return MODULE_NO_RESIDENT_END;
    }

    StartThread(th, NULL);
    printf("SEDA RELAY: IRX loaded\n");

    return MODULE_RESIDENT_END;
}
