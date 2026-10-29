/* Bounded read-only CDVD SCMD clock RPC. No disc operations or clock writes. */
#include <time.h>
static SifRpcClientData_t clock_rpc __attribute__((aligned(64)));
static unsigned char clock_reply[64] __attribute__((aligned(64)));
static int bcd_(unsigned v){return (v>>4)*10+(v&15);}
static int64_t wifi_clock_read(void){unsigned i;struct tm t;time_t epoch;unsigned char*b;
 memset(&clock_rpc,0,sizeof(clock_rpc));
 if(SifBindRpc(&clock_rpc,0x80000593,SIF_RPC_M_NOWAIT)<0)return 0;
 for(i=0;i<300&&SifCheckStatRpc(&clock_rpc);i++)DelayThread(1000);
 if(i==300||!clock_rpc.server)return 0;
 memset(clock_reply,0,sizeof(clock_reply));
 SyncDCache(clock_reply,clock_reply+sizeof(clock_reply));
 if(SifCallRpc(&clock_rpc,1,SIF_RPC_M_NOWAIT,0,0,clock_reply,16,0,0)<0)return 0;
 for(i=0;i<300&&SifCheckStatRpc(&clock_rpc);i++)DelayThread(1000);
 if(i==300){return 0;}b=(unsigned char*)UNCACHED_SEG(clock_reply);
 if(*(int*)b<=0||b[4])return 0;
 memset(&t,0,sizeof(t));t.tm_sec=bcd_(b[5]);t.tm_min=bcd_(b[6]);t.tm_hour=bcd_(b[7]);t.tm_mday=bcd_(b[9]);t.tm_mon=bcd_(b[10]&0x7f)-1;t.tm_year=100+bcd_(b[11]);
 if(t.tm_sec>59||t.tm_min>59||t.tm_hour>23||t.tm_mday<1||t.tm_mday>31||t.tm_mon<0||t.tm_mon>11||t.tm_year>199)return 0;
 setenv("TZ","UTC0",1);tzset();epoch=mktime(&t);return epoch>0?(int64_t)epoch-9*3600:0;
}
