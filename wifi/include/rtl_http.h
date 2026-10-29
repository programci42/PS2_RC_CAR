/* SPDX-License-Identifier: MIT. Bounded IPv4/TCP HTTP status service. */
#ifndef RTL_HTTP_H
#define RTL_HTTP_H
#include <stdint.h>
#include "wifi_files.h"
#define RTL_HTTP_CLIENTS 4
#define RTL_HTTP_REQUEST_MAX 4096
typedef struct {
    unsigned state,mss,window,request_size,response_size,response_sent,retries;
    uint8_t mac[6],ip[4];uint16_t port;
    uint32_t receive,next,unacked,last,active;
    unsigned flags,length,offset,ack_pending;
    char request[RTL_HTTP_REQUEST_MAX+1],response[29000];
    unsigned header_size;WfReply resource;
} RtlHttpClient;
typedef struct {
    uint8_t mac[6],ip[4];uint32_t seed,requests,responses;
    RtlHttpClient client[RTL_HTTP_CLIENTS];
    int (*control)(void *,const char *,uint32_t);void *control_context;
    unsigned relay_ready,owner,mask,left,right;
    WifiFiles *files;unsigned file_blocked;
    int (*api)(void*,const char*,const char*,const char*,const uint8_t*,unsigned,uint32_t,char*,unsigned);void *api_context;
    void (*error_sink)(void*,const char*);
    unsigned single_peer,peer_valid;uint8_t peer_ip[4];uint32_t peer_at;
    char last_request[72];
} RtlHttp;
void rtl_http_init(RtlHttp *,const uint8_t mac[6],const uint8_t ip[4],uint32_t seed);
void rtl_http_rx(RtlHttp *,const uint8_t *,unsigned,uint32_t now);
/* One Ethernet frame; only call when the radio TX queue is free. */
int rtl_http_tx(RtlHttp *,uint32_t now,uint8_t *,unsigned capacity);
void rtl_http_close_all(RtlHttp *);
#endif
