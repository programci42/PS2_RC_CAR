/* Power and firmware sequence adapted from NetBSD netbsd-7 if_urtwn.c.
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2014 Kevin Lo <kevlo@FreeBSD.org>
 * Copyright (c) 2016 Nathanial Sloss <nathanialsloss@yahoo.com.au>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * PS2 port: all transfers checked, bounded polls, fail closed on unknown
 * silicon/image. Only volatile register/RAM writes; no EFUSE/EEPROM writes.
 */
#include "rtl_bringup.h"
#include <string.h>
typedef struct {RtlIo *io;RtlDelayUs delay;void *ctx;RtlBootReport *r;} Boot;
static unsigned int le16(const uint8_t *p){return p[0]|((unsigned int)p[1]<<8);}
int rtl_boot_write_allowed(unsigned int a,unsigned int n){
    if(!n||n>196||a>65535||a+n>65536)return 0;
    if(a>=0x1000&&a+n<=0x2000)return 1; /* paged firmware RAM */
    return (a==0x0000&&(n==1||n==2))||(a==0x0002&&n==2)||
        (a==0x0004&&n==2)||(a==0x0011&&n==1)||(a==0x001c&&n==1)||
        (a==0x0021&&n==1)||(a==0x0080&&(n==1||n==4))||
        ((a==0x0081||a==0x0082)&&n==1)||(a==0x0100&&n==2)||
        (a==0x01cf&&n==1)||(a==0x0600&&n==1)||(a==0xfe10&&n==1)||
        (a==0x01e0&&n==4)||(a==0x0200&&n==4)||(a==0x0214&&n==1)||
        ((a==0x0424||a==0x0425||a==0x045d||a==0x0114||a==0x0209||a==0x0104)&&n==1)||
        ((a==0x010c||a==0x0116)&&n==2);
}
int rtl_boot_firmware_valid(const uint8_t *fw,size_t n){
    if(!fw||n<=32||n>32+32768)return RTL_BOOT_BAD_FIRMWARE;
    /* The bundled NetBSD RTL92C image has a 0x88c1 header, USB category 6,
     * function 2 and an exact RAM code size. Reject other chip families. */
    if(le16(fw)!=0x88c1||fw[2]!=6||fw[3]!=2||le16(fw+12)!=n-32)
        return RTL_BOOT_BAD_FIRMWARE;
    return RTL_OK;
}
static int readreg(Boot *b,unsigned int a,unsigned int n,uint32_t *v){
    int e;b->r->last_address=a;b->r->transfers++;
    e=rtl_read_register(b->io,a,n,v);
    if(!e)b->r->last_value=*v;
    return e;
}
static int region(Boot *b,unsigned int a,const uint8_t *p,unsigned int n){
    RtlRequest q;uint8_t data[196];int e;
    if(!p||!rtl_boot_write_allowed(a,n))return RTL_E_ARGUMENT;
    q.type=0x40;q.request=5;q.value=(uint16_t)a;q.request_index=0;q.length=(uint16_t)n;
    memcpy(data,p,n);b->r->last_address=a;b->r->transfers++;
    e=b->io->control(b->io->context,&q,data);
    return e<0?e:((unsigned int)e==n?RTL_OK:RTL_E_SHORT);
}
static int writeval(Boot *b,unsigned int a,unsigned int n,uint32_t v){
    uint8_t p[4];unsigned int i;
    for(i=0;i<n;i++)p[i]=(uint8_t)(v>>(i*8));
    return region(b,a,p,n);
}
static int modify(Boot *b,unsigned int a,unsigned int n,uint32_t clear,uint32_t set){
    uint32_t v;int e=readreg(b,a,n,&v);if(e)return e;
    return writeval(b,a,n,(v&~clear)|set);
}
static int poll(Boot *b,unsigned int a,unsigned int n,uint32_t mask,int set,
                unsigned int tries,unsigned int delay){
    unsigned int i;uint32_t v;int e;
    for(i=0;i<tries;i++){
        b->r->polls++;e=readreg(b,a,n,&v);if(e)return e;
        if(!!(v&mask)==!!set)return RTL_OK;
        b->delay(b->ctx,delay);
    }
    return RTL_E_TIMEOUT;
}
#define DO(x) do{int error_=(x);if(error_)return error_;}while(0)
static int dma_init(Boot *b){
    unsigned int i,hq,nq,lq,count,pages,remainder;uint32_t v,map;
    /* 0..248 form the transmit free list, 249..255 the packet ring.
     * The final ring link must return to 249, not wrap to reserved page 1. */
    for(i=0;i<256;i++){
        unsigned int next=i==248?255:(i==255?249:i+1);
        DO(writeval(b,0x01e0,4,0x40000000u|(i<<8)|next));
        DO(poll(b,0x01e0,4,0xc0000000u,0,20,5));
    }
    DO(readreg(b,0xfe66,2,&v));
    hq=!!(v&15);nq=!!(v&0xf0);lq=!!(v&0xf00);count=hq+nq+lq;
    if(!count||(count==2&&!hq))return RTL_BOOT_UNSUPPORTED;
    pages=17/count;remainder=17%count;
    DO(writeval(b,0x0214,1,nq?pages:0));
    DO(writeval(b,0x0200,4,0x80e70000u|(hq?pages+remainder:0)|((lq?pages:0)<<8)));
    DO(writeval(b,0x0424,1,249));DO(writeval(b,0x0425,1,249));
    DO(writeval(b,0x045d,1,249));DO(writeval(b,0x0114,1,249));DO(writeval(b,0x0209,1,249));
    map=count==3?0xf5b0:(count==2?(nq?0xfaf0:0xf5f0):(hq?0xfff0:(nq?0xaaa0:0x5550)));
    DO(modify(b,0x010c,2,0xfff0,map));DO(writeval(b,0x0116,2,0x27ff));
    DO(writeval(b,0x0104,1,0x11));return RTL_OK;
}
static int run(Boot *b,const uint8_t *fw,size_t size){
    uint32_t v;unsigned int off,page,n,chunk;int e;
    b->r->stage=RTL_BOOT_IDENTIFY;
    DO(readreg(b,0x00f0,4,&v));b->r->sys_cfg=v;
    if(!(v&0x08000000u)||(v&0x00800000u))return RTL_BOOT_UNSUPPORTED;
    b->r->stage=RTL_BOOT_AUTOLOAD;
    DO(poll(b,0x0004,1,2,1,1000,5));
    b->r->stage=RTL_BOOT_POWER;
    DO(writeval(b,0x001c,1,0));DO(writeval(b,0x0011,1,0x2b));b->delay(b->ctx,5);
    DO(readreg(b,0x0021,1,&v));
    if(!(v&1)){
        DO(writeval(b,0x0021,1,v|1));b->delay(b->ctx,100);
        DO(modify(b,0x0000,1,1,0));
    }
    DO(modify(b,0x0004,2,0,0x0100));DO(poll(b,0x0004,2,0x0100,0,1000,100));
    DO(writeval(b,0x0004,2,0x0812));DO(modify(b,0x0000,2,0x0200,0));
    b->r->stage=RTL_BOOT_MAC;
    DO(modify(b,0x0600,1,0x40,0));DO(poll(b,0x0600,1,0x80,0,200,5));
    DO(modify(b,0x0100,2,0,0x02ff));DO(writeval(b,0xfe10,1,0x19));
    b->r->stage=RTL_BOOT_DMA;DO(dma_init(b));
    b->r->stage=RTL_BOOT_RESET;
    DO(readreg(b,0x0080,1,&v));
    if(v&0x80){
        /* A host OS may leave RAM firmware running. Clear its download
         * selection before resetting, so the 8051 does not resume old RAM. */
        DO(writeval(b,0x0080,1,0));
        DO(writeval(b,0x01cf,1,0x20));
        e=poll(b,0x0002,2,0x0400,0,100,50);
        if(e==RTL_E_TIMEOUT){DO(modify(b,0x0002,2,0x0400,0));}
        else if(e)return e;
    }
    DO(modify(b,0x0002,2,0,0x0400));
    DO(modify(b,0x0080,1,0,1));DO(modify(b,0x0082,1,8,0));
    DO(modify(b,0x0080,1,0,4));b->delay(b->ctx,50);
    b->r->stage=RTL_BOOT_DOWNLOAD;
    fw+=32;size-=32;
    for(page=0;size;page++){
        n=size>4096?4096:(unsigned int)size;
        DO(modify(b,0x0080,4,0x00070000,page<<16));
        for(off=0;off<n;off+=chunk){
            unsigned int left=n-off;chunk=left>196?196:(left>4?4:1);
            DO(region(b,0x1000+off,fw+off,chunk));b->r->uploaded_bytes+=chunk;
        }
        fw+=n;size-=n;
    }
    DO(modify(b,0x0080,1,1,0));DO(writeval(b,0x0081,1,0));
    b->r->stage=RTL_BOOT_CHECKSUM;
    DO(poll(b,0x0080,4,4,1,1000,5));
    DO(modify(b,0x0080,4,0x40,2));
    /* Restart the 8051 after announcing the new image. Required when
     * capturing a warm device from the Windows driver (also rtl8xxxu flow). */
    DO(modify(b,0x0002,2,0x0400,0));b->delay(b->ctx,50);
    DO(modify(b,0x0002,2,0,0x0400));
    b->r->stage=RTL_BOOT_FIRMWARE_READY;
    DO(poll(b,0x0080,4,0x40,1,1000,5));
    b->r->stage=RTL_BOOT_COMPLETE;
    return RTL_OK;
}
int rtl_boot_run(RtlIo *io,RtlDelayUs delay,void *ctx,const uint8_t *fw,size_t n,RtlBootReport *r){
    Boot b;int e;
    if(!r)return RTL_E_ARGUMENT;
    memset(r,0,sizeof(*r));r->magic=RTL_BOOT_MAGIC;r->result=RTL_E_ARGUMENT;
    if(!io||!io->control||!delay)return r->result;
    e=rtl_boot_firmware_valid(fw,n);if(e){r->result=e;return e;}
    r->firmware_version=le16(fw+4);r->firmware_bytes=(uint32_t)(n-32);
    b.io=io;b.delay=delay;b.ctx=ctx;b.r=r;
    r->result=run(&b,fw,n);return r->result;
}
