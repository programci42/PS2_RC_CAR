/* SPDX-License-Identifier: MIT
 * Register-model unit test. This does NOT execute the chip's 8051 firmware. */
#include "rtl_bringup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned int checks;
typedef struct {
    uint8_t reg[65536],ram[32768],llt[256];unsigned int calls,writes,ram_bytes,delays;
    unsigned int fail_at,short_at;int stall,old_fw;
} Model;
static void init(Model *m){
    memset(m,0,sizeof(*m));m->reg[0xf3]=8;m->reg[4]=2;
    m->reg[0]=1;m->reg[1]=2;m->reg[0x600]=0x40;
    m->reg[0xfe66]=0x32;
}
static int control(void *ctx,const RtlRequest *q,uint8_t *p){
    Model *m=ctx;unsigned int a=q->value,n=q->length,i,page;
    m->calls++;
    CHECK(q->request==5&&q->request_index==0&&n>0&&n<=196&&a+n<=65536);
    if(m->calls==m->fail_at)return RTL_E_IO;
    if(m->calls==m->short_at)return (int)n-1;
    if(q->type==0xc0){
        CHECK(n==1||n==2||n==4);memcpy(p,m->reg+a,n);return (int)n;
    }
    CHECK(q->type==0x40&&rtl_boot_write_allowed(a,n));m->writes++;
    if(a>=0x1000&&a<0x2000){
        page=m->reg[0x82]&7;CHECK((m->reg[0x80]&1)!=0);
        CHECK(page*4096+a-0x1000+n<=sizeof(m->ram));
        memcpy(m->ram+page*4096+a-0x1000,p,n);m->ram_bytes+=n;
    }else{
        uint8_t old=m->reg[0x80];
        for(i=0;i<n;i++)m->reg[a+i]=p[i];
        if(a==4&&(p[1]&1)&&m->stall!=2)m->reg[5]&=~1;
        if(a==0x600&&m->stall==3)m->reg[a]|=0x80;
        if(a==0x1cf&&m->stall!=6)m->reg[3]&=~4;
        if(a==0x1e0){m->llt[p[1]]=p[0];if(m->stall!=7)m->reg[0x1e3]&=0x3f;}
        if(a==0x80){
            /* Real MCUFWDL checksum report is write-one-to-clear. */
            m->reg[a]=(uint8_t)((p[0]&~4)|(old&4));
            if(p[0]&4)m->reg[a]&=~4;
            if((old&1)&&!(p[0]&1)&&m->ram_bytes&&m->stall!=4)m->reg[a]|=4;
            if((p[0]&2)&&m->ram_bytes&&m->stall!=5)m->reg[a]|=0x40;
        }
    }
    return (int)n;
}
static void delay(void *ctx,unsigned int us){Model *m=ctx;CHECK(us>0&&us<=100);m->delays++;}
static int run(Model *m,const uint8_t *fw,size_t n,RtlBootReport *r){RtlIo io={control,m};return rtl_boot_run(&io,delay,m,fw,n,r);}
int main(int argc,char **argv){
    FILE *f;long size;uint8_t *fw;Model *m;RtlBootReport r;unsigned int calls,i,mode;
    CHECK(argc==2);f=fopen(argv[1],"rb");CHECK(f!=NULL);CHECK(fseek(f,0,SEEK_END)==0);
    size=ftell(f);CHECK(size>32);rewind(f);fw=malloc((size_t)size);m=malloc(sizeof(*m));CHECK(fw&&m);
    CHECK(fread(fw,1,(size_t)size,f)==(size_t)size);fclose(f);
    CHECK(rtl_boot_firmware_valid(fw,(size_t)size)==0);
    init(m);CHECK(run(m,fw,(size_t)size,&r)==0);
    CHECK(r.magic==RTL_BOOT_MAGIC&&r.stage==RTL_BOOT_COMPLETE&&r.result==0);
    CHECK(r.firmware_version==49&&r.firmware_bytes==(unsigned int)size-32);
    CHECK(r.uploaded_bytes==r.firmware_bytes&&m->ram_bytes==r.firmware_bytes);
    CHECK(memcmp(m->ram,fw+32,(size_t)size-32)==0);CHECK(m->ram[4095]==fw[32+4095]);
    calls=m->calls;CHECK(calls==r.transfers);
    for(i=0;i<248;i++)CHECK(m->llt[i]==i+1);
    CHECK(m->llt[248]==255&&m->llt[249]==250&&m->llt[254]==255&&m->llt[255]==249);
    CHECK(m->reg[0x214]==8&&m->reg[0x200]==9&&m->reg[0x202]==231);
    /* Every individual USB call must stop the sequence on error/short count. */
    for(mode=0;mode<2;mode++)for(i=1;i<=calls;i++){
        init(m);if(mode)m->short_at=i;else m->fail_at=i;
        CHECK(run(m,fw,(size_t)size,&r)==(mode?RTL_E_SHORT:RTL_E_IO));
        CHECK(m->calls==i&&r.stage!=RTL_BOOT_COMPLETE);
    }
    /* Test-chip / RTL8188C identity must fail before any volatile write. */
    init(m);m->reg[0xf3]=0;CHECK(run(m,fw,(size_t)size,&r)==RTL_BOOT_UNSUPPORTED&&m->writes==0);
    init(m);m->reg[0xf2]=0x80;CHECK(run(m,fw,(size_t)size,&r)==RTL_BOOT_UNSUPPORTED&&m->writes==0);
    for(i=1;i<=5;i++){
        static const unsigned int stages[]={0,RTL_BOOT_AUTOLOAD,RTL_BOOT_POWER,RTL_BOOT_MAC,RTL_BOOT_CHECKSUM,RTL_BOOT_FIRMWARE_READY};
        init(m);m->stall=(int)i;if(i==1)m->reg[4]=0;
        CHECK(run(m,fw,(size_t)size,&r)==RTL_E_TIMEOUT);CHECK(r.stage==stages[i]);
        CHECK(r.polls<=1262&&r.transfers<=calls+1000);
    }
    init(m);m->reg[0x80]=0x80;m->reg[3]=4;CHECK(run(m,fw,(size_t)size,&r)==0);
    init(m);m->reg[0x80]=0x80;m->reg[3]=4;m->stall=6;CHECK(run(m,fw,(size_t)size,&r)==0);CHECK(r.polls>=100);
    init(m);m->stall=7;CHECK(run(m,fw,(size_t)size,&r)==RTL_E_TIMEOUT&&r.stage==RTL_BOOT_DMA&&r.uploaded_bytes==0);
    init(m);m->reg[0xfe66]=0;CHECK(run(m,fw,(size_t)size,&r)==RTL_BOOT_UNSUPPORTED&&r.stage==RTL_BOOT_DMA);
    init(m);m->reg[0xfe66]=0x10;m->reg[0xfe67]=1;CHECK(run(m,fw,(size_t)size,&r)==RTL_BOOT_UNSUPPORTED);
    for(i=1;i<=7;i++){
        init(m);m->reg[0xfe66]=(uint8_t)((i&1?2:0)|(i&2?0x30:0));m->reg[0xfe67]=(uint8_t)(i&4?4:0);
        if(i==6)continue;
        CHECK(run(m,fw,(size_t)size,&r)==0);
    }
    /* Image rejection is before even the first USB read. */
    for(i=0;i<=(unsigned int)size;i++)if(i!=(unsigned int)size){
        init(m);CHECK(run(m,fw,i,&r)==RTL_BOOT_BAD_FIRMWARE&&m->calls==0);
    }
    fw[0]^=1;init(m);CHECK(run(m,fw,(size_t)size,&r)==RTL_BOOT_BAD_FIRMWARE&&m->calls==0);fw[0]^=1;
    fw[2]^=1;init(m);CHECK(run(m,fw,(size_t)size,&r)==RTL_BOOT_BAD_FIRMWARE&&m->calls==0);fw[2]^=1;
    CHECK(!rtl_boot_write_allowed(0x30,4));CHECK(!rtl_boot_write_allowed(0xcf,1));
    CHECK(!rtl_boot_write_allowed(0x1fff,2));CHECK(!rtl_boot_write_allowed(0x1000,197));
    CHECK(!rtl_boot_write_allowed(0xffff,2));CHECK(!rtl_boot_write_allowed(0,0));
    CHECK(!rtl_boot_write_allowed(0x80,2));CHECK(!rtl_boot_write_allowed(0x100,4));
    printf("bringup checks=%u failures=0 normal_USB_transfers=%u firmware_payload=%ld\n",checks,calls,size-32);
    printf("Model only: power bits, paged RAM, W1C, timeouts and transfer faults. No physical WiFi or 8051 execution.\n");
    free(m);free(fw);return 0;
}
