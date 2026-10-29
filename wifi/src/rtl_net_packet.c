/* SPDX-License-Identifier: MIT
 * RTL8192CU 32-byte TX / 24-byte RX descriptor framing, restricted virtual
 * pre-associated unencrypted 802.11 laboratory data profile.
 */
#include "rtl_net_packet.h"
#include <string.h>
static unsigned int be16(const uint8_t*p){return ((unsigned int)p[0]<<8)|p[1];}
static unsigned int le16(const uint8_t*p){return p[0]|((unsigned int)p[1]<<8);}
static void putbe(uint8_t*p,unsigned int n){p[0]=n>>8;p[1]=n;}
static unsigned int checksum(const uint8_t*p,unsigned int n){unsigned int sum=0;while(n>1){sum+=be16(p);p+=2;n-=2;}if(n)sum+=(unsigned int)*p<<8;while(sum>>16)sum=(sum&65535)+(sum>>16);return (~sum)&65535;}
int rtl_net_query(uint8_t*out,unsigned int capacity,unsigned int id,unsigned int port){
 static const uint8_t mac[6]={2,0x50,0x53,0x32,0,0x15},ap[6]={2,0x50,0x53,0x32,0,1};
 static const uint8_t snap[8]={0xaa,0xaa,3,0,0,0,8,0};
 static const uint8_t question[17]={7,'e','x','a','m','p','l','e',3,'c','o','m',0,0,1,0,1};
 uint8_t *w,*ip,*udp,*dns;unsigned int i,xor16=0;
 const unsigned int dnslen=12+sizeof(question),wireless=24+8+20+8+dnslen,total=32+wireless;
 if(!out||capacity<total||id>65535||port>65535||!port)return -1;
 memset(out,0,total);out[0]=wireless;out[1]=wireless>>8;out[2]=32;out[3]=0x8c;
 out[4]=0;out[5]=0; /* MACID 0, best-effort queue */
 for(i=0;i<32;i+=2){xor16^=le16(out+i);}out[28]=xor16;out[29]=xor16>>8;
 w=out+32;w[0]=8;w[1]=1;memcpy(w+4,ap,6);memcpy(w+10,mac,6);memcpy(w+16,ap,6);memcpy(w+24,snap,8);
 ip=w+32;ip[0]=0x45;putbe(ip+2,20+8+dnslen);putbe(ip+4,id);ip[8]=64;ip[9]=17;
 ip[12]=10;ip[14]=2;ip[15]=15;ip[16]=10;ip[18]=2;ip[19]=3;putbe(ip+10,checksum(ip,20));
 udp=ip+20;putbe(udp,port);putbe(udp+2,53);putbe(udp+4,8+dnslen);
 dns=udp+8;putbe(dns,id);dns[2]=1;dns[5]=1;memcpy(dns+12,question,sizeof(question));
 return (int)total;
}
int rtl_net_response(const uint8_t*d,unsigned int n,unsigned int id,unsigned int port,unsigned int*answers){
 static const uint8_t snap[8]={0xaa,0xaa,3,0,0,0,8,0};
 static const uint8_t question[17]={7,'e','x','a','m','p','l','e',3,'c','o','m',0,0,1,0,1};
 const uint8_t *w,*ip,*udp,*dns;unsigned int packet,ipsize,udpsize;
 if(!d||!answers||n<24+24+8+20+8+12)return -1;
 packet=le16(d)&0x3fff;if(packet+24!=n||(d[1]&0xc0)||(d[2]&0x0f)||(d[3]&3))return -2;
 w=d+24;if(le16(w)!=0x0208||memcmp(w+24,snap,8))return -3;
 ip=w+32;if(ip[0]!=0x45||ip[9]!=17||checksum(ip,20))return -4;
 ipsize=be16(ip+2);if(ipsize!=packet-32||ipsize<40)return -5;
 if(ip[12]!=10||ip[13]||ip[14]!=2||ip[15]!=3||ip[16]!=10||ip[17]||ip[18]!=2||ip[19]!=15)return -6;
 udp=ip+20;udpsize=be16(udp+4);if(be16(udp)!=53||be16(udp+2)!=port||udpsize!=ipsize-20||udpsize<20)return -7;
 dns=udp+8;if(udpsize<8+12+sizeof(question)||be16(dns)!=id||!(dns[2]&0x80)||(dns[3]&15)||be16(dns+4)!=1||memcmp(dns+12,question,sizeof(question)))return -8;
 *answers=be16(dns+6);return *answers?0:-9;
}
