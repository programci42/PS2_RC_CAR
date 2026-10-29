/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Host AP crypt: host-based TKIP encryption implementation for Host AP driver
 *
 * Copyright (c) 2003-2004, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 */


/* Portable checked group RX adaptation; no kernel structures. */
#include "rtl_tkip.h"
#include <string.h>
typedef uint16_t u16;
static const u16 Sbox[256] = {
	0xC6A5, 0xF884, 0xEE99, 0xF68D, 0xFF0D, 0xD6BD, 0xDEB1, 0x9154,
	0x6050, 0x0203, 0xCEA9, 0x567D, 0xE719, 0xB562, 0x4DE6, 0xEC9A,
	0x8F45, 0x1F9D, 0x8940, 0xFA87, 0xEF15, 0xB2EB, 0x8EC9, 0xFB0B,
	0x41EC, 0xB367, 0x5FFD, 0x45EA, 0x23BF, 0x53F7, 0xE496, 0x9B5B,
	0x75C2, 0xE11C, 0x3DAE, 0x4C6A, 0x6C5A, 0x7E41, 0xF502, 0x834F,
	0x685C, 0x51F4, 0xD134, 0xF908, 0xE293, 0xAB73, 0x6253, 0x2A3F,
	0x080C, 0x9552, 0x4665, 0x9D5E, 0x3028, 0x37A1, 0x0A0F, 0x2FB5,
	0x0E09, 0x2436, 0x1B9B, 0xDF3D, 0xCD26, 0x4E69, 0x7FCD, 0xEA9F,
	0x121B, 0x1D9E, 0x5874, 0x342E, 0x362D, 0xDCB2, 0xB4EE, 0x5BFB,
	0xA4F6, 0x764D, 0xB761, 0x7DCE, 0x527B, 0xDD3E, 0x5E71, 0x1397,
	0xA6F5, 0xB968, 0x0000, 0xC12C, 0x4060, 0xE31F, 0x79C8, 0xB6ED,
	0xD4BE, 0x8D46, 0x67D9, 0x724B, 0x94DE, 0x98D4, 0xB0E8, 0x854A,
	0xBB6B, 0xC52A, 0x4FE5, 0xED16, 0x86C5, 0x9AD7, 0x6655, 0x1194,
	0x8ACF, 0xE910, 0x0406, 0xFE81, 0xA0F0, 0x7844, 0x25BA, 0x4BE3,
	0xA2F3, 0x5DFE, 0x80C0, 0x058A, 0x3FAD, 0x21BC, 0x7048, 0xF104,
	0x63DF, 0x77C1, 0xAF75, 0x4263, 0x2030, 0xE51A, 0xFD0E, 0xBF6D,
	0x814C, 0x1814, 0x2635, 0xC32F, 0xBEE1, 0x35A2, 0x88CC, 0x2E39,
	0x9357, 0x55F2, 0xFC82, 0x7A47, 0xC8AC, 0xBAE7, 0x322B, 0xE695,
	0xC0A0, 0x1998, 0x9ED1, 0xA37F, 0x4466, 0x547E, 0x3BAB, 0x0B83,
	0x8CCA, 0xC729, 0x6BD3, 0x283C, 0xA779, 0xBCE2, 0x161D, 0xAD76,
	0xDB3B, 0x6456, 0x744E, 0x141E, 0x92DB, 0x0C0A, 0x486C, 0xB8E4,
	0x9F5D, 0xBD6E, 0x43EF, 0xC4A6, 0x39A8, 0x31A4, 0xD337, 0xF28B,
	0xD532, 0x8B43, 0x6E59, 0xDAB7, 0x018C, 0xB164, 0x9CD2, 0x49E0,
	0xD8B4, 0xACFA, 0xF307, 0xCF25, 0xCAAF, 0xF48E, 0x47E9, 0x1018,
	0x6FD5, 0xF088, 0x4A6F, 0x5C72, 0x3824, 0x57F1, 0x73C7, 0x9751,
	0xCB23, 0xA17C, 0xE89C, 0x3E21, 0x96DD, 0x61DC, 0x0D86, 0x0F85,
	0xE090, 0x7C42, 0x71C4, 0xCCAA, 0x90D8, 0x0605, 0xF701, 0x1C12,
	0xC2A3, 0x6A5F, 0xAEF9, 0x69D0, 0x1791, 0x9958, 0x3A27, 0x27B9,
	0xD938, 0xEB13, 0x2BB3, 0x2233, 0xD2BB, 0xA970, 0x0789, 0x33A7,
	0x2DB6, 0x3C22, 0x1592, 0xC920, 0x8749, 0xAAFF, 0x5078, 0xA57A,
	0x038F, 0x59F8, 0x0980, 0x1A17, 0x65DA, 0xD731, 0x84C6, 0xD0B8,
	0x82C3, 0x29B0, 0x5A77, 0x1E11, 0x7BCB, 0xA8FC, 0x6DD6, 0x2C3A,
};
static uint16_t le16(const uint8_t*p){return (uint16_t)(p[0]|((unsigned)p[1]<<8));}
static uint32_t le32(const uint8_t*p){return (uint32_t)le16(p)|((uint32_t)le16(p+2)<<16);}
static void put32(uint8_t*p,uint32_t v){unsigned i;for(i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static uint16_t sub(uint16_t v){uint16_t t=Sbox[v>>8];return Sbox[v&255]^(uint16_t)((t<<8)|(t>>8));}
static uint16_t rr1(uint16_t v){return (uint16_t)((v>>1)|(v<<15));}
static void mixing(const uint8_t tk[16],const uint8_t ta[6],uint64_t tsc,uint8_t key[16]){
    uint16_t v[6];unsigned i,j;
    v[0]=(uint16_t)(tsc>>16);v[1]=(uint16_t)(tsc>>32);
    v[2]=le16(ta);v[3]=le16(ta+2);v[4]=le16(ta+4);
    for(i=0;i<8;i++){
        j=2*(i&1);v[0]+=sub(v[4]^le16(tk+j));v[1]+=sub(v[0]^le16(tk+4+j));
        v[2]+=sub(v[1]^le16(tk+8+j));v[3]+=sub(v[2]^le16(tk+12+j));
        v[4]+=(uint16_t)(sub(v[3]^le16(tk+j))+i);
    }
    v[5]=(uint16_t)(v[4]+(uint16_t)tsc);
    for(i=0;i<6;i++)v[i]+=sub(v[(i+5)%6]^le16(tk+2*i));
    v[0]+=rr1(v[5]^le16(tk+12));v[1]+=rr1(v[0]^le16(tk+14));
    for(i=2;i<6;i++)v[i]+=rr1(v[i-1]);
    key[0]=(uint8_t)(tsc>>8);key[1]=(uint8_t)((key[0]|0x20)&0x7f);key[2]=(uint8_t)tsc;
    key[3]=(uint8_t)((v[5]^le16(tk))>>1);
    for(i=0;i<6;i++){key[4+2*i]=(uint8_t)v[i];key[5+2*i]=(uint8_t)(v[i]>>8);}
    memset(v,0,sizeof(v));
}
static void arc4(const uint8_t key[16],const uint8_t *in,uint8_t*out,size_t n){
    uint8_t s[256],tmp;unsigned i,j=0;size_t k;
    for(i=0;i<256;i++)s[i]=(uint8_t)i;
    for(i=0;i<256;i++){j=(j+s[i]+key[i&15])&255;tmp=s[i];s[i]=s[j];s[j]=tmp;}
    i=j=0;for(k=0;k<n;k++){i=(i+1)&255;j=(j+s[i])&255;tmp=s[i];s[i]=s[j];s[j]=tmp;out[k]=in[k]^s[(s[i]+s[j])&255];}
    memset(s,0,sizeof(s));
}
static uint32_t crc(const uint8_t*p,size_t n){uint32_t v=~0u;unsigned j;
    while(n--){v^=*p++;for(j=0;j<8;j++)v=(v>>1)^(0xedb88320u&(0u-(v&1)));}return ~v;
}
static uint32_t rol(uint32_t v,unsigned n){return (v<<n)|(v>>(32-n));}
static void block(uint32_t*l,uint32_t*r){
    *r^=rol(*l,17);*l+=*r;*r^=((*l&0xff00ffu)<<8)|((*l&0xff00ff00u)>>8);*l+=*r;
    *r^=rol(*l,3);*l+=*r;*r^=rol(*l,30);*l+=*r;
}
static void michael(const uint8_t key[8],const uint8_t *hdr,const uint8_t *p,size_t n,uint8_t mic[8]){
    uint8_t pseudo[16]={0},tail[4]={0};uint32_t l=le32(key),r=le32(key+4);unsigned i;
    /* FromDS: DA=addr1, SA=addr3. QoS priority is low four bits. */
    memcpy(pseudo,hdr+4,6);memcpy(pseudo+6,hdr+16,6);pseudo[12]=hdr[0]==0x88?(hdr[24]&15):0;
    for(i=0;i<16;i+=4){l^=le32(pseudo+i);block(&l,&r);}
    while(n>=4){l^=le32(p);block(&l,&r);p+=4;n-=4;}
    memcpy(tail,p,n);tail[n]=0x5a;l^=le32(tail);block(&l,&r);block(&l,&r);
    put32(mic,l);put32(mic+4,r);
}
int rtl_tkip_group_rx(const uint8_t gtk[32],const uint8_t ap[6],unsigned kid,uint64_t replay[17],
    const uint8_t*p,size_t n,uint8_t*out,size_t cap){
    uint8_t key[16],plain[2316],mic[8];uint64_t tsc;unsigned h,tid,i,diff=0;size_t len;
    if(!gtk||!ap||!replay||!p||!out||n<44||(p[0]!=8&&p[0]!=0x88)||
       (p[1]&0xc7)!=0x42||!(p[4]&1)||memcmp(p+10,ap,6)||(p[22]&15))return -1;
    h=p[0]==0x88?26:24;tid=h==26?(p[24]&15):16;
    if(n<h+20||(h==26&&(p[24]&0x80)))return -1;
    if((p[h+3]&0x3f)!=0x20||(p[h+3]>>6)!=kid||p[h+1]!=((p[h]|0x20)&0x7f))return -1;
    tsc=p[h+2]|((uint64_t)p[h]<<8);for(i=0;i<4;i++)tsc|=(uint64_t)p[h+4+i]<<(16+8*i);
    if(tsc<=replay[tid])return -3;
    len=n-h-8;if(len>sizeof(plain)||len<12||len-12>cap)return -1;
    mixing(gtk,ap,tsc,key);arc4(key,p+h+8,plain,len);memset(key,0,sizeof(key));
    if(crc(plain,len-4)!=le32(plain+len-4)){memset(plain,0,sizeof(plain));return -3;}
    michael(gtk+16,p,plain,len-12,mic);
    for(i=0;i<8;i++)diff|=mic[i]^plain[len-12+i];
    memset(mic,0,sizeof(mic));
    if(diff){memset(plain,0,sizeof(plain));return -5;}
    memcpy(out,plain,len-12);memset(plain,0,sizeof(plain));replay[tid]=tsc;return (int)(len-12);
}

