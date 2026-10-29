/* SPDX-License-Identifier: MIT */
#include "wifi_store.h"
#include <stdio.h>
#include <string.h>
static void wipe(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static int name(char out[160],const char *dir,int slot){int n;if(!dir)return -1;n=snprintf(out,160,"%s/NET%d.DAT",dir,slot);return n<0||n>=160?-1:0;}
static int read_slot(const char *dir,int slot,WifiProfile *profile){
    char path[160];uint8_t data[WIFI_PROFILE_BYTES];FILE *f;size_t n;int extra,result;
    if(name(path,dir,slot))return -1;
    f=fopen(path,"rb");if(!f)return -1;
    n=fread(data,1,sizeof(data),f);extra=fgetc(f);result=ferror(f);if(fclose(f))result=1;
    if(result||n!=sizeof(data)||extra!=EOF){wipe(data,sizeof(data));return -1;}
    result=wifi_profile_decode(data,n,profile);wipe(data,sizeof(data));return result;
}
static int newest(const char *dir,WifiProfile *profile){
    WifiProfile a,b;int ra=read_slot(dir,0,&a),rb=read_slot(dir,1,&b),slot=-1;
    if(!ra&&(rb||!wifi_profile_newer(b.generation,a.generation))){*profile=a;slot=0;}
    else if(!rb){*profile=b;slot=1;}
    wipe(&a,sizeof(a));wipe(&b,sizeof(b));return slot;
}
int wifi_store_load(const char *dir,WifiProfile *profile){if(!profile)return -1;return newest(dir,profile)<0?-1:0;}
int wifi_store_save(const char *dir,WifiProfile *profile){
    char path[160];uint8_t data[WIFI_PROFILE_BYTES];WifiProfile current,copy,verify;int slot,result=-1;FILE *f;
    if(!profile)return -1;
    copy=*profile;slot=newest(dir,&current);
    copy.generation=slot<0?1:current.generation+1;slot=slot<0?0:1-slot;
    if(name(path,dir,slot)||wifi_profile_encode(&copy,data))goto done;
    f=fopen(path,"wb");if(!f)goto done;
    if(fwrite(data,1,sizeof(data),f)!=sizeof(data)){fclose(f);goto done;}
    if(fclose(f))goto done;
    if(read_slot(dir,slot,&verify)||memcmp(copy.ssid,verify.ssid,32)||copy.ssid_length!=verify.ssid_length||memcmp(copy.pmk,verify.pmk,32)||copy.generation!=verify.generation)goto done;
    *profile=copy;result=0;
done:
    wipe(data,sizeof(data));wipe(&current,sizeof(current));wipe(&copy,sizeof(copy));wipe(&verify,sizeof(verify));return result;
}
