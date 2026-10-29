#include "wifi_known.h"
#include "physical_fixtures.h"
#include <string.h>
#include <stdio.h>
int wifi_known_test(void){
    static const uint8_t csv[]="\xef\xbb\xbfSSID;Sifre\r\n# test\r\nUnknown;password\r\nIEEE;password\r\n";
    RtlScanList list={0};uint8_t pmk[32],flags[RTL_SCAN_CAPACITY];unsigned selected=99;
    list.count=1;memcpy(list.ap[0].ssid,"IEEE",4);list.ap[0].ssid_length=4;list.ap[0].wpa2_ccmp_psk=1;list.ap[0].channel=6;
    if(wifi_known_select(csv,sizeof(csv)-1,&list,&selected,pmk)!=1||selected||memcmp(pmk,fx_pmk,32)){printf("WIFI_KNOWN_TEST FAIL line=%d\n",__LINE__);return 1;}
    if(wifi_known_mark(csv,sizeof(csv)-1,&list,flags)!=1||flags[0]!=1){printf("WIFI_KNOWN_TEST FAIL line=%d\n",__LINE__);return 1;}
    {const char*with_ip="IEEE;password;192.0.2.25;static:192.0.2.99\r\n";
     if(wifi_known_select((const uint8_t*)with_ip,strlen(with_ip),&list,&selected,pmk)!=1||memcmp(pmk,fx_pmk,32)){printf("WIFI_KNOWN_TEST FAIL CSV-IP-password-preservation\n");return 1;}}
    if(wifi_known_mark((const uint8_t*)"ieee;password",13,&list,flags)!=0||flags[0]){printf("WIFI_KNOWN_TEST FAIL line=%d\n",__LINE__);return 1;}
    if(wifi_known_mark((const uint8_t*)"IEEE;short",10,&list,flags)!=0||flags[0]){printf("WIFI_KNOWN_TEST FAIL line=%d\n",__LINE__);return 1;}
    if(wifi_known_select((const uint8_t*)"ieee;password",13,&list,&selected,pmk)!=0){printf("WIFI_KNOWN_TEST FAIL line=%d\n",__LINE__);return 1;}
    if(wifi_known_select((const uint8_t*)"IEEE;short",10,&list,&selected,pmk)!=0){printf("WIFI_KNOWN_TEST FAIL line=%d\n",__LINE__);return 1;}
    list.ap[0].wpa2_ccmp_psk=0;if(wifi_known_select(csv,sizeof(csv)-1,&list,&selected,pmk)!=0){printf("WIFI_KNOWN_TEST FAIL line=%d\n",__LINE__);return 1;}
    {WifiApp a;char label[80];wifi_app_init(&a);wifi_app_driver(&a,0);list.ap[0].wpa2_ccmp_psk=1;
     wifi_app_scan(&a,a.ticket,&list,0);a.known[0]=1;wifi_app_input(&a,WIFI_R2,0);
     if(a.state!=WIFI_DERIVING||wifi_app_take_actions(&a)!=WIFI_DO_KNOWN||a.password_length){printf("WIFI_KNOWN_TEST FAIL line=%d\n",__LINE__);return 1;}
     wifi_app_failure(&a,a.ticket,"fixture");wifi_app_input(&a,0,1);a.known[0]=WIFI_KNOWN_PROFILE;a.has_profile=1;a.profile.ssid_length=4;memcpy(a.profile.ssid,"IEEE",4);memcpy(a.profile.pmk,fx_pmk,32);
     wifi_app_input(&a,WIFI_R2,2);if(a.state!=WIFI_JOINING||wifi_app_take_actions(&a)!=WIFI_DO_JOIN||memcmp(a.pending_pmk,fx_pmk,32)||a.password_length){printf("WIFI_KNOWN_TEST FAIL line=%d\n",__LINE__);return 1;}
     wifi_app_failure(&a,a.ticket,"fixture");wifi_app_take_actions(&a);wifi_app_input(&a,0,3);a.has_profile=0;a.known[0]=0;wifi_app_input(&a,WIFI_R2,4);
     if(a.state!=WIFI_LIST||wifi_app_take_actions(&a)){printf("WIFI_KNOWN_TEST FAIL line=%d\n",__LINE__);return 1;}
     wifi_pad_description(WIFI_X,label,sizeof(label));if(strcmp(label,"X")){printf("WIFI_KNOWN_TEST FAIL line=%d\n",__LINE__);return 1;}
     wifi_pad_description(WIFI_X|WIFI_TRIANGLE,label,sizeof(label));if(strcmp(label,"X + TRIANGLE")){printf("WIFI_KNOWN_TEST FAIL line=%d\n",__LINE__);return 1;}
     wifi_pad_description(0,label,sizeof(label));if(strcmp(label,"NONE")){printf("WIFI_KNOWN_TEST FAIL line=%d\n",__LINE__);return 1;}
     memset(label,0x55,sizeof(label));wifi_pad_description(0xffff,label,2);if(label[0]&&label[1]){printf("WIFI_KNOWN_TEST FAIL line=%d\n",__LINE__);return 1;}
    }
    printf("WIFI_KNOWN_TEST PASS BOM-CRLF-header-case-security-PMK-known-mark-R2-CSV-saved-profile-no-password-pad-labels\n");return 0;
}
