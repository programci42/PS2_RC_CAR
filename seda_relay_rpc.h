#ifndef SEDA_RELAY_RPC_H
#define SEDA_RELAY_RPC_H

#define SEDA_RELAY_RPC_ID 0x5344524c

#define SEDA_RELAY_RPC_SEND_COMMAND 1
#define SEDA_RELAY_RPC_GET_STATUS   2

#define SEDA_RELAY_MODE_AUTO             0
#define SEDA_RELAY_MODE_CTRL_OUT_IF_9    1
#define SEDA_RELAY_MODE_CTRL_FEAT_IF_9   2
#define SEDA_RELAY_MODE_CTRL_OUT_DEV_9   3
#define SEDA_RELAY_MODE_INT_9            4
#define SEDA_RELAY_MODE_INT_8            5
#define SEDA_RELAY_MODE_INT_65           6
#define SEDA_RELAY_MODE_CTRL_OUT_IF_65   7
#define SEDA_RELAY_MODE_CTRL_OUT_IF_9_I5 8
#define SEDA_RELAY_MODE_INT_PAYLOAD8     9
#define SEDA_RELAY_MODE_CTRL_OUT_PAYLOAD8 10
#define SEDA_RELAY_MODE_CTRL_FEAT_PAYLOAD8 11
#define SEDA_RELAY_MODE_COUNT            12

#define SEDA_RELAY_STATUS_DISCONNECTED 0
#define SEDA_RELAY_STATUS_CONNECTED    1
#define SEDA_RELAY_STATUS_READY        2

#define SEDA_RELAY_OK                0
#define SEDA_RELAY_ERR_NOT_READY    -1
#define SEDA_RELAY_ERR_USB          -2
#define SEDA_RELAY_ERR_BAD_COMMAND  -3

typedef struct {
    int result;
    int command;
    int mode;
    int status;
    int control_result;
    int interrupt_result;
} SedaRelayRpcPacket;

#endif
