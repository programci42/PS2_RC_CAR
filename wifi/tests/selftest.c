/* SPDX-License-Identifier: MIT */
#include "rtl_core.h"
#include "selftest.h"
#include <string.h>
typedef struct { int result,calls; RtlRequest request; unsigned char bytes[4]; } FakeUsb;
static int fake_control(void *context,const RtlRequest *req,uint8_t *data) {
    FakeUsb *f=(FakeUsb*)context;f->calls++;f->request=*req;
    if(f->result>=0) memcpy(data,f->bytes,req->length);
    return f->result;
}
static void check(RtlTestReport *r,int condition,int line) {
    r->checks++;if(!condition) {r->failures++;if(!r->first_failure)r->first_failure=line;}
}
#define CHECK(x) check(&r,(x),__LINE__)
RtlTestReport rtl_selftest(void) {
    RtlTestReport r={0};RtlEndpoints ep;
    unsigned char config[]={9,2,39,0,1,1,0,0x80,250,9,4,0,0,3,0xff,0xff,0xff,0,
        7,5,0x81,2,64,0,0,7,5,0x02,2,64,0,0,7,5,0x03,2,64,0,0};
    unsigned char bad[sizeof(config)];
    unsigned int i,w;
    FakeUsb f;RtlIo io={fake_control,&f};uint32_t value;
    CHECK(rtl_matches(0x0bda,0x8178));CHECK(!rtl_matches(0x0bda,0x8176));CHECK(!rtl_matches(0x178e,0x07d9));
    CHECK(rtl_parse_config(config,sizeof(config),&ep)==0);
    CHECK(ep.configuration==1&&ep.interface_no==0&&ep.power_ma==500&&ep.bulk_in==0x81&&ep.bulk_in_packet==64&&ep.out_count==2&&ep.bulk_out[0]==2&&ep.bulk_out[1]==3);
    for(i=0;i<sizeof(config);i++)CHECK(rtl_parse_config(config,i,&ep)<0);
    memcpy(bad,config,sizeof(bad));bad[22]=0;bad[23]=2;bad[29]=0;bad[30]=2;bad[36]=0;bad[37]=2;
    CHECK(rtl_parse_config(bad,sizeof(bad),&ep)==0&&ep.bulk_in_packet==512&&ep.bulk_out_packet[1]==512);
    memcpy(bad,config,sizeof(bad));bad[18]=0;CHECK(rtl_parse_config(bad,sizeof(bad),&ep)==RTL_E_DESCRIPTOR);
    memcpy(bad,config,sizeof(bad));bad[18]=255;CHECK(rtl_parse_config(bad,sizeof(bad),&ep)==RTL_E_DESCRIPTOR);
    memcpy(bad,config,sizeof(bad));bad[27]=0x81;CHECK(rtl_parse_config(bad,sizeof(bad),&ep)==RTL_E_DESCRIPTOR);
    memcpy(bad,config,sizeof(bad));bad[20]=0x80;CHECK(rtl_parse_config(bad,sizeof(bad),&ep)==RTL_E_DESCRIPTOR);
    memcpy(bad,config,sizeof(bad));bad[14]=3;CHECK(rtl_parse_config(bad,sizeof(bad),&ep)==RTL_E_INTERFACE);
    memcpy(bad,config,sizeof(bad));bad[12]=1;CHECK(rtl_parse_config(bad,sizeof(bad),&ep)==RTL_E_INTERFACE);
    memcpy(bad,config,sizeof(bad));bad[13]=2;CHECK(rtl_parse_config(bad,sizeof(bad),&ep)==RTL_E_DESCRIPTOR);
    memcpy(bad,config,sizeof(bad));bad[22]=0;CHECK(rtl_parse_config(bad,sizeof(bad),&ep)==RTL_E_DESCRIPTOR);
    CHECK(rtl_parse_config(0,sizeof(config),&ep)==RTL_E_DESCRIPTOR);CHECK(rtl_parse_config(config,sizeof(config),0)==RTL_E_ARGUMENT);
    for(w=1;w<=4;w*=2) {
        memset(&f,0,sizeof(f));f.result=w;f.bytes[0]=0x12;f.bytes[1]=0x34;f.bytes[2]=0x56;f.bytes[3]=0x78;
        value=0xffffffff;CHECK(rtl_read_register(&io,0xf0,w,&value)==0);
        CHECK(value==(w==1?0x12u:w==2?0x3412u:0x78563412u));
        CHECK(f.calls==1&&f.request.type==0xc0&&f.request.request==5&&f.request.value==0xf0&&f.request.request_index==0&&f.request.length==w);
        f.result=w-1;value=0xaabbccdd;CHECK(rtl_read_register(&io,0xf0,w,&value)==RTL_E_SHORT&&value==0xaabbccdd);
    }
    for(i=1;i<=8;i++) {f.result=-(int)i;value=0x1234;CHECK(rtl_read_register(&io,0x80,1,&value)==-(int)i&&value==0x1234);}
    f.calls=0;CHECK(rtl_read_register(&io,65536,4,&value)==RTL_E_ARGUMENT&&f.calls==0);
    CHECK(rtl_read_register(&io,65535,2,&value)==RTL_E_ARGUMENT&&f.calls==0);
    CHECK(rtl_read_register(&io,0,3,&value)==RTL_E_ARGUMENT&&f.calls==0);
    CHECK(rtl_read_register(0,0,1,&value)==RTL_E_ARGUMENT);CHECK(rtl_read_register(&io,0,1,0)==RTL_E_ARGUMENT);
    /* Deterministic malformed-descriptor stress: no unbounded walks or OOB reads. */
    for(i=0;i<4096;i++) {
        memcpy(bad,config,sizeof(bad));bad[i%sizeof(bad)]=(unsigned char)(i*29);
        CHECK(rtl_parse_config(bad,i%sizeof(bad),&ep)<0);
    }
    return r;
}
#ifdef RTL_HOST_TEST
#include <stdio.h>
int main(void) {RtlTestReport r=rtl_selftest();printf("checks=%d failures=%d first_failure_line=%d\n",r.checks,r.failures,r.first_failure);return r.failures?1:0;}
#endif
