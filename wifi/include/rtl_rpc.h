/* SPDX-License-Identifier: MIT */
#ifndef RTL_RPC_H
#define RTL_RPC_H
#define RTL_RPC_ID 0x52544c44
#define RTL_RPC_STATUS 1
#define RTL_RPC_READ 2
#define RTL_RPC_NET_DNS 3
#define RTL_RPC_BOOT 4
#define RTL_RPC_RECOVER 9
#define RTL_NET_MAGIC 0x554e4554
typedef struct {unsigned int magic;int result;unsigned int sequence,tx_bytes,rx_bytes,dns_id,dns_answers;} RtlNetReport;
#define RTL_RPC_MAGIC 0x8192c001
#define RTL_SEEN_MAX 8
enum { RTL_WAITING=0,RTL_CONFIGURING=1,RTL_READY=2,RTL_CONFIG_FAILED=3,RTL_DESC_FAILED=4 };
/* Only 32-bit fields; same layout on EE and IOP. No pointers cross RPC. */
typedef struct {
    unsigned int magic,version,generation,state;
    int usb_result,parse_result;
    unsigned int configuration,interface_no,power_ma,bulk_in,bulk_in_packet,out_count;
    unsigned int bulk_out[3],bulk_out_packet[3];
    unsigned int registers_attempted,register_value[4];int register_result[4];
    unsigned int seen_count,seen_vid[RTL_SEEN_MAX],seen_pid[RTL_SEEN_MAX];
} RtlReport;
#endif
