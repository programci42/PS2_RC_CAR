/* Public packet fixtures, independent TCP checksum/handshake validation. */
#include "rtl_http.h"
#include "robot_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define H(x) do{checks++;if(!(x)){printf("HTTP_TEST FAIL line=%d %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned u16(const unsigned char*p){return p[0]*256u+p[1];}
static unsigned long u32(const unsigned char*p){return u16(p)*65536ul+u16(p+2);}
static void s16(unsigned char*p,unsigned v){p[0]=v/256;p[1]=v;}
static void s32(unsigned char*p,unsigned long v){s16(p,v>>16);s16(p+2,v);}
static unsigned ck(const unsigned char*p,unsigned n,unsigned v){unsigned i;for(i=0;i<n;i++)v+=(i&1)?p[i]:(unsigned)p[i]*256;while(v>>16)v=(v&65535)+(v>>16);return (~v)&65535;}
static const unsigned char mac[6]={2,1,2,3,4,5},remote[6]={2,6,7,8,9,10},ip[4]={192,0,2,25};
static unsigned packet(unsigned char*f,unsigned long seq,unsigned long ack,unsigned flags,const char*data,unsigned mss){
    unsigned n=data?strlen(data):0,th=mss?24:20,total=14+20+th+n;memset(f,0,total);
    memcpy(f,mac,6);memcpy(f+6,remote,6);s16(f+12,0x800);f[14]=0x45;s16(f+16,total-14);f[22]=64;f[23]=6;
    memcpy(f+26,"\xc0\x00\x02\x01",4);memcpy(f+30,ip,4);s16(f+24,ck(f+14,20,0));
    s16(f+34,45000);s16(f+36,80);s32(f+38,seq);s32(f+42,ack);f[46]=(th/4)<<4;f[47]=flags;s16(f+48,8192);
    if(mss){f[54]=2;f[55]=4;s16(f+56,mss);}if(n)memcpy(f+34+th,data,n);
    s16(f+50,ck(f+34,th+n,u16(f+26)+u16(f+28)+u16(f+30)+u16(f+32)+6+th+n));return total;
}
static void verify(const unsigned char*f,unsigned n){unsigned t=n-34;
    H(n>=54&&u16(f+36)==45000&&u16(f+34)==80);H(!memcmp(f,remote,6));H(!ck(f+14,20,0));
    H(!ck(f+34,t,u16(f+26)+u16(f+28)+u16(f+30)+u16(f+32)+6+t));
}
static void methods(void){
    const char*request[]={"HEAD / HTTP/1.1\r\n\r\n","GET /missing HTTP/1.1\r\n\r\n","POST / HTTP/1.1\r\n\r\n","GET /status HTTP/1.1\r\n\r\n","POST /drive/forward?s=100 HTTP/1.1\r\nx-ps2-control: 1\r\n\r\n","POST /drive/forward?s=100 HTTP/1.1\r\n\r\n","GET /status HTTP/1.1\r\n\r\n","GET / HTTP/1.1\r\n\r\n","GET /tools HTTP/1.1\r\n\r\n"};
    const char*status[]={"200 OK","404 Not Found","405 Method Not Allowed","200 OK","200 OK","400 Bad Request","423 Locked","200 OK","200 OK"};unsigned k;
    for(k=0;k<9;k++){static RtlHttp h;RobotControl robot;unsigned char f[1514],out[1514];static char text[32768];unsigned n,used=0,i;unsigned long server,seq;
        rtl_http_init(&h,mac,ip,1);n=packet(f,100,0,2,0,k==8?1460:512);rtl_http_rx(&h,f,n,0);H(rtl_http_tx(&h,0,out,sizeof(out))==58);H(u16(out+56)==1460);server=u32(out+38)+1;
        robot_init(&robot);robot.ready=1;h.control=robot_web;h.control_context=&robot;
        {static WifiFiles files;memset(&files,0,sizeof(files));h.files=&files;h.file_blocked=1;}
        if(k==6||k==7){h.single_peer=h.peer_valid=1;memcpy(h.peer_ip,"\xc0\x00\x02\x02",4);h.client[1].state=2;memcpy(h.client[1].ip,h.peer_ip,4);}
        n=packet(f,101,server,24,request[k],0);rtl_http_rx(&h,f,n,1);seq=101+strlen(request[k]);
        for(i=0;i<64;i++){int len=rtl_http_tx(&h,2+i*2,out,sizeof(out));H(len>=54);if(k==8&&i==0)H(len==1514);verify(out,len);memcpy(text+used,out+54,len-54);used+=len-54;
            server=u32(out+38)+len-54+!!(out[47]&1);n=packet(f,seq,server,16,0,0);rtl_http_rx(&h,f,n,3+i*2);if(h.responses)break;}
        H(h.responses==1);text[used]=0;H(strstr(text,status[k]));if(!k){char*end=strstr(text,"\r\n\r\n");H(end&&end[4]==0&&strstr(text,"Content-Length: "));}
        if(k==8)H(strstr(text,"C++ / MIPS assembly"));
        if(k==4)H(robot.web==ROBOT_FORWARD);else H(!robot.web);
        if(k==6){H(h.peer_ip[3]==2&&h.requests==0);}if(k==7){H(h.peer_ip[3]==1&&!h.client[1].state&&h.requests==1);}
    }
}
int http_test(void){static RtlHttp h;unsigned char f[1514],out[1514],saved[1514];unsigned n,i,bytes;unsigned long server,seq;int len;static char response[32768];
    rtl_http_init(&h,mac,ip,1234);n=packet(f,100,0,2,0,64);rtl_http_rx(&h,f,n,0);
    len=rtl_http_tx(&h,0,out,sizeof(out));H(len==58);verify(out,len);H(out[47]==18&&u32(out+42)==101);server=u32(out+38)+1;
    memcpy(saved,out,len);H(rtl_http_tx(&h,500,out,sizeof(out))==0);
    len=rtl_http_tx(&h,2000,out,sizeof(out));H(len==58&&!memcmp(saved,out,len));
    n=packet(f,101,server,16,0,0);rtl_http_rx(&h,f,n,2001);
    n=packet(f,101,server,24,"GET / HTTP/1.1\r\nHost: ps2\r\n",0);rtl_http_rx(&h,f,n,2002);seq=101+strlen("GET / HTTP/1.1\r\nHost: ps2\r\n");
    len=rtl_http_tx(&h,2002,out,sizeof(out));H(len==54&&u32(out+42)==seq);verify(out,len);
    /* Out-of-order suffix must not prematurely create a response. */
    n=packet(f,seq+10,server,24,"\r\n",0);rtl_http_rx(&h,f,n,2003);H(h.requests==0);
    n=packet(f,seq,server,24,"\r\n",0);rtl_http_rx(&h,f,n,2004);seq+=2;H(h.requests==1);
    bytes=0;for(i=0;i<512;i++){
        len=rtl_http_tx(&h,2100+i*2000,out,sizeof(out));H(len>=54);verify(out,len);n=len-54;H(n<=64);
        memcpy(response+bytes,out+54,n);bytes+=n;server=u32(out+38)+n+!!(out[47]&1);
        if(i==0){memcpy(saved,out,len);H(rtl_http_tx(&h,2101,out,sizeof(out))==0);H(rtl_http_tx(&h,3100,out,sizeof(out))==len&&!memcmp(saved,out,len));}
        n=packet(f,seq,server,16,0,0);rtl_http_rx(&h,f,n,3200+i*2000);if(h.responses)break;
    }
    H(h.responses==1);response[bytes]=0;H(strstr(response,"HTTP/1.0 200 OK\r\n")&&strstr(response,"192.0.2.25")&&strstr(response,"Connection: close"));
#ifndef _EE
    {FILE *page=fopen("reports/robot-control-preview.html","wb");char *body=strstr(response,"\r\n\r\n");H(page&&body);fwrite(body+4,1,strlen(body+4),page);fclose(page);}
#endif
    n=packet(f,seq,server,17,0,0);rtl_http_rx(&h,f,n,4000+i*2000);len=rtl_http_tx(&h,4000+i*2000,out,sizeof(out));H(len==54&&u32(out+42)==seq+1);verify(out,len);
    /* Invalid checksum, fragmentation, and wrong target never allocate. */
    rtl_http_init(&h,mac,ip,1234);n=packet(f,1,0,2,0,512);f[50]^=1;rtl_http_rx(&h,f,n,0);H(!rtl_http_tx(&h,0,out,sizeof(out)));
    for(i=0;i<RTL_HTTP_CLIENTS;i++){n=packet(f,1,0,2,0,512);f[6]+=i;rtl_http_rx(&h,f,n,0);}H(rtl_http_tx(&h,0,out,sizeof(out))>0);
    H(!rtl_http_tx(&h,31001,out,sizeof(out)));for(i=0;i<RTL_HTTP_CLIENTS;i++)H(!h.client[i].state);
    methods();printf("HTTP_TEST PASS checks=%u TCP-handshake-MSS-split-request-retransmit-close-checksum-bounded-HEAD-404-405\n",checks);return 0;
}
