/* SPDX-License-Identifier: MIT
 * Independent diagnostic implementation. Register control request constants
 * are documented by Linux rtlwifi/usb.c. No RF initialisation is implemented.
 */
#include "rtl_core.h"
#include <string.h>
int rtl_matches(unsigned int vid, unsigned int pid) { return vid==RTL_VID && pid==RTL_PID; }
static unsigned int le16(const uint8_t *p) { return p[0] | ((unsigned int)p[1]<<8); }
static int complete(const RtlEndpoints *ep, unsigned int seen, unsigned int expected) {
    return ep->bulk_in && ep->out_count && seen==expected;
}
int rtl_parse_config(const uint8_t *p, size_t length, RtlEndpoints *out)
{
    RtlEndpoints candidate;
    size_t offset, total;
    unsigned int seen=0,expected=0,active=0,addresses=0;
    if(!out) return RTL_E_ARGUMENT;
    memset(out,0,sizeof(*out));
    if(!p || length<9 || p[0]<9 || p[1]!=2) return RTL_E_DESCRIPTOR;
    total=le16(p+2);
    if(total<9 || total>length || total>4096 || p[0]>total || p[5]==0) return RTL_E_DESCRIPTOR;
    memset(&candidate,0,sizeof(candidate));
    for(offset=p[0];offset<total;) {
        unsigned int n,type;
        if(total-offset<2) return RTL_E_DESCRIPTOR;
        n=p[offset];type=p[offset+1];
        if(n<2 || n>total-offset) return RTL_E_DESCRIPTOR;
        if(type==4) {
            if(n<9) return RTL_E_DESCRIPTOR;
            if(active && complete(&candidate,seen,expected)) { *out=candidate; return RTL_OK; }
            memset(&candidate,0,sizeof(candidate)); seen=addresses=0;
            active=(p[offset+3]==0 && p[offset+5]==0xff);
            expected=p[offset+4];
            candidate.interface_no=p[offset+2];candidate.configuration=p[5];candidate.power_ma=(unsigned int)p[8]*2;
        } else if(type==5) {
            unsigned int addr,packet,bit;
            if(n<7) return RTL_E_DESCRIPTOR;
            if(active) {
                addr=p[offset+2];packet=le16(p+offset+4);
                bit=1u<<((addr&15)+((addr&128)?16:0));
                if(!(addr&15) || (addr&0x70) || (addresses&bit) || ++seen>expected) return RTL_E_DESCRIPTOR;
                addresses|=bit;
                if((p[offset+3]&3)==2) {
                    if(!packet || packet>512) return RTL_E_DESCRIPTOR;
                    if(addr&128) {
                        if(candidate.bulk_in) return RTL_E_DESCRIPTOR;
                        candidate.bulk_in=addr;candidate.bulk_in_packet=packet;
                    } else {
                        if(candidate.out_count==3) return RTL_E_DESCRIPTOR;
                        candidate.bulk_out[candidate.out_count]=addr;
                        candidate.bulk_out_packet[candidate.out_count++]=packet;
                    }
                }
            }
        }
        offset+=n;
    }
    if(active && complete(&candidate,seen,expected)) { *out=candidate;return RTL_OK; }
    return RTL_E_INTERFACE;
}
int rtl_read_register(RtlIo *io,unsigned int address,unsigned int width,uint32_t *value)
{
    uint8_t bytes[4]={0};
    RtlRequest req;
    int result;
    unsigned int i;
    if(!io || !io->control || !value || address>65535 || !(width==1||width==2||width==4) || address+width>65536)
        return RTL_E_ARGUMENT;
    req.type=0xc0;req.request=0x05;req.value=(uint16_t)address;req.request_index=0;req.length=(uint16_t)width;
    result=io->control(io->context,&req,bytes);
    if(result<0) return result;
    if((unsigned int)result!=width) return RTL_E_SHORT;
    *value=0;
    for(i=0;i<width;i++) *value|=(uint32_t)bytes[i]<<(i*8);
    return RTL_OK;
}
const char *rtl_error(int e) {
    switch(e) {
    case RTL_OK:return "OK";case RTL_E_ARGUMENT:return "BAD ARGUMENT";
    case RTL_E_DESCRIPTOR:return "BAD DESCRIPTOR";case RTL_E_INTERFACE:return "NO MATCHING BULK INTERFACE";
    case RTL_E_IO:return "USB ERROR";case RTL_E_SHORT:return "SHORT TRANSFER";
    case RTL_E_TIMEOUT:return "TIMEOUT";case RTL_E_DISCONNECTED:return "DISCONNECTED";
    case RTL_E_BUSY:return "TRANSFER PENDING";default:return "UNKNOWN ERROR";
    }
}
