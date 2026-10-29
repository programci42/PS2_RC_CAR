#include "rtl_net_packet.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char**argv){
 unsigned char tx[2048],rx[2048],bad[2048];unsigned int answers=0,i,checks=0,xor16=0;size_t n;FILE*f;
 assert(argc==2);f=fopen(argv[1],"rb");assert(f);n=fread(rx,1,sizeof(rx),f);fclose(f);assert(n>=125);
 assert(rtl_net_query(tx,sizeof(tx),0x5405,49157)==121);checks++;
 assert(tx[2]==32&&(tx[3]&0x8c)==0x8c&&tx[32]==8&&tx[33]==1);checks++;
 assert(tx[32+32+12]==10&&tx[32+32+14]==2&&tx[32+32+15]==15);checks++;
 assert(tx[32+32+16]==10&&tx[32+32+18]==2&&tx[32+32+19]==3);checks++;
 for(i=0;i<32;i+=2){xor16^=tx[i]|((unsigned int)tx[i+1]<<8);}assert(xor16==0);checks++;
 assert(rtl_net_query(tx,120,0x5405,49157)==-1);checks++;
 assert(rtl_net_response(rx,n,0x5405,49157,&answers)==0&&answers>0);checks++;
 assert(rtl_net_response(rx,n,0x5404,49157,&answers)<0);checks++;
 assert(rtl_net_response(rx,n,0x5405,49156,&answers)<0);checks++;
 for(i=0;i<n;i++){assert(rtl_net_response(rx,i,0x5405,49157,&answers)<0);checks++;}
 memcpy(bad,rx,n);bad[1]|=0x40;assert(rtl_net_response(bad,n,0x5405,49157,&answers)<0);checks++;
 memcpy(bad,rx,n);bad[24+24]^=1;assert(rtl_net_response(bad,n,0x5405,49157,&answers)<0);checks++;
 memcpy(bad,rx,n);bad[24+32+10]^=1;assert(rtl_net_response(bad,n,0x5405,49157,&answers)<0);checks++;
 memcpy(bad,rx,n);bad[24+32+20+4]=0;bad[24+32+20+5]=0;assert(rtl_net_response(bad,n,0x5405,49157,&answers)<0);checks++;
 memcpy(bad,rx,n);bad[24+32+20+8+12]^=1;assert(rtl_net_response(bad,n,0x5405,49157,&answers)<0);checks++;
 printf("Network framing checks=%u failures=0, input=actual emulator USB RX capture\n",checks);return 0;
}
