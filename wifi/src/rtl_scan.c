/* SPDX-License-Identifier: MIT */
#include "rtl_scan.h"
#include <string.h>
static unsigned int le16(const uint8_t *p){return p[0]|((unsigned int)p[1]<<8);}
static int suite(const uint8_t *p,unsigned int type){return !p[0]&&p[1]==0x0f&&p[2]==0xac&&p[3]==type;}
enum {JOIN_OK,JOIN_INVALID,JOIN_GROUP,JOIN_PAIRWISE,JOIN_AKM,JOIN_WPA3,JOIN_PMF,JOIN_TKIP,JOIN_ENTERPRISE};
static int rsn(const uint8_t *p,size_t n,int *join,int *why){
    size_t off=0;unsigned int count,i,caps=0;int group,tkip,ccmp=0,psk=0,sae=0,enterprise=0;
    if(why)*why=JOIN_INVALID;
    *join=0;if(n<8||le16(p)!=1)return -1;
    group=suite(p+2,4);tkip=suite(p+2,2);off=6;
    count=le16(p+off);off+=2;if(!count||count>16||count>(n-off)/4)return -1;
    for(i=0;i<count;i++,off+=4)if(suite(p+off,4))ccmp=1;
    if(n-off<2)return -1;
    count=le16(p+off);off+=2;if(!count||count>16||count>(n-off)/4)return -1;
    for(i=0;i<count;i++,off+=4){if(suite(p+off,2))psk=1;if(suite(p+off,8))sae=1;if(suite(p+off,1))enterprise=1;}
    if(n-off){if(n-off<2)return -1;caps=le16(p+off);off+=2;}
    if(n-off){
        if(n-off<2)return -1;
        count=le16(p+off);off+=2;if(count>(n-off)/16)return -1;off+=count*16;
        if(n-off){if(n-off!=4)return -1;off+=4;}
    }
    if(off!=n)return -1;
    *join=(group||tkip)&&ccmp&&psk&&!(caps&0x0040); /* pairwise CCMP, group CCMP/TKIP */
    if(why)*why=*join?JOIN_OK:(!(group||tkip)?JOIN_GROUP:(!ccmp?JOIN_PAIRWISE:(!psk?(sae?JOIN_WPA3:(enterprise?JOIN_ENTERPRISE:JOIN_AKM)):JOIN_PMF)));
    return 0;
}
int rtl_scan_parse(const uint8_t *f,size_t n,unsigned int channel,int signal,RtlAccessPoint *out){
    RtlAccessPoint ap;size_t off;unsigned int id,len,i;int ssid_seen=0,ds_seen=0,ht_seen=0,rsn_seen=0,wpa=0,join=0;
    unsigned int ds=0,ht=0;uint8_t any=0;
    if(!f||!out||n<36||n>2304||channel<1||channel>14)return -1;
    if((le16(f)&0x00ff)!=0x0080&&(le16(f)&0x00ff)!=0x0050)return -1;
    if((le16(f)&0xc700)||(f[22]&15))return -1; /* DS/protected/HT/fragmented management. */
    if(f[16]&1)return -1;
    for(i=0;i<6;i++)any|=f[16+i];
    if(!any||memcmp(f+10,f+16,6))return -1;
    memset(&ap,0,sizeof(ap));memcpy(ap.bssid,f+16,6);ap.capabilities=(uint16_t)le16(f+34);
    if(!(ap.capabilities&1)||(ap.capabilities&2))return -1; /* infrastructure AP */
    ap.signal=signal;
    for(off=36;off<n;off+=len){
        if(n-off<2)return -1;
        id=f[off];len=f[off+1];off+=2;if(len>n-off)return -1;
        if(id==0){
            if(ssid_seen++||len>32)return -1;
            ap.ssid_length=(uint8_t)len;memcpy(ap.ssid,f+off,len);
        }else if(id==1||id==50){
            for(i=0;i<len;i++)if((f[off+i]&127)==12)ap.ofdm6=1;
        }else if(id==3){
            if(ds_seen++||len!=1||f[off]<1||f[off]>14)return -1;
            ds=f[off];
        }else if(id==61){
            if(ht_seen++||len!=22||f[off]<1||f[off]>14)return -1;
            ht=f[off];
        }else if(id==48){
            if(rsn_seen++||rsn(f+off,len,&join,0))return -1;
            ap.rsn_length=(uint16_t)(len+2);memcpy(ap.rsn,f+off-2,len+2);
        }else if(id==221&&len>=4&&!memcmp(f+off,"\x00\x50\xf2\x01",4))wpa=1;
    }
    if(!ssid_seen||(ds&&ht&&ds!=ht))return -1;
    ap.channel=(uint8_t)(ds?ds:(ht?ht:channel));
    ap.security=rsn_seen?(join?RTL_SEC_WPA2:RTL_SEC_OTHER_RSN):((wpa||(ap.capabilities&0x10))?RTL_SEC_LEGACY:RTL_SEC_OPEN);
    ap.wpa2_ccmp_psk=(uint8_t)(rsn_seen&&join&&!!(ap.capabilities&0x10));
    if(rsn_seen&&!(ap.capabilities&0x10))ap.security=RTL_SEC_OTHER_RSN;
    *out=ap;return 0;
}
int rtl_scan_add(RtlScanList *list,const RtlAccessPoint *ap){
    unsigned int i;
    if(!list||!ap||list->count>RTL_SCAN_CAPACITY||ap->ssid_length>32||ap->channel<1||ap->channel>14)return -1;
    for(i=0;i<list->count;i++)if(!memcmp(list->ap[i].bssid,ap->bssid,6)){
        RtlAccessPoint copy=*ap;
        if(!copy.ssid_length){copy.ssid_length=list->ap[i].ssid_length;memcpy(copy.ssid,list->ap[i].ssid,32);}
        list->ap[i]=copy;return (int)i;
    }
    if(list->count==RTL_SCAN_CAPACITY)return -1;
    list->ap[list->count]=*ap;return (int)list->count++;
}
int rtl_scan_label(const RtlAccessPoint *ap,char *out,size_t cap){
    static const char hex[]="0123456789ABCDEF";size_t i,n=0;
    if(!ap||!out||!cap||ap->ssid_length>32)return -1;
    out[0]=0;
    if(!ap->ssid_length){if(cap<9)return -1;memcpy(out,"<hidden>",9);return 0;}
    for(i=0;i<ap->ssid_length;i++){
        uint8_t c=ap->ssid[i];size_t need=(c>=32&&c<127&&c!='\\')?1:4;
        if(cap-n<=need){out[0]=0;return -1;}
        if(need==1)out[n++]=(char)c;
        else{out[n++]='\\';out[n++]='x';out[n++]=hex[c>>4];out[n++]=hex[c&15];}
    }
    out[n]=0;return 0;
}
static int join_reason(const RtlAccessPoint *ap){
    int join=0,why=JOIN_INVALID;
    if(!ap)return why;
    if(ap->wpa2_ccmp_psk)return JOIN_OK;
    if(ap->rsn_length<2||ap->rsn_length>sizeof(ap->rsn)||ap->rsn[0]!=48||ap->rsn[1]+2u!=ap->rsn_length)return why;
    if(rsn(ap->rsn+2,ap->rsn_length-2,&join,&why))return JOIN_INVALID;
    return join?JOIN_INVALID:why;
}
const char *rtl_scan_security_label(const RtlAccessPoint *ap){
    if(!ap)return "UNKNOWN";
    if(ap->wpa2_ccmp_psk)return ap->rsn_length>=8&&ap->rsn[7]==2?"AES+TKIP":"WPA2/AES";
    if(ap->security==RTL_SEC_OPEN)return "OPEN";
    if(ap->security==RTL_SEC_LEGACY)return "WPA/WEP";
    switch(join_reason(ap)){
    case JOIN_TKIP:return "TKIP";case JOIN_WPA3:return "WPA3";
    case JOIN_ENTERPRISE:return "802.1X";case JOIN_PMF:return "PMF";
    case JOIN_PAIRWISE:return "NO AES";default:return "UNSUPPORTED";}
}
const char *rtl_scan_join_message(const RtlAccessPoint *ap){
    if(!ap)return "Network info unavailable. TRIANGLE: rescan.";
    if(ap->wpa2_ccmp_psk)return "Enter password. R2: connect SELECT: back";
    if(ap->security==RTL_SEC_OPEN)return "Open networks are not supported in this build.";
    if(ap->security==RTL_SEC_LEGACY)return "WPA/WEP or missing RSN; this build supports WPA2-Personal.";
    switch(join_reason(ap)){
    case JOIN_TKIP:return "TKIP group cipher. Select WPA2-PSK / AES on the access point.";
    case JOIN_PAIRWISE:return "Pairwise cipher is not AES/CCMP; unsupported.";
    case JOIN_WPA3:return "WPA3/SAE-only network. WPA2-PSK / AES is required.";
    case JOIN_ENTERPRISE:return "802.1X enterprise network. This build supports WPA2-Personal.";
    case JOIN_PMF:return "Mandatory PMF is not supported in this build.";
    case JOIN_GROUP:return "Unsupported group cipher; AES/CCMP required.";
    case JOIN_AKM:return "Unsupported authentication method; PSK required.";
    default:return "Incompatible/missing security information. TRIANGLE: rescan.";}
}
