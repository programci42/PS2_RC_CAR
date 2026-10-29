/* SPDX-License-Identifier: MIT -- two-slot storage recovery using public data. */
#include "wifi_store.h"
#include <stdio.h>
#include <string.h>
static unsigned int checks;
#define CHECK(x) do{checks++;if(!(x)){printf("WIFI_STORE_TEST FAIL line=%d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv){
    WifiProfile p,loaded;char path[200];FILE *f;uint8_t data[WIFI_PROFILE_BYTES];unsigned int i;
    CHECK(argc==2);memset(&p,0,sizeof(p));p.ssid_length=6;memcpy(p.ssid,"Lab AP",6);memset(p.pmk,0x42,32);
    CHECK(wifi_store_load(argv[1],&loaded)!=0);CHECK(wifi_store_save(argv[1],&p)==0&&p.generation==1);
    CHECK(wifi_store_load(argv[1],&loaded)==0&&loaded.generation==1&&!memcmp(loaded.pmk,p.pmk,32));
    memset(p.pmk,0x43,32);CHECK(wifi_store_save(argv[1],&p)==0&&p.generation==2);CHECK(wifi_store_load(argv[1],&loaded)==0&&loaded.generation==2&&loaded.pmk[0]==0x43);
    snprintf(path,sizeof(path),"%s/NET1.DAT",argv[1]);
    /* Every truncated second write must preserve the first good slot. */
    CHECK(wifi_profile_encode(&p,data)==0);
    for(i=0;i<WIFI_PROFILE_BYTES;i++){
        f=fopen(path,"wb");CHECK(f!=NULL);CHECK(fwrite(data,1,i,f)==i);CHECK(fclose(f)==0);
        CHECK(wifi_store_load(argv[1],&loaded)==0&&loaded.generation==1&&loaded.pmk[0]==0x42);
    }
    CHECK(wifi_store_save(argv[1],&p)==0&&p.generation==2);CHECK(wifi_store_load(argv[1],&loaded)==0&&loaded.pmk[0]==0x43);
    f=fopen(path,"ab");CHECK(f!=NULL);CHECK(fputc('X',f)=='X');CHECK(fclose(f)==0);
    CHECK(wifi_store_load(argv[1],&loaded)==0&&loaded.generation==1);
    CHECK(wifi_store_save("/nonexistent-rtl-wifi-test-dir",&p)!=0);
    printf("WIFI_STORE_TEST PASS checks=%u two-slot-recovery=YES credentials=synthetic\n",checks);return 0;
}
