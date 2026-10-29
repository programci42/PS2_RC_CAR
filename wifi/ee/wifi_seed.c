/* SPDX-License-Identifier: MIT. Native MC RPC, USB bootstrap only if absent. */
#define NEWLIB_PORT_AWARE
#ifdef RTL_SEED_HOST_TEST
#include "seed_ps2_stub.h"
#else
#include <tamtypes.h>
#include <kernel.h>
#include <delaythread.h>
#include <loadfile.h>
#include <libmc.h>
#include <fileXio_rpc.h>
#include <io_common.h>
#endif
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "rtl_nonce.h"
#include "wifi_seed.h"
extern unsigned char iomanx_irx[],filexio_irx[],bdm_irx[],fatfs_irx[],usbmass_irx[];
extern unsigned int size_iomanx_irx,size_filexio_irx,size_bdm_irx,size_fatfs_irx,size_usbmass_irx;
static int ready,mc_fault,usb_ready,usb_attempted;
static int card_type,card_free,card_format;
static int card_seen_formatted;
static unsigned char mc_buffer[128] __attribute__((aligned(64)));
static unsigned char usb_buffer[128] __attribute__((aligned(64)));
static char launch_seed[256],source_path[256];
static char mc_directory[256]="/RTL_WIFI",mc_seed_path[256]="/RTL_WIFI/SEED.DAT";
static char mc_bootstrap[256],mc_known_path[256];
static char known_source[256];
int wifi_seed_known_path(char*out,unsigned cap){if(!known_source[0]||strlen(known_source)>=cap)return -1;strcpy(out,known_source);return 0;}
static int launch_on_mc,mc_port;
static void wipe(void*p,unsigned n){volatile unsigned char*v=p;while(n--)*v++=0;}
static int finish(int issued){
    int i,cmd,r,done;if(mc_fault)return RTL_NONCE_IO_BUSY;
    if(issued<0)return RTL_NONCE_READ;
    for(i=0;i<5000;i++){
        done=mcSync(MC_NOWAIT,&cmd,&r);
        if(done==1)return r;
        if(done<0)return RTL_NONCE_READ;
        DelayThread(1000);
    }
    mc_fault=1;return RTL_NONCE_IO_BUSY;
}
static int card(void){
    int r;
    if(!ready)return RTL_NONCE_READ;
    if(mc_fault)return RTL_NONCE_IO_BUSY;
    card_type=card_free=card_format=0;
    r=finish(mcGetInfo(mc_port,0,&card_type,&card_free,&card_format));
    if(r==RTL_NONCE_IO_BUSY)return r;
    if(card_type!=MC_TYPE_PS2||r<sceMcResNoFormat){card_seen_formatted=0;return RTL_NONCE_NO_CARD;}
    if(r==sceMcResChangedCard||card_format)card_seen_formatted=1;
    if(r==sceMcResNoFormat){card_seen_formatted=0;return RTL_NONCE_UNFORMATTED;}
    if(!card_seen_formatted)return RTL_NONCE_UNFORMATTED;
    return 0;
}
static int close_mc(int fd,int result){
    int r;if(mc_fault)return RTL_NONCE_IO_BUSY;
    r=finish(mcClose(fd));return result?result:(r<0?RTL_NONCE_WRITE:0);
}
static int mc_read_seed(void *ctx,uint8_t out[64]){
    int r,fd;const char *path=ctx?(const char*)ctx:mc_seed_path;r=card();if(r)return r;
    fd=finish(mcOpen(mc_port,0,path,FIO_O_RDONLY));
    if(fd<0)return fd==sceMcResNoEntry?RTL_NONCE_MISSING:(fd==RTL_NONCE_IO_BUSY?fd:RTL_NONCE_READ);
    memset(mc_buffer,0,sizeof(mc_buffer));SyncDCache(mc_buffer,mc_buffer+sizeof(mc_buffer));
    r=finish(mcRead(fd,UNCACHED_SEG(mc_buffer),65));
    if(r>=0&&r!=64)r=RTL_NONCE_INVALID;else if(r==64){memcpy(out,UNCACHED_SEG(mc_buffer),64);r=0;}
    else if(r!=RTL_NONCE_IO_BUSY)r=RTL_NONCE_READ;
    r=close_mc(fd,r);if(!mc_fault)wipe(mc_buffer,sizeof(mc_buffer));return r;
}
static int write_fd(int fd,const uint8_t data[64]){
    int r;memcpy(mc_buffer,data,64);r=finish(mcWrite(fd,mc_buffer,64));
    if(r==64){r=finish(mcFlush(fd));if(r<0&&r!=RTL_NONCE_IO_BUSY)r=RTL_NONCE_WRITE;}
    else if(r!=RTL_NONCE_IO_BUSY)r=RTL_NONCE_WRITE;
    r=close_mc(fd,r);if(!mc_fault)wipe(mc_buffer,sizeof(mc_buffer));return r;
}
static int mc_write_seed(void *ctx,const uint8_t data[64]){
    int fd,r;const char *path=ctx?(const char*)ctx:mc_seed_path;r=card();if(r)return r;
    fd=finish(mcOpen(mc_port,0,path,FIO_O_RDWR));
    if(fd<0)return fd==RTL_NONCE_IO_BUSY?fd:RTL_NONCE_WRITE;
    /* An existing file must be exactly 64 bytes, and is never truncated. */
    r=finish(mcSeek(fd,0,SEEK_END));if(r!=64)return close_mc(fd,RTL_NONCE_INVALID);
    r=finish(mcSeek(fd,0,SEEK_SET));if(r!=0)return close_mc(fd,RTL_NONCE_WRITE);
    return write_fd(fd,data);
}
static int mc_create_seed(void *ctx,const uint8_t data[64]){
    uint8_t old[64];int r,fd;(void)ctx;r=mc_read_seed(0,old);wipe(old,64);
    if(r!=RTL_NONCE_MISSING)return r?r:RTL_NONCE_INVALID;
    /* Only this one application directory is created. No format/delete API. */
    r=finish(mcMkDir(mc_port,0,mc_directory));
    if(r==RTL_NONCE_IO_BUSY)return r;
    fd=finish(mcOpen(mc_port,0,mc_seed_path,FIO_O_RDWR|FIO_O_CREAT));
    if(fd<0)return fd==RTL_NONCE_IO_BUSY?fd:RTL_NONCE_WRITE;
    r=finish(mcSeek(fd,0,SEEK_END));if(r!=0)return close_mc(fd,RTL_NONCE_INVALID);
    return write_fd(fd,data);
}
static int usb_read_seed(void *ctx,uint8_t out[64]){
    int fd,n,r=0;(void)ctx;
    if(!usb_ready||!source_path[0])return RTL_NONCE_NO_SOURCE;
    fd=fileXioOpen(source_path,FIO_O_RDONLY,0);if(fd<0)return RTL_NONCE_NO_SOURCE;
    memset(usb_buffer,0,sizeof(usb_buffer));SyncDCache(usb_buffer,usb_buffer+sizeof(usb_buffer));
    n=fileXioRead(fd,usb_buffer,65);if(fileXioClose(fd)<0)r=RTL_NONCE_READ;
    if(n!=64)r=RTL_NONCE_INVALID;
    if(!r)memcpy(out,UNCACHED_SEG(usb_buffer),64);
    wipe(usb_buffer,sizeof(usb_buffer));return r;
}
static int usb_write_seed(void *ctx,const uint8_t data[64]){
    int fd,n,r=0;(void)ctx;
    fd=fileXioOpen(source_path,FIO_O_RDWR,0);if(fd<0)return RTL_NONCE_WRITE;
    n=fileXioWrite(fd,(void*)data,64);if(n!=64)r=RTL_NONCE_WRITE;
    if(fileXioClose(fd)<0)r=RTL_NONCE_WRITE;
    return r;
}
static int module(void *data,unsigned size){int result=0,id=SifExecModuleBuffer(data,size,0,0,&result);return id<0||result<0?-1:0;}
static void load_usb(void){
    if(usb_attempted)return;
    usb_attempted=1;
    if(!module(iomanx_irx,size_iomanx_irx)&&!module(filexio_irx,size_filexio_irx)&&
       !module(bdm_irx,size_bdm_irx)&&!module(fatfs_irx,size_fatfs_irx)&&!module(usbmass_irx,size_usbmass_irx))
        usb_ready=fileXioInit()>=0;
}
int wifi_storage_usb_init(void){load_usb();return usb_ready?0:-1;}
void wifi_seed_init(int mc_ready,int argc,char **argv){
    const char *slash;size_t len;ready=mc_ready;
    launch_on_mc=0;mc_port=0;mc_bootstrap[0]=mc_known_path[0]=0;
    known_source[0]=0;
    snprintf(mc_directory,sizeof(mc_directory),"/RTL_WIFI");snprintf(mc_seed_path,sizeof(mc_seed_path),"/RTL_WIFI/SEED.DAT");
    if(argc&&argv&&argv[0]&&(slash=strrchr(argv[0],'/'))){
        len=(size_t)(slash-argv[0])+1;
        if(len+sizeof("Wifi.seed")<sizeof(launch_seed)){memcpy(launch_seed,argv[0],len);memcpy(launch_seed+len,"Wifi.seed",sizeof("Wifi.seed"));}
        if((!strncmp(argv[0],"mc0:",4)||!strncmp(argv[0],"mc1:",4))&&slash>argv[0]+4){
            size_t dirlen=(size_t)(slash-(argv[0]+4));
            if(dirlen+sizeof("/RTL_WIFI/SEED.DAT")<sizeof(mc_seed_path)){
                launch_on_mc=1;mc_port=argv[0][2]-'0';
                snprintf(mc_directory,sizeof(mc_directory),"%.*s/RTL_WIFI",(int)dirlen,argv[0]+4);
                snprintf(mc_seed_path,sizeof(mc_seed_path),"%.*s/RTL_WIFI/SEED.DAT",(int)dirlen,argv[0]+4);
                snprintf(mc_bootstrap,sizeof(mc_bootstrap),"%.*s/Wifi.seed",(int)dirlen,argv[0]+4);
                snprintf(mc_known_path,sizeof(mc_known_path),"%.*s/bilinenaglar.csv",(int)dirlen,argv[0]+4);
            }
        }
    }
}
int wifi_seed_nonce(const char *unused,uint8_t nonce[32]){
    RtlNonceIo target={0,mc_read_seed,mc_write_seed,mc_create_seed};
    RtlNonceIo source={0,usb_read_seed,usb_write_seed,0};
    uint8_t existing[64];int r,i,fd,attempt;static const char *const defaults[]={"mass:/Wifi.seed","mass:/Release/Wifi.seed","mass0:/Wifi.seed","mass0:/Release/Wifi.seed"};
    (void)unused;r=mc_read_seed(0,existing);wipe(existing,64);
    if(!r)return rtl_nonce_next_io(&target,nonce);
    if(r!=RTL_NONCE_MISSING){memset(nonce,0,32);return r;}
    if(launch_on_mc){RtlNonceIo local_source={mc_bootstrap,mc_read_seed,mc_write_seed,0};return rtl_nonce_setup(&target,&local_source,nonce);}
    load_usb();
    source_path[0]=0;
    if(usb_ready)for(attempt=0;attempt<50&&!source_path[0];attempt++){
        for(i=-1;i<4;i++){
            const char*p=i<0?launch_seed:defaults[i];if(!p[0])continue;
            fd=fileXioOpen(p,FIO_O_RDONLY,0);
            if(fd>=0){fileXioClose(fd);snprintf(source_path,sizeof(source_path),"%s",p);break;}
        }
        if(!source_path[0])DelayThread(100000); /* USB mount is asynchronous. */
    }
    return rtl_nonce_setup(&target,&source,nonce);
}
int wifi_seed_read_known(uint8_t *out,unsigned capacity){
    static uint8_t data[4160] __attribute__((aligned(64)));
    static const char *prefix[]={"mass:/","mass:/Release/","mass0:/","mass0:/Release/"};
    char path[256];int i,attempt,fd,n;const char*slash;
    if(!out||capacity<4096)return -1;
    if(launch_on_mc){
        snprintf(known_source,sizeof(known_source),"mc%d:%.247s",mc_port,mc_known_path);
        int result=card();if(result)return -1;
        fd=finish(mcOpen(mc_port,0,mc_known_path,FIO_O_RDONLY));if(fd<0)return fd==sceMcResNoEntry?0:-1;
        memset(data,0,sizeof(data));SyncDCache(data,data+sizeof(data));
        /* libmc copies an unaligned file tail in its EE callback. Give both
         * the IOP DMA and that callback the same uncached destination. */
        n=finish(mcRead(fd,UNCACHED_SEG(data),4097));result=close_mc(fd,0);
        if(n<0||n>4096||result){if(!mc_fault)wipe(data,sizeof(data));return -1;}
        memcpy(out,UNCACHED_SEG(data),n);wipe(data,sizeof(data));return n;
    }
    load_usb();if(!usb_ready)return -1;
    for(attempt=0;attempt<20;attempt++){
        for(i=-1;i<4;i++){
            if(i<0){slash=strrchr(launch_seed,'/');if(!slash)continue;
                snprintf(path,sizeof(path),"%.*s/bilinenaglar.csv",(int)(slash-launch_seed),launch_seed);
            }else snprintf(path,sizeof(path),"%sbilinenaglar.csv",prefix[i]);
            fd=fileXioOpen(path,FIO_O_RDONLY,0);if(fd<0)continue;
            memset(data,0,sizeof(data));SyncDCache(data,data+sizeof(data));
            n=fileXioRead(fd,data,4097);fileXioClose(fd);
            if(n<0||n>4096){wipe(data,sizeof(data));return -1;}
            snprintf(known_source,sizeof(known_source),"%s",path);memcpy(out,UNCACHED_SEG(data),n);wipe(data,sizeof(data));return n;
        }
        DelayThread(100000);
    }
    return 0;
}
