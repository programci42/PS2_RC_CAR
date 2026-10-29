/* SPDX-License-Identifier: MIT -- synthetic beacons, no RF observations. */
#include "rtl_scan.h"
#include <stdio.h>
#include <string.h>
static unsigned int checks;
#define CHECK(x) do{checks++;if(!(x)){printf("SCAN_PARSER_TEST FAIL line=%d\n",__LINE__);return 1;}}while(0)
static const uint8_t rsn_ie[]={48,20,1,0,0,15,172,4,1,0,0,15,172,4,1,0,0,15,172,2,0,0};
static size_t beacon(uint8_t *f){
    static const uint8_t mac[6]={2,1,2,3,4,5};size_t off=36;
    memset(f,0,512);f[0]=0x80;memset(f+4,255,6);memcpy(f+10,mac,6);memcpy(f+16,mac,6);f[32]=100;f[34]=0x11;
    f[off++]=0;f[off++]=6;memcpy(f+off,"Lab AP",6);off+=6;
    f[off++]=3;f[off++]=1;f[off++]=6;memcpy(f+off,rsn_ie,sizeof(rsn_ie));return off+sizeof(rsn_ie);
}
int main(void){
    uint8_t f[512],g[512];size_t n,i;RtlAccessPoint ap,old;RtlScanList list;char label[129];
    n=beacon(f);CHECK(rtl_scan_parse(f,n,1,-42,&ap)==0);
    CHECK(ap.channel==6&&ap.security==RTL_SEC_WPA2&&ap.wpa2_ccmp_psk&&ap.signal==-42);
    CHECK(ap.ssid_length==6&&!memcmp(ap.ssid,"Lab AP",6)&&ap.rsn_length==22);
    CHECK(rtl_scan_label(&ap,label,sizeof(label))==0&&!strcmp(label,"Lab AP"));
    f[0]=0x50;CHECK(rtl_scan_parse(f,n,1,-42,&ap)==0);f[0]=0x80;
    for(i=0;i<n;i++){memset(&ap,0xaa,sizeof(ap));old=ap;CHECK(rtl_scan_parse(f,i,1,-42,&ap)!=0||!ap.wpa2_ccmp_psk);if(ap.ssid_length==0xaa)CHECK(!memcmp(&ap,&old,sizeof(ap)));}
    memcpy(g,f,n);g[n]=0;g[n+1]=0;CHECK(rtl_scan_parse(g,n+2,1,-42,&ap)!=0);
    memcpy(g,f,n);g[45]=0;CHECK(rtl_scan_parse(g,n,1,-42,&ap)!=0); /* DS size */
    memcpy(g,f,n);g[46]=15;CHECK(rtl_scan_parse(g,n,1,-42,&ap)!=0);
    memcpy(g,f,n);g[36]=1;CHECK(rtl_scan_parse(g,n,1,-42,&ap)!=0); /* no SSID */
    memcpy(g,f,n);g[37]=33;CHECK(rtl_scan_parse(g,n,1,-42,&ap)!=0);
    memcpy(g,f,n);g[55]=255;CHECK(rtl_scan_parse(g,n,1,-42,&ap)!=0); /* pairwise count */
    memcpy(g,f,n);g[54]=2;CHECK(rtl_scan_parse(g,n,1,-42,&ap)==0&&ap.wpa2_ccmp_psk); /* TKIP group + CCMP pairwise */
    CHECK(!strcmp(rtl_scan_security_label(&ap),"AES+TKIP")&&strstr(rtl_scan_join_message(&ap),"Sifreyi"));
    memcpy(g,f,n);g[n-2]=0x40;CHECK(rtl_scan_parse(g,n,1,-42,&ap)==0&&!ap.wpa2_ccmp_psk); /* PMF required */
    CHECK(!strcmp(rtl_scan_security_label(&ap),"PMF")&&strstr(rtl_scan_join_message(&ap),"PMF"));
    memcpy(g,f,n);g[n-3]=8;CHECK(!rtl_scan_parse(g,n,1,-42,&ap)&&!ap.wpa2_ccmp_psk);
    CHECK(!strcmp(rtl_scan_security_label(&ap),"WPA3")&&strstr(rtl_scan_join_message(&ap),"WPA3"));
    memcpy(g,f,n);g[n-3]=1;CHECK(!rtl_scan_parse(g,n,1,-42,&ap)&&!ap.wpa2_ccmp_psk);
    CHECK(!strcmp(rtl_scan_security_label(&ap),"802.1X"));
    memcpy(g,f,n);g[34]=1;CHECK(rtl_scan_parse(g,n,1,-42,&ap)==0&&!ap.wpa2_ccmp_psk&&ap.security==RTL_SEC_OTHER_RSN);
    CHECK(rtl_scan_parse(f,n-22,1,-42,&ap)==0&&ap.security==RTL_SEC_LEGACY&&!ap.wpa2_ccmp_psk);
    memcpy(g,f,n);g[34]=1;CHECK(rtl_scan_parse(g,n-22,1,-42,&ap)==0&&ap.security==RTL_SEC_OPEN);
    for(i=0;i<4;i++){memcpy(g,f,n);g[1]=(uint8_t)(1u<<i);CHECK(rtl_scan_parse(g,n,1,-42,&ap)!=0||i==3);}
    memcpy(g,f,n);g[1]=0x40;CHECK(rtl_scan_parse(g,n,1,-42,&ap)!=0);
    memcpy(g,f,n);g[22]=1;CHECK(rtl_scan_parse(g,n,1,-42,&ap)!=0);
    memcpy(g,f,n);g[16]=3;CHECK(rtl_scan_parse(g,n,1,-42,&ap)!=0);
    CHECK(rtl_scan_parse(f,n,0,-42,&ap)!=0&&rtl_scan_parse(f,n,15,-42,&ap)!=0);
    CHECK(rtl_scan_parse(f,n,1,-42,&ap)==0);memset(&list,0,sizeof(list));
    CHECK(rtl_scan_add(&list,&ap)==0&&list.count==1);ap.signal=-30;CHECK(rtl_scan_add(&list,&ap)==0&&list.count==1&&list.ap[0].signal==-30);
    ap.ssid_length=0;CHECK(rtl_scan_add(&list,&ap)==0&&list.ap[0].ssid_length==6);
    CHECK(rtl_scan_label(&ap,label,sizeof(label))==0&&!strcmp(label,"<hidden>"));
    ap.ssid_length=4;ap.ssid[0]='A';ap.ssid[1]=0;ap.ssid[2]='\\';ap.ssid[3]=255;
    CHECK(rtl_scan_label(&ap,label,sizeof(label))==0&&!strcmp(label,"A\\x00\\x5C\\xFF"));
    for(i=1;i<=13;i++)CHECK(rtl_scan_label(&ap,label,i)!=0&&label[0]==0);
    for(i=1;i<RTL_SCAN_CAPACITY;i++){ap.bssid[5]=(uint8_t)(i+10);CHECK(rtl_scan_add(&list,&ap)==(int)i);}
    ap.bssid[5]=255;CHECK(rtl_scan_add(&list,&ap)!=0&&list.count==RTL_SCAN_CAPACITY);
    printf("SCAN_PARSER_TEST PASS checks=%u source=synthetic-beacons RF-scan=NO\n",checks);return 0;
}
