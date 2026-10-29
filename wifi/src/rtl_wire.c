/* SPDX-License-Identifier: MIT. RTL8192C USB descriptors (NetBSD if_urtwnreg.h). */
#include "rtl_wire.h"
#include <string.h>
static unsigned le16(const uint8_t*p){return p[0]|((unsigned)p[1]<<8);}
/* PHY quality + 1 (zero = absent), based on the RTL8192CU status layout.
   This is a PHY SQ/EVM indicator, not calibrated RSSI or a throughput rate. */
unsigned rtl_wire_quality(const uint8_t*p,unsigned n){unsigned off,size,rate,sq;int evm;if(!p||n<24||!(p[3]&4)||(p[1]&0xc0))return 0;off=24+(p[3]&3);size=(p[2]&15)*8;rate=p[12]&63;if(size<8||off+size>n)return 0;
 if(rate<=3){sq=p[off+4];return 1+(sq>64?0:sq<20?100:(64-sq)*100/44);}
 if(size<16){return 0;}evm=(int)(int8_t)p[off+13]/2;if(evm>0)evm=0;if(evm< -33)evm=-33;return 1+(evm==-33?100:(unsigned)(-evm*3));}
int rtl_wire_tx_rate(const uint8_t *p,unsigned n,uint8_t *out,unsigned cap,unsigned packet,unsigned rate){
    unsigned off=32,i,check=0,fast;
    if(!p||!out||n<24||n>RTL_WIRE_MAX||(packet!=64&&packet!=512))return -1;
    fast=rate==4&&(p[0]&0x0c)==8&&(p[1]&0x40);
    /* A USB transfer exactly divisible by max packet requires an extra
     * 8-byte packet-offset area, otherwise the adapter waits for a ZLP. */
    if((n+32)%packet==0)off=40;
    if(cap<n+off)return -1;
    memset(out,0,off);out[0]=n;out[1]=n>>8;out[2]=32;out[3]=0x8c;
    if(p[4]&1)out[3]|=1; /* BMC, descriptor bit24 */
    out[5]=(p[0]&0x0c)==0?0x12:0; /* management / best effort */
    out[6]=fast?5:6; /* 11G OFDM6 for negotiated protected data; 11B otherwise. */
    out[14]=(le16(p+22)>>4)&255;out[15]=(le16(p+22)>>12)&15;
    if(off==40)out[7]|=4; /* packet offset bit26 in DW1 */
    out[16]=0;out[17]=1; /* driver-selected rate DW4 bit8, legacy 1Mbps */
    out[20]=fast?4:0; /* OFDM6 (index 4), otherwise CCK1. No HT is claimed. */
    for(i=0;i<32;i+=2)check^=le16(out+i);
    out[28]=check;out[29]=check>>8;
    memcpy(out+off,p,n);return n+off;
}
int rtl_wire_tx(const uint8_t*p,unsigned n,uint8_t*out,unsigned cap,unsigned packet){return rtl_wire_tx_rate(p,n,out,cap,packet,0);}
int rtl_wire_rx_next(const uint8_t *p,unsigned n,uint8_t *out,unsigned cap,unsigned *consumed){
    unsigned len,off,total;if(!consumed)return -1;*consumed=0;
    if(!p||!out||n<24)return -1;
    len=le16(p)&0x3fff;off=24+(p[2]&15)*8+((p[3]>>0)&3);
    if(!len||off>n||len>n-off)return -1;
    total=off+len;*consumed=(total+127)&~127u;
    if(*consumed>n)*consumed=n; /* The last frame may omit alignment padding. */
    if((p[1]&0xc0)||len<24||len>cap)return -1;
    memcpy(out,p+off,len);return len;
}
int rtl_wire_rx(const uint8_t *p,unsigned n,uint8_t *out,unsigned cap){
    unsigned consumed;return rtl_wire_rx_next(p,n,out,cap,&consumed);
}
