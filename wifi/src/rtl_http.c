#include "wifi_i18n.h"
/* SPDX-License-Identifier: MIT. RFC 9293 TCP subset for a local HTTP/1.0
 * status page: bounded clients/headers, stop-and-wait TX, MSS, checksums,
 * duplicate/out-of-order ACKs, retransmission and idle expiry. No TLS. */
#include "rtl_http.h"
#include "wifi_version.h"
#include "robot_web_asset.h"
#include "relay_test_asset.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
static int control_header(const char*s){const char*p=s;const char*key="x-ps2-control: 1";unsigned i;
    while((p=strstr(p,"\r\n"))!=0){p+=2;if(!*p||!strncmp(p,"\r\n",2))break;
        for(i=0;key[i]&&p[i]&&tolower((unsigned char)p[i])==key[i];i++);
        if(!key[i]&&p[i]=='\r'&&p[i+1]=='\n')return 1;
    }return 0;
}
static unsigned be16(const uint8_t*p){return ((unsigned)p[0]<<8)|p[1];}
static uint32_t be32(const uint8_t*p){return ((uint32_t)be16(p)<<16)|be16(p+2);}
static void put16(uint8_t*p,unsigned v){p[0]=v>>8;p[1]=v;}
static void put32(uint8_t*p,uint32_t v){put16(p,v>>16);put16(p+2,v);}
static uint32_t sum(const uint8_t*p,unsigned n,uint32_t v){while(n>1){v+=be16(p);p+=2;n-=2;}if(n)v+=(unsigned)*p<<8;return v;}
static unsigned fold(uint32_t v){while(v>>16)v=(v&65535)+(v>>16);return (~v)&65535;}
static unsigned tcpcheck(const uint8_t*ip,const uint8_t*t,unsigned n){return fold(sum(t,n,sum(ip+12,8,n+6)));}
static void release_resource(RtlHttpClient*c){if(c->resource.stream&&c->resource.close)c->resource.close(c->resource.stream);memset(&c->resource,0,sizeof(c->resource));}
static void close_client(RtlHttpClient*c){release_resource(c);memset(c,0,sizeof(*c));}
void rtl_http_close_all(RtlHttp*h){unsigned i;for(i=0;i<RTL_HTTP_CLIENTS;i++)close_client(&h->client[i]);}
void rtl_http_init(RtlHttp*h,const uint8_t mac[6],const uint8_t ip[4],uint32_t seed){memset(h,0,sizeof(*h));memcpy(h->mac,mac,6);memcpy(h->ip,ip,4);h->seed=seed;}
static const char*field(const char*s,const char*key){const char*p=s;unsigned i;while((p=strstr(p,"\r\n"))!=0){p+=2;if(!strncmp(p,"\r\n",2))break;for(i=0;key[i]&&p[i]&&tolower((unsigned char)p[i])==key[i];i++);if(!key[i]){p+=i;while(*p==' ')p++;return p;}}return 0;}
static const char*reason(int code){switch(code){case 200:return "OK";case 206:return "Partial Content";case 400:return "Bad Request";case 403:return "Forbidden";case 404:return "Not Found";case 405:return "Method Not Allowed";case 409:return "Conflict";case 413:return "Content Too Large";case 416:return "Range Not Satisfiable";case 422:return "Unprocessable Content";case 428:return "Precondition Required";case 503:return "Service Unavailable";case 507:return "Insufficient Storage";default:return "Internal Server Error";}}
#include "ps2_tools_asset.h"
#include "wifi_i18n_asset.h"
static void page(RtlHttp*h,RtlHttpClient*c,uint32_t now){
    static char body[28000];const char *status="200 OK",*title="PS2 Slim Wi-Fi",*type="text/html; charset=utf-8";int len,head=0;
    {const char*s=strchr(c->request,' '),*e=s?strchr(s+1,' '):0;unsigned i;int root=!strncmp(c->request,"GET / HTTP/1.",13)||!strncmp(c->request,"HEAD / HTTP/1.",14);
     if(h->single_peer&&(!h->peer_valid||root||((uint32_t)(now-h->peer_at)>30000u))){
      if(!h->peer_valid||memcmp(h->peer_ip,c->ip,4)){for(i=0;i<RTL_HTTP_CLIENTS;i++)if(h->client+i!=c&&h->client[i].state&&memcmp(h->client[i].ip,c->ip,4))close_client(h->client+i);memcpy(h->peer_ip,c->ip,4);h->peer_valid=1;}
     }
     if(h->single_peer&&h->peer_valid&&memcmp(h->peer_ip,c->ip,4)){snprintf(body,sizeof(body),WIFI_TR("Another device took control. Open the home page to reconnect."));status="423 Locked";type="text/plain; charset=utf-8";goto response;}
     h->peer_at=now;h->requests++;
     if(s&&e){unsigned m=s-c->request,n=e-s-1;const char*q=memchr(s+1,'?',n);if(q)n=q-s-1;if(m>7)m=7;if(n>26)n=26;snprintf(h->last_request,sizeof(h->last_request),"HTTP %u.%u.%u.%u %.*s %.*s",c->ip[0],c->ip[1],c->ip[2],c->ip[3],m,c->request,n,s+1);}
    }
    if(!strncmp(c->request,"GET /i18n.js HTTP/1.",20)){snprintf(body,sizeof(body),"window.ps2Language=\"%s\";\n%s",wifi_language?"en":"tr",WIFI_I18N_ASSET);type="application/javascript; charset=utf-8";goto response;}
    if(!strncmp(c->request,"GET /relay-test.js HTTP/1.",26)){snprintf(body,sizeof(body),"%s",RELAY_TEST_ASSET);type="application/javascript; charset=utf-8";goto response;}
    if(!strncmp(c->request,"GET /tools HTTP/1.",18)){snprintf(body,sizeof(body),"%s",PS2_TOOLS_ASSET);goto response;}
    if(h->api){char method[8],target[256];const char*s=strchr(c->request,' '),*e=s?strchr(s+1,' '):0,*b=strstr(c->request,"\r\n\r\n");
        if(s&&e&&b&&s-c->request<8&&e-s-1<256){int code;memcpy(method,c->request,s-c->request);method[s-c->request]=0;memcpy(target,s+1,e-s-1);target[e-s-1]=0;body[0]=0;
            code=h->api(h->api_context,method,target,c->request,(const uint8_t*)b+4,c->request_size-(unsigned)(b+4-c->request),now,body,sizeof(body));
            if(code){char state[64];if(code>=400&&h->error_sink)h->error_sink(h->api_context,WIFI_TR("HTTP settings/system request rejected or not saved."));snprintf(state,sizeof(state),"%d %s",code,reason(code));type=code==200?"application/json; charset=utf-8":"text/plain; charset=utf-8";head=!strcmp(method,"HEAD");len=strlen(body);c->response_size=snprintf(c->response,sizeof(c->response),"HTTP/1.0 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n\r\n%s",state,type,len,head?"":body);c->response_sent=0;c->state=3;return;}
        }
    }
    if(h->files){char method[8],target[1024];const char*space=strchr(c->request,' '),*end=space?strchr(space+1,' '):0,*body_at=strstr(c->request,"\r\n\r\n");int handled=0;
        if(space&&end&&space-c->request<8&&end-space-1<1024&&body_at){WfReply reply;memcpy(method,c->request,space-c->request);method[space-c->request]=0;memcpy(target,space+1,end-space-1);target[end-space-1]=0;
            if(h->file_blocked&&(!strncmp(target,"/?",2)||!strncmp(target,"/files/",7))){snprintf(body,sizeof(body),WIFI_TR("Stop robot and release buttons before file operations."));status="409 Conflict";type="text/plain; charset=utf-8";goto response;}
            handled=wifi_files_request(h->files,method,target,c->request,(const uint8_t*)body_at+4,c->request_size-(unsigned)(body_at+4-c->request),now,&reply);
            if(handled>0){unsigned headers;head=!strcmp(method,"HEAD");
                if(reply.status>=500&&h->error_sink)h->error_sink(h->api_context,WIFI_TR("HTTP file I/O error."));
                headers=snprintf(c->response,sizeof(c->response),"HTTP/1.0 %d %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nAccept-Ranges: bytes\r\n%s\r\n",reply.status,reason(reply.status),reply.type,(unsigned long)reply.size,reply.extra);
                c->resource=reply;c->header_size=headers;c->response_size=headers+(head?0:reply.size);if(head)release_resource(c);c->response_sent=0;c->state=3;return;
            }
        }
        if(handled<0){snprintf(body,sizeof(body),WIFI_TR("Not enough memory for file service."));status="503 Service Unavailable";type="text/plain; charset=utf-8";goto response;}
    }
    if(!strncmp(c->request,"GET /status HTTP/1.",19)){
        snprintf(body,sizeof(body),"{\"ready\":%u,\"owner\":%u,\"mask\":%u,\"left\":%u,\"right\":%u}",h->relay_ready,h->owner,h->mask,h->left,h->right);type="application/json";goto response;
    }
    if(!strncmp(c->request,"POST /drive/",12)){
        char command[64];const char*end=strchr(c->request+12,' ');int result=-3;
        if(end&&end-c->request-12>0&&end-c->request-12<64&&control_header(c->request)){
            memcpy(command,c->request+12,end-c->request-12);command[end-c->request-12]=0;
            result=h->control?h->control(h->control_context,command,now):-1;
        }
        status=result==0?"200 OK":result==-1?"503 Service Unavailable":result==-2?"409 Conflict":"400 Bad Request";
        snprintf(body,sizeof(body),"%s",result==0?WIFI_TR("Command accepted."):result==-1?WIFI_TR("Relay board not ready."):result==-2?WIFI_TR("Gamepad has priority or command is stale."):WIFI_TR("Invalid control request."));type="text/plain; charset=utf-8";goto response;
    }
    if(!strncmp(c->request,"HEAD / HTTP/1.",14))head=1;
    else if(strncmp(c->request,"GET / HTTP/1.",13)){
        if(!strncmp(c->request,"GET ",4)||!strncmp(c->request,"HEAD ",5)){status="404 Not Found";title=WIFI_TR("Page not found");}
        else {status="405 Method Not Allowed";title=WIFI_TR("Only GET and HEAD are supported");}
    }
    snprintf(body,sizeof(body),"<!doctype html><html lang=\"en\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width\"><title>PS2 Slim Robot</title><body><h1>%s</h1><p>Wi-Fi connection active.</p><p>IP: %u.%u.%u.%u · Version: %s · HTTP requests: %lu</p>%s</body></html>",title,h->ip[0],h->ip[1],h->ip[2],h->ip[3],WIFI_VERSION,(unsigned long)h->requests,ROBOT_WEB_ASSET);
response:
    len=strlen(body);
    c->response_size=snprintf(c->response,sizeof(c->response),"HTTP/1.0 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n\r\n%s",status,type,len,head?"":body);
    c->response_sent=0;c->state=3;
}
void rtl_http_rx(RtlHttp*h,const uint8_t*f,unsigned n,uint32_t now){
    const uint8_t*ip,*t;unsigned ih,th,total,size,flags,i,mss=512;uint32_t seq,ack;RtlHttpClient*c=0,*free_slot=0;
    if(!h||!f||n<54||memcmp(f,h->mac,6)||f[6]&1||be16(f+12)!=0x800)return;
    ip=f+14;ih=(ip[0]&15)*4;total=be16(ip+2);
    if(ip[0]>>4!=4||ih<20||ih>60||total<ih+20||total>n-14||ip[9]!=6||!ip[8]||
       (be16(ip+6)&0xbfff)||fold(sum(ip,ih,0))||memcmp(ip+16,h->ip,4)||!ip[12]||ip[12]>=224||ip[12]==127)return;
    t=ip+ih;th=(t[12]>>4)*4;
    if(th<20||th>total-ih||be16(t+2)!=80||!be16(t)||tcpcheck(ip,t,total-ih))return;
    flags=t[13];size=total-ih-th;seq=be32(t+4);ack=be32(t+8);
    for(i=0;i<RTL_HTTP_CLIENTS;i++){
        RtlHttpClient*s=&h->client[i];
        if(s->state&&(uint32_t)(now-s->active)>(s->state==5?2000u:30000u))close_client(s);
        if(!s->state||(s->state==5&&!free_slot))free_slot=s;
        else if(s->port==be16(t)&&!memcmp(s->ip,ip+12,4)&&!memcmp(s->mac,f+6,6))c=s;
    }
    if(flags&2){
        if(flags&(1|4|16)||size)return;
        if(c){if(c->state==1&&seq+1==c->receive)c->last=now-1000;return;}
        if(!free_slot)return;
        for(i=20;i<th;){unsigned k=t[i],l;if(!k)break;if(k==1){i++;continue;}if(i+2>th)return;l=t[i+1];if(l<2||l>th-i)return;if(k==2){if(l!=4)return;mss=be16(t+i+2);if(mss<64)mss=64;if(mss>1460)mss=1460;}i+=l;}
        c=free_slot;close_client(c);c->state=1;c->mss=mss;c->window=be16(t+14);c->port=be16(t);memcpy(c->ip,ip+12,4);memcpy(c->mac,f+6,6);
        c->receive=seq+1;h->seed+=64007;c->unacked=h->seed+now*16;c->next=c->unacked+1;c->flags=0x12;c->active=now;c->last=now-1000;return;
    }
    if(!c)return;
    if(flags&4){if(seq==c->receive)close_client(c);return;}
    if(!(flags&16)||ack!=c->next||seq!=c->receive){
        /* No reassembly buffer: acknowledge the contiguous prefix. TCP
         * retransmission recovers out-of-order and overlapping segments. */
        if(c->state!=1)c->ack_pending=1;
        return;
    }
    c->active=now;c->window=be16(t+14);
    if(c->unacked!=c->next){
        c->unacked=ack;c->retries=0;
        if(c->state==1)c->state=2;
        else if(c->state==3){c->response_sent+=c->length;if(c->resource.progress)c->resource.progress(c->resource.stream,c->response_sent>c->header_size?c->response_sent-c->header_size:0);if(c->flags&1){c->state=4;h->responses++;release_resource(c);}}
        c->length=c->flags=0;
    }
    if(size&&c->state==2){
        if(size>RTL_HTTP_REQUEST_MAX-c->request_size){close_client(c);return;}
        memcpy(c->request+c->request_size,t+th,size);c->request_size+=size;c->request[c->request_size]=0;c->receive+=size;c->ack_pending=1;
        {const char*end=strstr(c->request,"\r\n\r\n");if(end){unsigned header_bytes=end+4-c->request,body_bytes=0;const char*length=field(c->request,"content-length:"),*transfer=field(c->request,"transfer-encoding:");
            if(transfer){close_client(c);return;}
            if(length){char*tail;unsigned long count=strtoul(length,&tail,10);if(tail==length||*tail!='\r'||count>RTL_HTTP_REQUEST_MAX-header_bytes){close_client(c);return;}body_bytes=count;}
            if(c->request_size>=header_bytes+body_bytes)page(h,c,now);
        }}
    }else if(size){c->ack_pending=1;return;}
    if(flags&1){c->receive++;c->ack_pending=1;if(c->state==2)c->state=3;else if(c->state==4)c->state=5;}
}
int rtl_http_tx(RtlHttp*h,uint32_t now,uint8_t*out,unsigned cap){
    unsigned i;for(i=0;i<RTL_HTTP_CLIENTS;i++){
        RtlHttpClient*c=&h->client[i];uint8_t*ip=out+14,*t=out+34;unsigned len=0,flags=16,th=20,total;uint32_t seq;
        if(!c->state)continue;
        if((uint32_t)(now-c->active)>(c->state==5?2000u:30000u)){close_client(c);continue;}
        if(c->unacked!=c->next){
            if((uint32_t)(now-c->last)<(1000u<<(c->retries>2?2:c->retries)))continue;
            if(c->retries++>=5){close_client(c);continue;}
            flags=c->flags;len=c->length;seq=c->unacked;
        }else if(c->state==3){
            if(!c->window)continue;
            len=c->response_size-c->response_sent;if(len>c->mss)len=c->mss;
            if(len>c->window)len=c->window;
            flags=0x18;if(c->response_sent+len==c->response_size)flags|=1;
            seq=c->next;c->length=len;c->offset=c->response_sent;c->flags=flags;c->next+=len+!!(flags&1);c->retries=0;
        }else if(c->ack_pending){seq=c->next;}
        else continue;
        if(flags&2)th=24;
        total=14+20+th+len;if(cap<total)return -1;
        memset(out,0,total);memcpy(out,c->mac,6);memcpy(out+6,h->mac,6);put16(out+12,0x800);
        ip[0]=0x45;put16(ip+2,total-14);ip[8]=64;ip[9]=6;memcpy(ip+12,h->ip,4);memcpy(ip+16,c->ip,4);put16(ip+10,fold(sum(ip,20,0)));
        put16(t,80);put16(t+2,c->port);put32(t+4,seq);put32(t+8,c->receive);t[12]=(th/4)<<4;t[13]=flags;put16(t+14,RTL_HTTP_REQUEST_MAX-c->request_size);
        if(flags&2){t[20]=2;t[21]=4;put16(t+22,1460);}
        if(len){if(c->resource.stream){unsigned copied=0;
                if(c->offset<c->header_size){copied=c->header_size-c->offset;if(copied>len)copied=len;memcpy(t+th,c->response+c->offset,copied);}
                if(copied<len&&c->resource.read(c->resource.stream,c->offset+copied-c->header_size,t+th+copied,len-copied)!=(int)(len-copied)){close_client(c);return 0;}
            }else memcpy(t+th,c->response+c->offset,len);}
        put16(t+16,tcpcheck(ip,t,th+len));c->last=now;c->ack_pending=0;return total;
    }return 0;
}
