/* SPDX-License-Identifier: MIT */
#include "rtl_wpa_crypto.h"
#include "mbedtls/md.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/ccm.h"
#include "mbedtls/nist_kw.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/constant_time.h"
#include <string.h>
#include <stdlib.h>
static unsigned int be16(const uint8_t *p){return ((unsigned int)p[0]<<8)|p[1];}
static int hmac(const uint8_t *key,size_t keylen,const uint8_t *data,size_t len,uint8_t out[20]){
    return mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1),key,keylen,data,len,out);
}
int rtl_wpa_pmk(const uint8_t *p,size_t pn,const uint8_t *s,size_t sn,uint8_t out[32]){
    int e;
    if(!out)return -1;
    memset(out,0,32);
    if(!p||!s||pn<8||pn>63||sn<1||sn>32)return -1;
    e=mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA1,p,pn,s,sn,4096,32,out);
    if(e)mbedtls_platform_zeroize(out,32);
    return e? -1:0;
}
int rtl_wpa_ptk(const uint8_t pmk[32],const uint8_t ap[6],const uint8_t sta[6],
    const uint8_t an[32],const uint8_t sn[32],uint8_t ptk[64]){
    uint8_t input[100],digest[20];unsigned int i,off=0,n;int e=0;
    if(!ptk)return -1;
    memset(ptk,0,64);
    if(!pmk||!ap||!sta||!an||!sn)return -1;
    memcpy(input,"Pairwise key expansion",23);
    if(memcmp(ap,sta,6)<0){memcpy(input+23,ap,6);memcpy(input+29,sta,6);}
    else{memcpy(input+23,sta,6);memcpy(input+29,ap,6);}
    if(memcmp(an,sn,32)<0){memcpy(input+35,an,32);memcpy(input+67,sn,32);}
    else{memcpy(input+35,sn,32);memcpy(input+67,an,32);}
    for(i=0;i<4;i++){
        input[99]=(uint8_t)i;e=hmac(pmk,32,input,sizeof(input),digest);if(e)break;
        n=64-off<20?64-off:20;memcpy(ptk+off,digest,n);off+=n;
    }
    mbedtls_platform_zeroize(input,sizeof(input));mbedtls_platform_zeroize(digest,sizeof(digest));
    if(e)mbedtls_platform_zeroize(ptk,64);
    return e? -1:0;
}
int rtl_wpa_eapol_mic(const uint8_t kck[16],const uint8_t *p,size_t n,uint8_t mic[16]){
    uint8_t *copy,digest[20];int e;
    if(!mic)return -1;
    memset(mic,0,16);
    if(!kck||!p||n<99||n>2048||(p[0]!=1&&p[0]!=2)||p[1]!=3||
       be16(p+2)!=n-4||p[4]!=2||(be16(p+5)&7)!=2||be16(p+97)!=n-99)return -1;
    copy=malloc(n);if(!copy)return -1;
    memcpy(copy,p,n);memset(copy+81,0,16);e=hmac(kck,16,copy,n,digest);
    if(!e)memcpy(mic,digest,16);
    mbedtls_platform_zeroize(digest,sizeof(digest));mbedtls_platform_zeroize(copy,n);free(copy);
    return e? -1:0;
}
int rtl_wpa_eapol_verify(const uint8_t kck[16],const uint8_t *p,size_t n){
    uint8_t mic[16];int e=rtl_wpa_eapol_mic(kck,p,n,mic);
    if(!e)e=mbedtls_ct_memcmp(mic,p+81,16);
    mbedtls_platform_zeroize(mic,sizeof(mic));return e? -1:0;
}
int rtl_wpa_unwrap(const uint8_t kek[16],const uint8_t *in,size_t n,uint8_t *out,size_t cap,size_t *outlen){
    mbedtls_nist_kw_context ctx;int e;
    if(outlen)*outlen=0;
    if(!kek||!in||!out||!outlen||n<24||n>2048||n%8||cap<n-8)return -1;
    mbedtls_nist_kw_init(&ctx);e=mbedtls_nist_kw_setkey(&ctx,MBEDTLS_CIPHER_ID_AES,kek,128,0);
    if(!e)e=mbedtls_nist_kw_unwrap(&ctx,MBEDTLS_KW_MODE_KW,in,n,out,outlen,cap);
    mbedtls_nist_kw_free(&ctx);
    if(e){mbedtls_platform_zeroize(out,n-8);*outlen=0;}
    return e? -1:0;
}
static int ccm(int decrypt,const uint8_t *key,const uint8_t *nonce,const uint8_t *aad,size_t an,
               const uint8_t *in,size_t n,uint8_t *out,uint8_t *tag){
    mbedtls_ccm_context ctx;int e;
    if(!key||!nonce||(!aad&&an)||!in||!out||!tag||n>2304||an>32)return -1;
    mbedtls_ccm_init(&ctx);e=mbedtls_ccm_setkey(&ctx,MBEDTLS_CIPHER_ID_AES,key,128);
    if(!e){
        if(decrypt)e=mbedtls_ccm_auth_decrypt(&ctx,n,nonce,13,aad,an,in,out,tag,8);
        else e=mbedtls_ccm_encrypt_and_tag(&ctx,n,nonce,13,aad,an,in,out,tag,8);
    }
    mbedtls_ccm_free(&ctx);
    if(e){mbedtls_platform_zeroize(out,n);if(!decrypt)mbedtls_platform_zeroize(tag,8);}
    return e? -1:0;
}
int rtl_wpa_ccm_encrypt(const uint8_t tk[16],const uint8_t nonce[13],const uint8_t *aad,size_t an,
    const uint8_t *plain,size_t n,uint8_t *cipher,uint8_t tag[8]){return ccm(0,tk,nonce,aad,an,plain,n,cipher,tag);}
int rtl_wpa_ccm_decrypt(const uint8_t tk[16],const uint8_t nonce[13],const uint8_t *aad,size_t an,
    const uint8_t *cipher,size_t n,const uint8_t tag[8],uint8_t *plain){
    uint8_t t[8];int e;if(!tag)return -1;memcpy(t,tag,8);
    e=ccm(1,tk,nonce,aad,an,cipher,n,plain,t);mbedtls_platform_zeroize(t,8);return e;
}
