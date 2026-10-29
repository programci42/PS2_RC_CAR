/* SPDX-License-Identifier: MIT */
#include "rtl_supplicant.h"
#include "rtl_wpa_crypto.h"
#include "rtl_tkip.h"
#include <string.h>
static unsigned be16(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
static void put16(uint8_t *p,unsigned v){p[0]=v>>8;p[1]=v;}
static int nonzero(const uint8_t *p,size_t n){unsigned v=0;while(n--)v|=*p++;return v!=0;}
void rtl_supplicant_clear(RtlSupplicant *s){volatile uint8_t *p=(volatile uint8_t*)s;size_t n=sizeof(*s);if(s)while(n--)*p++=0;}
int rtl_supplicant_station_rsn(const uint8_t *p,size_t n,uint8_t out[22]){
    static const uint8_t base[22]={48,20,1,0,0,15,172,4,1,0,0,15,172,4,1,0,0,15,172,2,0,0};
    size_t pos=8;unsigned count,i,caps=0;int ccmp=0,psk=0;
    if(!p||!out||n<20||n>257||p[0]!=48||p[1]+2u!=n||p[2]!=1||p[3]||
       memcmp(p+4,"\x00\x0f\xac",3)||(p[7]!=2&&p[7]!=4))return -1;
    count=p[pos]|((unsigned)p[pos+1]<<8);pos+=2;
    if(!count||count>16||count>(n-pos)/4)return -1;
    for(i=0;i<count;i++,pos+=4)if(!memcmp(p+pos,"\x00\x0f\xac\x04",4))ccmp=1;
    if(n-pos<2)return -1;
    count=p[pos]|((unsigned)p[pos+1]<<8);pos+=2;
    if(!count||count>16||count>(n-pos)/4)return -1;
    for(i=0;i<count;i++,pos+=4)if(!memcmp(p+pos,"\x00\x0f\xac\x02",4))psk=1;
    if(n-pos){if(n-pos<2)return -1;caps=p[pos]|((unsigned)p[pos+1]<<8);pos+=2;}
    if(n-pos){if(n-pos<2)return -1;count=p[pos]|((unsigned)p[pos+1]<<8);pos+=2;
        if(count>(n-pos)/16)return -1;
        pos+=count*16;if(n-pos){if(n-pos!=4)return -1;pos+=4;}}
    if(pos!=n||!ccmp||!psk||(caps&0x40))return -1;
    memcpy(out,base,22);out[7]=p[7];return 0;
}
int rtl_supplicant_start(RtlSupplicant *s,const uint8_t pmk[32],const uint8_t ap[6],
    const uint8_t sta[6],const uint8_t sn[32],const uint8_t *rsn,size_t n){
    uint8_t negotiated[22];
    if(!s||!pmk||!ap||!sta||!sn||!rsn||rtl_supplicant_station_rsn(rsn,n,negotiated)||
       !nonzero(sn,32)||(ap[0]&1)||(sta[0]&1)||!nonzero(ap,6)||!nonzero(sta,6))return -1;
    rtl_supplicant_clear(s);memcpy(s->pmk,pmk,32);memcpy(s->ap,ap,6);memcpy(s->sta,sta,6);
    memcpy(s->snonce,sn,32);memcpy(s->rsn,rsn,n);s->rsn_length=n;s->group_cipher=negotiated[7];s->state=RTL_WPA_WAIT_M1;return 0;
}
static int reply(RtlSupplicant *s,uint8_t version,unsigned info,const uint8_t replay[8],
    int m2,uint8_t *out,size_t cap){
    /* Single CCMP group/pairwise cipher, PSK AKM, no PMF/WMM/HT advertised. */
    uint8_t rsn[22];
    if(rtl_supplicant_station_rsn(s->rsn,s->rsn_length,rsn))return -1;
    size_t n=99+(m2?sizeof(rsn):0);if(cap<n)return -1;
    memset(out,0,n);out[0]=version;out[1]=3;put16(out+2,n-4);out[4]=2;
    /* RSN supplicant M2/M4/group replies have zero Key Length. The
     * authenticator's M1/M3 length is not copied into these responses. */
    put16(out+5,info);put16(out+7,0);memcpy(out+9,replay,8);
    if(m2){memcpy(out+17,s->snonce,32);put16(out+97,sizeof(rsn));memcpy(out+99,rsn,sizeof(rsn));}
    if(rtl_wpa_eapol_mic(s->ptk,out,n,out+81))return -2;
    return (int)n;
}
int rtl_supplicant_rx(RtlSupplicant *s,const uint8_t *p,size_t n,uint8_t *out,size_t cap){
    unsigned info,klen,gtk_len;int order;uint8_t plain[512],gtk[32]={0};size_t len=0,pos;
    unsigned found_rsn=0,found_gtk=0,kid=0;
    if(!s||!p||!out||!s->state||n<99||n>512)return -1;
    if((p[0]!=1&&p[0]!=2)||p[1]!=3||p[4]!=2||be16(p+2)!=n-4||be16(p+97)!=n-99)return -1;
    info=be16(p+5);klen=be16(p+7);order=memcmp(p+9,s->replay,8);
    gtk_len=s->group_cipher==2?32:16;
    if((info&7)!=2||klen!=((info&8)?16:gtk_len))return -1;
    if(info==0x008a){ /* pairwise M1 */
        if(n!=99||!nonzero(p+17,32))return -1;
        if(s->state!=RTL_WPA_WAIT_M1 && s->state!=RTL_WPA_WAIT_M3)return -4;
        if(s->state==RTL_WPA_WAIT_M3){
            /* Retransmit M2, but never reuse SNonce for a changed handshake. */
            if(order<0||memcmp(p+17,s->anonce,32))return -3;
            memcpy(s->replay,p+9,8); /* AP may increment replay on an M1 retransmission. */
        }else{
            memcpy(s->anonce,p+17,32);memcpy(s->replay,p+9,8);
            if(rtl_wpa_ptk(s->pmk,s->ap,s->sta,s->anonce,s->snonce,s->ptk))return -2;
            s->state=RTL_WPA_WAIT_M3;
        }
        return reply(s,p[0],0x010a,p+9,1,out,cap);
    }
    if(info==0x1382&&s->state==RTL_WPA_ONLINE){ /* group key handshake */
        int cmp=memcmp(p+9,s->last_group_length?s->group_replay:s->replay,8),result;
        if(cmp==0&&s->last_group_length==n&&!memcmp(p,s->last_group,n))
            return reply(s,p[0],0x0302,p+9,0,out,cap);
        if(cmp<=0||rtl_wpa_eapol_verify(s->ptk,p,n)||
           rtl_wpa_unwrap(s->ptk+16,p+99,n-99,plain,sizeof(plain),&len))return -3;
        for(pos=0;pos<len;){unsigned id=plain[pos],l;
            if(id==221&&(pos+1==len||plain[pos+1]==0))break;
            if(pos+2>len)return -1;
            l=plain[pos+1];if(pos+2+l>len)return -1;
            if(id==221&&l>=4&&!memcmp(plain+pos+2,"\x00\x0f\xac\x01",4)){
                if(found_gtk++||l!=6+gtk_len||(plain[pos+6]&0xf8)||plain[pos+7])return -3;
                kid=plain[pos+6]&3;memcpy(gtk,plain+pos+8,gtk_len);
            }pos+=l+2;
        }
        memset(plain,0,sizeof(plain));if(!found_gtk)return -3;
        result=reply(s,p[0],0x0302,p+9,0,out,cap);if(result<0)return result;
        /* Do not reset replay state if the AP repeats the same GTK, even
         * with a new EAPOL replay counter (group-key reinstallation attack). */
        if(memcmp(gtk,s->gtk,gtk_len)){
            uint64_t rsc=0;unsigned i;for(i=0;i<6;i++)rsc|=(uint64_t)p[65+i]<<(8*i);
            for(i=0;i<17;i++)s->group_pn[i]=rsc;
            memcpy(s->gtk,gtk,gtk_len);
        }
        s->gtk_index=kid;memcpy(s->group_replay,p+9,8);memcpy(s->last_group,p,n);s->last_group_length=n;
        memset(gtk,0,sizeof(gtk));return result;
    }
    if(info!=0x13ca)return 0;
    if(s->state<RTL_WPA_WAIT_M3||memcmp(p+17,s->anonce,32))return -3;
    if(s->state==RTL_WPA_ONLINE||s->state==RTL_WPA_WAIT_M4_TX){
        /* KRACK defence: a retransmitted M3 only gets an M4, no key/PN reset. */
        if(order||n!=s->last_m3_length||memcmp(p,s->last_m3,n))return -3;
        return reply(s,p[0],0x030a,p+9,0,out,cap);
    }
    if(order<=0||rtl_wpa_eapol_verify(s->ptk,p,n))return -3;
    if(rtl_wpa_unwrap(s->ptk+16,p+99,n-99,plain,sizeof(plain),&len))return -3;
    for(pos=0;pos<len;){
        unsigned id=plain[pos],l;
        if(id==0xdd && (pos+1==len||plain[pos+1]==0))break; /* AES wrap padding */
        if(pos+2>len){memset(plain,0,sizeof(plain));return -1;}l=plain[pos+1];
        if(pos+2+l>len){memset(plain,0,sizeof(plain));return -1;}
        if(id==48){
            if(found_rsn++||l+2!=s->rsn_length||memcmp(plain+pos,s->rsn,l+2))return -3;
        }else if(id==221&&l>=4&&!memcmp(plain+pos+2,"\x00\x0f\xac\x01",4)){
            if(found_gtk++||l!=6+gtk_len||(plain[pos+6]&0xf8)||plain[pos+7])return -3;
            kid=plain[pos+6]&3;memcpy(gtk,plain+pos+8,gtk_len);
        }
        pos+=l+2;
    }
    memset(plain,0,sizeof(plain));
    if(!found_rsn||!found_gtk)return -3;
    /* Build M4 before committing keys, so insufficient output storage is atomic. */
    {int result=reply(s,p[0],0x030a,p+9,0,out,cap);if(result<0)return result;
        memcpy(s->gtk,gtk,gtk_len);memset(gtk,0,sizeof(gtk));s->gtk_index=kid;
        memcpy(s->replay,p+9,8);memcpy(s->last_m3,p,n);s->last_m3_length=n;
        s->installs++;s->tx_pn=0;memset(s->rx_pn,0,sizeof(s->rx_pn));memset(s->group_pn,0,sizeof(s->group_pn));
        /* Key RSC is little endian; reject group frames at/below advertised RSC. */
        {uint64_t rsc=0;unsigned i;for(i=0;i<6;i++)rsc|=(uint64_t)p[65+i]<<(8*i);
         for(i=0;i<17;i++)s->group_pn[i]=rsc;}
        s->state=RTL_WPA_WAIT_M4_TX;return result;}
}
void rtl_supplicant_tx_done(RtlSupplicant *s,int ok){if(s&&ok&&s->state==RTL_WPA_WAIT_M4_TX)s->state=RTL_WPA_ONLINE;}
static size_t aad_nonce(const uint8_t *h,unsigned hlen,uint64_t pn,uint8_t *aad,uint8_t *nonce){
    unsigned i,tid=hlen==26?(h[24]&15):0;
    aad[0]=h[0]&0x8f;aad[1]=h[1]&0xc7;memcpy(aad+2,h+4,18);aad[20]=h[22]&15;aad[21]=0;
    if(hlen==26){aad[22]=tid;aad[23]=0;}
    nonce[0]=tid;memcpy(nonce+1,h+10,6);for(i=0;i<6;i++)nonce[7+i]=(uint8_t)(pn>>(8*(5-i)));
    return hlen==26?24:22;
}
int rtl_ccmp_protect(RtlSupplicant *s,const uint8_t h[24],const uint8_t *p,size_t n,uint8_t *o,size_t cap){
    uint8_t aad[24],nonce[13];uint64_t pn;unsigned i;size_t al;
    if(!s||!h||!p||!o||s->state!=RTL_WPA_ONLINE||n>2304||cap<n+40||
       h[0]!=8||(h[1]&3)!=1||(h[1]&0x84)||memcmp(h+10,s->sta,6)||memcmp(h+4,s->ap,6)||
       s->tx_pn>=0xffffffffffffULL)return -1;
    pn=++s->tx_pn;memcpy(o,h,24);o[1]|=0x40;o[24]=pn;o[25]=pn>>8;o[26]=0;o[27]=0x20;
    for(i=0;i<4;i++)o[28+i]=pn>>(16+8*i);
    al=aad_nonce(o,24,pn,aad,nonce);
    if(rtl_wpa_ccm_encrypt(s->ptk+32,nonce,aad,al,p,n,o+32,o+32+n))return -2;
    return (int)n+40;
}
int rtl_ccmp_unprotect(RtlSupplicant *s,const uint8_t *p,size_t n,uint8_t *o,size_t cap){
    uint8_t aad[24],nonce[13];uint64_t pn=0,*last;unsigned hlen,tid,kid,i,group;size_t al,len;
    if(!s||!p||!o||s->state!=RTL_WPA_ONLINE||n<40||(p[0]!=8&&p[0]!=0x88)||
       (p[1]&0xc7)!=0x42||memcmp(p+10,s->ap,6)||(p[22]&15))return -1;
    hlen=p[0]==0x88?26:24;if(n<hlen+16)return -1;
    tid=hlen==26?(p[24]&15):16;if(hlen==26&&(p[24]&0x80))return -1; /* no A-MSDU */
    group=!!(p[4]&1);if(!group&&memcmp(p+4,s->sta,6))return -1;
    if(group&&s->group_cipher==2)return -1;
    kid=p[hlen+3]>>6;if(p[hlen+2]||(p[hlen+3]&0x3f)!=0x20||
       (group?kid!=s->gtk_index:kid!=0))return -1;
    pn=p[hlen]|((uint64_t)p[hlen+1]<<8);for(i=0;i<4;i++)pn|=(uint64_t)p[hlen+4+i]<<(16+8*i);
    last=group?&s->group_pn[tid]:&s->rx_pn[tid];if(pn<=*last)return -3;
    len=n-hlen-16;if(len>2304||cap<len)return -1;
    al=aad_nonce(p,hlen,pn,aad,nonce);
    if(rtl_wpa_ccm_decrypt(group?s->gtk:s->ptk+32,nonce,aad,al,p+hlen+8,len,p+n-8,o))return -3;
    *last=pn;return (int)len;
}
int rtl_wifi_unprotect(RtlSupplicant *s,const uint8_t*p,size_t n,uint8_t*out,size_t cap){
    if(!s||!p||n<24||s->state!=RTL_WPA_ONLINE)return -1;
    if((p[4]&1)&&s->group_cipher==2)return rtl_tkip_group_rx(s->gtk,s->ap,s->gtk_index,s->group_pn,p,n,out,cap);
    return rtl_ccmp_unprotect(s,p,n,out,cap);
}
int rtl_supplicant_mic_report(RtlSupplicant *s,uint8_t*out,size_t cap){
    unsigned i;uint64_t counter;
    if(!s||!out||cap<99||s->state!=RTL_WPA_ONLINE||s->request_sequence==~(uint64_t)0)return -1;
    counter=++s->request_sequence;memset(out,0,99);out[0]=2;out[1]=3;out[3]=95;out[4]=2;
    put16(out+5,0x0f02);for(i=0;i<8;i++)out[16-i]=(uint8_t)(counter>>(8*i));
    if(rtl_wpa_eapol_mic(s->ptk,out,99,out+81))return -1;
    return 99;
}
