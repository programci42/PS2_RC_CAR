/* SPDX-License-Identifier: MIT. Persistent, domain-separated HMAC ratchet. */
#include "rtl_nonce.h"
#include "mbedtls/md.h"
#include "mbedtls/platform_util.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
static int valid(const uint8_t *seed){unsigned i,v=0;for(i=0;i<64;i++)v|=seed[i];return !!v;}
static int expand(const uint8_t seed[64],unsigned domain,uint8_t next[64],uint8_t *nonce){
    uint8_t input[33],digest[20];unsigned i;int r=0;
    memcpy(input,seed+32,32);
    for(i=0;i<4;i++){
        input[32]=(uint8_t)(domain+i);
        if(mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1),seed,32,input,33,digest)){r=RTL_NONCE_CRYPTO;break;}
        memcpy(next+i*16,digest,16);
    }
    if(!r&&nonce)for(i=0;i<2;i++){
        uint8_t label[2]={0x80,(uint8_t)i};
        if(mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1),seed,32,label,2,digest)){r=RTL_NONCE_CRYPTO;break;}
        memcpy(nonce+i*16,digest,16);
    }
    mbedtls_platform_zeroize(input,sizeof(input));mbedtls_platform_zeroize(digest,sizeof(digest));return r;
}
static int update(const RtlNonceIo *io,const uint8_t seed[64],uint8_t next[64]){
    uint8_t verify[64];int r=expand(seed,0,next,0);
    if(!r)r=io->write(io->context,next);
    if(!r){r=io->read(io->context,verify);if(r||memcmp(next,verify,64))r=RTL_NONCE_VERIFY;}
    mbedtls_platform_zeroize(verify,sizeof(verify));return r;
}
int rtl_nonce_next_io(const RtlNonceIo *io,uint8_t nonce[32]){
    uint8_t seed[64],next[64],discard[64];int r=RTL_NONCE_READ;
    if(!nonce)return r;
    memset(nonce,0,32);
    if(!io||!io->read||!io->write)return r;
    r=io->read(io->context,seed);
    if(!r&&!valid(seed))r=RTL_NONCE_INVALID;
    if(!r)r=update(io,seed,next);
    if(!r)r=expand(seed,0,discard,nonce);
    if(r)memset(nonce,0,32);
    mbedtls_platform_zeroize(seed,64);mbedtls_platform_zeroize(next,64);mbedtls_platform_zeroize(discard,64);return r;
}
int rtl_nonce_setup(const RtlNonceIo *target,const RtlNonceIo *source,uint8_t nonce[32]){
    uint8_t seed[64],next[64],child[64],verify[64];int r;
    if(!nonce)return RTL_NONCE_READ;
    memset(nonce,0,32);
    if(!target||!target->read||!target->write||!target->create)return RTL_NONCE_READ;
    r=target->read(target->context,verify);
    if(!r){mbedtls_platform_zeroize(verify,64);return rtl_nonce_next_io(target,nonce);}
    if(r!=RTL_NONCE_MISSING)return r;
    if(!source||!source->read||!source->write)return RTL_NONCE_NO_SOURCE;
    r=source->read(source->context,seed);if(r==RTL_NONCE_MISSING)r=RTL_NONCE_NO_SOURCE;
    if(!r&&!valid(seed))r=RTL_NONCE_INVALID;
    if(!r)r=update(source,seed,next);
    /* Child chain must differ from the USB ratchet: losing a memory card
     * must not resume an older point of the same sequence. */
    if(!r)r=expand(next,0x40,child,0);
    if(!r)r=target->create(target->context,child);
    if(!r){r=target->read(target->context,verify);if(r||memcmp(verify,child,64))r=RTL_NONCE_VERIFY;}
    if(!r)r=rtl_nonce_next_io(target,nonce);
    if(r)memset(nonce,0,32);
    mbedtls_platform_zeroize(seed,64);mbedtls_platform_zeroize(next,64);
    mbedtls_platform_zeroize(child,64);mbedtls_platform_zeroize(verify,64);return r;
}
static int file_read(void *ctx,uint8_t out[64]){
    FILE*f=fopen((const char*)ctx,"rb");size_t n;int extra,err;
    if(!f)return errno==ENOENT?RTL_NONCE_MISSING:RTL_NONCE_READ;
    n=fread(out,1,64,f);extra=fgetc(f);err=ferror(f);if(fclose(f))err=1;
    return err?RTL_NONCE_READ:(n!=64||extra!=EOF?RTL_NONCE_INVALID:0);
}
static int file_write(void *ctx,const uint8_t data[64]){
    FILE*f=fopen((const char*)ctx,"r+b");int r=0;if(!f)return RTL_NONCE_WRITE;
    if(fwrite(data,1,64,f)!=64||fflush(f))r=RTL_NONCE_WRITE;
    if(fclose(f))r=RTL_NONCE_WRITE;
    return r;
}
int rtl_nonce_next(const char *path,uint8_t nonce[32]){
    RtlNonceIo io={(void*)path,file_read,file_write,0};
    if(!path){if(nonce)memset(nonce,0,32);return RTL_NONCE_READ;}
    return rtl_nonce_next_io(&io,nonce);
}
const char *rtl_nonce_error(int r){switch(r){
    case RTL_NONCE_MISSING:return "Seed file not found.";
    case RTL_NONCE_READ:return "Cannot read seed file; check memory-card access.";
    case RTL_NONCE_INVALID:return "Existing seed file invalid; not overwritten.";
    case RTL_NONCE_WRITE:return "Cannot update seed; card may be full or unwritable.";
    case RTL_NONCE_VERIFY:return "Seed read-back verification failed.";
    case RTL_NONCE_NO_CARD:return "Insert a PS2 memory card in slot one.";
    case RTL_NONCE_UNFORMATTED:return "Memory card is unformatted; this app does not format cards.";
    case RTL_NONCE_NO_SOURCE:return "First setup: put your private Wifi.seed beside Wifi.elf.";
    case RTL_NONCE_IO_BUSY:return "Card operation timed out; reopen the application.";
    default:return "Cannot prepare secure seed.";
}}
