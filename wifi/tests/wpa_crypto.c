/* SPDX-License-Identifier: MIT -- public vectors / synthetic test keys only. */
#include "rtl_wpa_crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wpa_fixtures.h"
static unsigned int checks;
#define CHECK(x) do{checks++;if(!(x)){printf("WPA_CRYPTO_TEST FAIL line=%d\n",__LINE__);return 1;}}while(0)
static void hex(const char *p,uint8_t *out){while(*p){unsigned int b;sscanf(p,"%2x",&b);*out++=(uint8_t)b;p+=2;}}
static int zero(const uint8_t *p,size_t n){size_t i;for(i=0;i<n;i++)if(p[i])return 0;return 1;}
int main(void){
    uint8_t pmk[32],expected[64],ptk[64],reverse[64],key[16],nonce[13],aad[8],plain[23],cipher[23],tag[8],out[64],wrapped[24];
    uint8_t an[32],sn[32],ap[6],sta[6],eapol[107],mic[16];size_t n;unsigned int i,j;
    CHECK(rtl_wpa_pmk((const uint8_t*)"password",8,(const uint8_t*)"IEEE",4,pmk)==0);
    hex("f42c6fc52df0ebef9ebb4b90b38a5f902e83fe1b135a70e23aed762e9710a12e",expected);CHECK(!memcmp(pmk,expected,32));
    for(i=0;i<32;i++){an[i]=(uint8_t)i;sn[i]=(uint8_t)(128+i);}
    for(i=0;i<6;i++){ap[i]=(uint8_t)(i+1);sta[i]=(uint8_t)(i+16);}
    CHECK(rtl_wpa_ptk(pmk,ap,sta,an,sn,ptk)==0);CHECK(!memcmp(ptk,fixture_ptk,64));
    CHECK(rtl_wpa_ptk(pmk,sta,ap,sn,an,reverse)==0);CHECK(!memcmp(ptk,reverse,64));
    memcpy(eapol,fixture_eapol,sizeof(eapol));
    CHECK(rtl_wpa_eapol_mic(ptk,eapol,sizeof(eapol),mic)==0);CHECK(!memcmp(mic,fixture_mic,16));
    memcpy(eapol+81,mic,16);CHECK(rtl_wpa_eapol_verify(ptk,eapol,sizeof(eapol))==0);
    for(i=0;i<sizeof(eapol);i++){
        eapol[i]^=1;CHECK(rtl_wpa_eapol_verify(ptk,eapol,sizeof(eapol))!=0);eapol[i]^=1;
    }
    for(i=0;i<sizeof(eapol);i++)CHECK(rtl_wpa_eapol_verify(ptk,eapol,i)!=0);
    /* RFC 3394 section 4.1. */
    hex("000102030405060708090a0b0c0d0e0f",key);
    hex("1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5",wrapped);
    CHECK(rtl_wpa_unwrap(key,wrapped,24,out,sizeof(out),&n)==0&&n==16);
    hex("00112233445566778899aabbccddeeff",expected);CHECK(!memcmp(out,expected,16));
    for(i=0;i<24;i++){wrapped[i]^=1;memset(out,0xaa,16);n=999;CHECK(rtl_wpa_unwrap(key,wrapped,24,out,sizeof(out),&n)!=0&&n==0&&zero(out,16));wrapped[i]^=1;}
    /* RFC 3610 packet vector 1: 23 encrypted bytes + 8-byte tag. */
    for(i=0;i<16;i++)key[i]=(uint8_t)(0xc0+i);
    hex("00000003020100a0a1a2a3a4a5",nonce);for(i=0;i<8;i++)aad[i]=(uint8_t)i;
    for(i=0;i<23;i++)plain[i]=(uint8_t)(i+8);
    CHECK(rtl_wpa_ccm_encrypt(key,nonce,aad,8,plain,23,cipher,tag)==0);
    hex("588c979a61c663d2f066d0c2c0f989806d5f6b61dac384",expected);CHECK(!memcmp(cipher,expected,23));
    hex("17e8d12cfdf926e0",expected);CHECK(!memcmp(tag,expected,8));
    CHECK(rtl_wpa_ccm_decrypt(key,nonce,aad,8,cipher,23,tag,out)==0&&!memcmp(out,plain,23));
    for(i=0;i<23;i++){cipher[i]^=1;memset(out,0xaa,23);CHECK(rtl_wpa_ccm_decrypt(key,nonce,aad,8,cipher,23,tag,out)!=0&&zero(out,23));cipher[i]^=1;}
    for(i=0;i<8;i++){tag[i]^=1;CHECK(rtl_wpa_ccm_decrypt(key,nonce,aad,8,cipher,23,tag,out)!=0&&zero(out,23));tag[i]^=1;}
    for(i=0;i<8;i++){aad[i]^=1;CHECK(rtl_wpa_ccm_decrypt(key,nonce,aad,8,cipher,23,tag,out)!=0&&zero(out,23));aad[i]^=1;}
    for(i=0;i<13;i++){nonce[i]^=1;CHECK(rtl_wpa_ccm_decrypt(key,nonce,aad,8,cipher,23,tag,out)!=0&&zero(out,23));nonce[i]^=1;}
    for(i=0;i<8;i++)CHECK(rtl_wpa_pmk((const uint8_t*)"password",i,(const uint8_t*)"IEEE",4,pmk)!=0&&zero(pmk,32));
    for(j=0;j<2;j++)CHECK(rtl_wpa_pmk((const uint8_t*)"password",8,(const uint8_t*)"IEEE",j?33:0,pmk)!=0&&zero(pmk,32));
    CHECK(rtl_wpa_unwrap(key,wrapped,24,out,15,&n)!=0&&n==0);
    CHECK(rtl_wpa_ccm_decrypt(key,nonce,aad,33,cipher,23,tag,out)!=0);
    printf("WPA_CRYPTO_TEST PASS checks=%u backend=MbedTLS-3.6.5 public-vectors-only\n",checks);
    printf("Primitives only: no association, EAPOL state machine, secure RNG or physical WiFi test.\n");return 0;
}
