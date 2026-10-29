/* RAM-only device model: never opens a host file or a real PS2 memory card. */
#include "wifi_files.h"
#include "rtl_http.h"
#include "wifi_known.h"
#include "wifi_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define T(x) do{checks++;if(!(x)){printf("FILES_TEST FAIL line=%d %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct {char path[256];unsigned dir,size;unsigned char *data;} Node;
static Node nodes[96];static unsigned removed,writes,renames,fail_rename,fail_write,fail_remove,opened;
static WifiFiles fs;
static WifiRuntime *http_runtime;
static int find(const char*p){unsigned i;for(i=0;i<96;i++)if(!strcmp(nodes[i].path,p))return i;return -1;}
static int add(const char*p,int dir,const void*d,unsigned n){unsigned i;for(i=0;i<96&&nodes[i].path[0];i++);T(i<96);strcpy(nodes[i].path,p);nodes[i].dir=dir;nodes[i].size=n;nodes[i].data=malloc(n?n:1);T(nodes[i].data!=0);if(n)memcpy(nodes[i].data,d,n);return i;}
static int stat_(void*c,const char*p,WfEntry*e){int i=find(p);(void)c;if(i<0)return -1;memset(e,0,sizeof(*e));e->dir=nodes[i].dir;e->size=nodes[i].size;return 0;}
static int list_(void*c,const char*p,WfEntry*e,unsigned cap){unsigned i,n=0,len=strlen(p);(void)c;for(i=0;i<96;i++)if(nodes[i].path[0]&&!strncmp(nodes[i].path,p,len)){const char*s=nodes[i].path+len;if(*s=='/')s++;if(!*s||strchr(s,'/'))continue;if(n==cap)return -2;memset(e+n,0,sizeof(*e));strcpy(e[n].name,s);e[n].dir=nodes[i].dir;e[n++].size=nodes[i].size;}return n;}
static int open_(void*c,const char*p){int i=find(p);(void)c;if(i>=0)opened++;return i;}
static int close_(void*c,int fd){(void)c;(void)fd;T(opened>0);opened--;return 0;}
static int read_(void*c,int fd,uint32_t off,void*out,unsigned n){Node*a=nodes+fd;(void)c;if(off>a->size)return -1;if(n>a->size-off)n=a->size-off;memcpy(out,a->data+off,n);return n;}
static int write_(void*c,const char*p,uint32_t off,const void*d,unsigned n,int create){int i=find(p);Node*a;(void)c;writes++;if(fail_write)return -1;if(i<0){if(!create)return -1;i=add(p,0,0,0);}a=nodes+i;if(create)a->size=0;if(off>a->size)return -1;if(off+n>a->size){unsigned char*q=realloc(a->data,off+n);if(!q)return -1;a->data=q;a->size=off+n;}if(n)memcpy(a->data+off,d,n);return n;}
static int rename_(void*c,const char*a,const char*b){int i=find(a);(void)c;renames++;if(renames==fail_rename||i<0||find(b)>=0)return -1;strcpy(nodes[i].path,b);return 0;}
static int remove_(void*c,const char*p){int i=find(p);(void)c;removed++;if(fail_remove||i<0||nodes[i].dir)return -1;free(nodes[i].data);memset(nodes+i,0,sizeof(*nodes));return 0;}
static void setup(void){unsigned i;WfOps ops={0,stat_,list_,open_,read_,close_,write_,rename_,remove_};T(!opened);for(i=0;i<96;i++){free(nodes[i].data);memset(nodes+i,0,sizeof(*nodes));}removed=writes=renames=fail_rename=fail_write=0;
 fail_remove=0;add("mc0:/",1,0,0);add("mc1:/",1,0,0);add("mass:/",1,0,0);add("mc0:/Wifi",1,0,0);add("mc0:/Wifi/wifi.elf",0,"OLD",3);add("mc0:/Wifi/test.txt",0,"hello\n",6);wifi_files_init(&fs,&ops,"mc0:/Wifi/wifi.elf");}
static int request(const char*method,const char*target,const char*headers,const void*b,unsigned n,char*out,unsigned cap){WfReply r;unsigned off=0;int result=wifi_files_request(&fs,method,target,headers,b,n,100,&r),status;T(result==1);status=r.status;while(off<r.size){unsigned want=r.size-off;int got;if(want>73)want=73;T(off+want<cap);got=r.read(r.stream,off,out+off,want);T(got==(int)want);off+=got;}out[off]=0;r.close(r.stream);T(!fs.readers);return status;}
static const char auth[]="POST / HTTP/1.0\r\nX-PS2-Files: 1\r\n\r\n",get[]="GET / HTTP/1.0\r\n\r\n";
static char out[65536];
static int req(const char*t){return request("GET",t,get,0,0,out,sizeof(out));}
static int post(const char*t,const void*b,unsigned n){return request("POST",t,auth,b,n,out,sizeof(out));}
static uint32_t crc32(const unsigned char*p,unsigned n){uint32_t c=~0u;unsigned j;while(n--){c^=*p++;for(j=0;j<8;j++)c=c&1?(c>>1)^0xedb88320u:c>>1;}return ~c;}
static void put32(unsigned char*p,unsigned v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
static void elf(unsigned char*p){memset(p,0,128);memcpy(p,"\177ELF\1\1\1",7);p[16]=2;p[18]=8;put32(p+24,0x100000);put32(p+28,52);p[40]=52;p[42]=32;p[44]=1;put32(p+52,1);put32(p+56,84);put32(p+60,0x100000);put32(p+68,44);put32(p+72,48);put32(p+76,5);}
static void upload(const char*path,const unsigned char*b,unsigned n){char target[1024];unsigned off=0;snprintf(target,sizeof(target),"/files/upload/begin?path=%s&size=%u",path,n);T(post(target,0,0)==200);while(off<n){unsigned size=n-off;if(size>2048)size=2048;snprintf(target,sizeof(target),"/files/upload/chunk?offset=%u",off);T(post(target,b+off,size)==200);off+=size;}}
static int commit(const unsigned char*b,unsigned n){char target[128];int i=find(fs.upload.path);snprintf(target,sizeof(target),"/files/upload/commit?crc32=%08lX&pcbackup=%08lX",(unsigned long)crc32(b,n),(unsigned long)(i>=0?crc32(nodes[i].data,nodes[i].size):0));return post(target,0,0);}

/* Exercise the production TCP layer, with NUL bytes split across HTTP packets. */
static unsigned be16(const unsigned char*p){return p[0]*256u+p[1];}
static uint32_t be32(const unsigned char*p){return be16(p)*65536u+be16(p+2);}
static void big16(unsigned char*p,unsigned v){p[0]=v>>8;p[1]=v;}
static void big32(unsigned char*p,uint32_t v){big16(p,v>>16);big16(p+2,v);}
static unsigned checksum(const unsigned char*p,unsigned n,unsigned sum){unsigned i;for(i=0;i<n;i++)sum+=(i&1)?p[i]:p[i]*256u;while(sum>>16)sum=(sum&65535)+(sum>>16);return ~sum&65535;}
static const unsigned char mac[6]={2,1,2,3,4,5},ip[4]={192,0,2,25};
static unsigned packet(unsigned char*f,uint32_t seq,uint32_t ack,unsigned flags,const void*d,unsigned size){unsigned n=54+size;memset(f,0,n);memcpy(f,mac,6);f[6]=2;f[11]=9;big16(f+12,0x800);f[14]=0x45;big16(f+16,n-14);f[22]=64;f[23]=6;memcpy(f+26,"\xc0\x00\x02\x01",4);memcpy(f+30,ip,4);big16(f+24,checksum(f+14,20,0));big16(f+34,45000);big16(f+36,80);big32(f+38,seq);big32(f+42,ack);f[46]=0x50;f[47]=flags;big16(f+48,8192);if(size)memcpy(f+54,d,size);big16(f+50,checksum(f+34,20+size,be16(f+26)+be16(f+28)+be16(f+30)+be16(f+32)+6+20+size));return n;}
static void transport(const char*header,const void*data,unsigned size,unsigned blocked,int head){static RtlHttp h;unsigned char f[1514],tx[1514],saved[1514];unsigned n,i,at=0,hn=strlen(header);uint32_t seq=101,ack;int len;unsigned char all[4096];
 T(hn+size<=sizeof(all));memcpy(all,header,hn);if(size)memcpy(all+hn,data,size);rtl_http_init(&h,mac,ip,1);h.files=&fs;h.file_blocked=blocked;if(http_runtime){h.api=wifi_runtime_api;h.api_context=http_runtime;}n=packet(f,100,0,2,0,0);rtl_http_rx(&h,f,n,0);T(rtl_http_tx(&h,0,tx,sizeof(tx))==58);ack=be32(tx+38)+1;
 for(i=0;i<hn+size;){unsigned take=17;if(take>hn+size-i)take=hn+size-i;n=packet(f,seq,ack,24,all+i,take);rtl_http_rx(&h,f,n,1+i);seq+=take;i+=take;if(i<hn+size)T(!h.requests);}
 for(i=0;i<1024;i++){len=rtl_http_tx(&h,1000+i*2,tx,sizeof(tx));T(len>=54);T(at+(unsigned)len-54<sizeof(out));memcpy(out+at,tx+54,len-54);at+=len-54;if(!i){memcpy(saved,tx,len);T(rtl_http_tx(&h,2000,tx,sizeof(tx))==len&&!memcmp(saved,tx,len));}ack=be32(tx+38)+len-54+!!(tx[47]&1);n=packet(f,seq,ack,16,0,0);rtl_http_rx(&h,f,n,1001+i*2);if(h.responses)break;}
 out[at]=0;T(h.responses==1);if(head){char*body=strstr(out,"\r\n\r\n");T(body&&body[4]==0);}rtl_http_close_all(&h);T(!fs.readers&&!opened);}

int files_test(void){char path[WF_PATH],target[1024];unsigned i,before;unsigned char image[128],binary[10000];setup();
 T(req("/files/info")==200&&strstr(out,"\"pcBackupRequired\":1"));
 T(!wifi_files_path("mc0",path)&&!strcmp(path,"mc0:/"));T(!wifi_files_path("mass0:/a%20b",path)&&!strcmp(path,"mass:/a b"));
 {const char*bad[]={"host:/x","mc0:/../BOOT","mc1:/a/%2e%2e/b","mass:/x%00y","mc0:/a%5Cb","mc0:/a:b","mc0:/a*","mc0:/%ZZ","mc0:/12345678901234567890123456789012"};for(i=0;i<sizeof(bad)/sizeof(*bad);i++)T(wifi_files_path(bad[i],path)<0);}
 T(req("/?ls=mc0")==200&&strstr(out,"test.txt")&&strstr(out,"wifi.elf"));T(req("/?ls=mc1")==200);T(req("/?ls=mass")==200);
 add("mc0:/Wifi/<x>&.txt",0,"safe",4);T(req("/?ls=mc0")==200&&strstr(out,"&lt;x&gt;&amp;.txt")&&!strstr(out,"<x>"));
 T(req("/?view=mc0:/Wifi/test.txt")==200&&!strcmp(out,"hello\n"));
 for(i=0;i<sizeof(binary);i++){binary[i]=i;}add("mass:/binary.bin",0,binary,sizeof(binary));T(req("/?download=mass:/binary.bin")==200&&!memcmp(out,binary,sizeof(binary)));T(req("/?view=mass:/binary.bin")==200&&strstr(out,"00000000  00 01 02 03")&&strstr(out,"00002700"));
 add("mc1:/pic.png",0,"PNG",3);T(req("/?view=mc1:/pic.png")==200&&strstr(out,"<img src=\"/?raw=mc1:/pic.png\""));add("mass:/clip.mp4",0,"MP4",3);T(req("/?view=mass:/clip.mp4")==200&&strstr(out,"<video")&&strstr(out,"controls"));
 {WfReply r;T(wifi_files_request(&fs,"GET","/?download=mass:/binary.bin","GET / HTTP/1.0\r\nRange: bytes=99-199\r\n\r\n",0,0,0,&r)==1);T(r.status==206&&r.size==101&&strstr(r.extra,"99-199/10000"));T(r.read(r.stream,0,out,101)==101&&!memcmp(out,binary+99,101));r.close(r.stream);T(!fs.readers);}
 T(request("GET","/?raw=mass:/binary.bin","GET / HTTP/1.0\r\nRange: bytes=-10\r\n\r\n",0,0,out,sizeof(out))==206&&!memcmp(out,binary+9990,10));T(request("GET","/?raw=mass:/binary.bin","GET / HTTP/1.0\r\nRange: bytes=10000-\r\n\r\n",0,0,out,sizeof(out))==416);
 before=removed;{const char*safe[]={"mc0:/BOOT/BOOT.ELF","mc0:/SYS-CONF/FREEMCB.CNF","mc1:/BEEXEC-SYSTEM/osdmain.elf","mc0:/Wifi/SEED.DAT","mc0:/Wifi/Wifi.seed","mc0:/Wifi/NET0.DAT","mc0:/Wifi/wifi.elf","mc0:/Wifi"};for(i=0;i<sizeof(safe)/sizeof(*safe);i++){snprintf(target,sizeof(target),"/?del=%s",safe[i]);T(req(target)==403);}}T(removed==before);
 T(request("HEAD","/?del=mc0:/Wifi/test.txt",get,0,0,out,sizeof(out))==405&&find("mc0:/Wifi/test.txt")>=0);T(request("GET","/?del=mc0:/Wifi/test.txt","GET / HTTP/1.0\r\nSec-Fetch-Site: cross-site\r\n\r\n",0,0,out,sizeof(out))==403);T(req("/?del=mc0:/Wifi/test.txt")==200&&find("mc0:/Wifi/test.txt")<0&&removed==before+1);
 T(request("POST","/files/upload/begin?path=mc0:/Wifi/new.txt&size=4",get,0,0,out,sizeof(out))==403);T(post("/files/upload/begin?path=mc0:/BOOT/BOOT.ELF&size=4",0,0)==403);
 elf(image);upload("mc0:/Wifi/WIFI.ELF",image,sizeof(image));T(!strcmp(fs.upload.path,"mc0:/Wifi/wifi.elf"));T(post("/files/upload/chunk?offset=0",image,sizeof(image))==200);image[127]=1;T(post("/files/upload/chunk?offset=0",image,sizeof(image))==409);image[127]=0;
 T(post("/files/upload/commit?crc32=00000000",0,0)==422&&find("mc0:/Wifi/wifi.elf")>=0);
 snprintf(target,sizeof(target),"/files/upload/commit?crc32=%08lX",(unsigned long)crc32(image,sizeof(image)));before=removed;T(post(target,0,0)==428&&removed==before);
 strcat(target,"&pcbackup=00000000");T(post(target,0,0)==409&&removed==before&&nodes[find("mc0:/Wifi/wifi.elf")].size==3);
 fail_remove=1;T(commit(image,sizeof(image))==500&&nodes[find("mc0:/Wifi/wifi.elf")].size==3);fail_remove=0;
 T(commit(image,sizeof(image))==200);T(!fs.upload.active&&find("mc0:/Wifi/wifi.elf.bak0")<0);i=find("mc0:/Wifi/wifi.elf");T(nodes[i].size==128&&!memcmp(nodes[i].data,image,128));
 upload("mass:/new.bin",binary,sizeof(binary));T(commit(binary,sizeof(binary))==200);T(!memcmp(nodes[find("mass:/new.bin")].data,binary,sizeof(binary)));
 setup();upload("mc0:/Wifi/wifi.elf",binary,128);T(commit(binary,128)==422&&nodes[find("mc0:/Wifi/wifi.elf")].size==3);T(post("/files/upload/abort",0,0)==200&&find("mc0:/Wifi/wifi.elf.part")<0);
 upload("mc0:/Wifi/wifi.elf",image,128);fail_rename=renames+1;T(commit(image,128)==500);T(find("mc0:/Wifi/wifi.elf")<0&&find("mc0:/Wifi/wifi.elf.part")>=0&&find("mc0:/Wifi/wifi.elf.bak0")<0);T(commit(image,128)==200&&find("mc0:/Wifi/wifi.elf")>=0);
 setup();
 T(post("/files/upload/begin?path=mc0:/Wifi/wifi.elf&size=128",0,0)==200);fail_write=1;T(post("/files/upload/chunk?offset=0",image,128)==507&&fs.upload.received==0);fail_write=0;T(post("/files/upload/chunk?offset=0",image,128)==200);nodes[find(fs.upload.stage)].data[100]^=1;T(commit(image,128)==422&&nodes[find("mc0:/Wifi/wifi.elf")].size==3);wifi_files_tick(&fs,120101);T(!fs.upload.active&&find("mc0:/Wifi/wifi.elf.part")>=0);T(post("/files/upload/begin?path=mc0:/Wifi/wifi.elf&size=128",0,0)==409);
 setup();transport("GET /?ls=mc0 HTTP/1.1\r\n\r\n",0,0,0,0);T(strstr(out,"200 OK")&&strstr(out,"test.txt"));transport("GET /?del=mc0:/Wifi/test.txt HTTP/1.1\r\n\r\n",0,0,1,0);T(strstr(out,"409 Conflict")&&!removed);transport("HEAD /?download=mc0:/Wifi/test.txt HTTP/1.1\r\n\r\n",0,0,0,1);T(strstr(out,"Content-Length: 6\r\n"));
 add("mass:/large.bin",0,binary,sizeof(binary));transport("GET /?download=mass:/large.bin HTTP/1.1\r\n\r\n",0,0,0,0);{char*body=strstr(out,"\r\n\r\n");T(body&&!memcmp(body+4,binary,sizeof(binary)));}
 T(post("/files/upload/begin?path=mass:/zero.bin&size=128",0,0)==200);memset(image,0,128);image[127]=254;transport("POST /files/upload/chunk?offset=0 HTTP/1.1\r\nX-PS2-Files: 1\r\nContent-Length: 128\r\n\r\n",image,128,0,0);T(strstr(out,"200 OK")&&fs.upload.received==128);T(commit(image,128)==200&&!memcmp(nodes[find("mass:/zero.bin")].data,image,128));
 add("mass:/README",0,"plain text\n",11);T(req("/?view=mass:/README")==200&&!strcmp(out,"plain text\n"));
 T(post("/files/upload/begin?path=mass:/empty&size=0",0,0)==200);T(commit(image,0)==200&&nodes[find("mass:/empty")].size==0);
 setup();elf(image);for(i=0;i<10;i++){snprintf(target,sizeof(target),"mc0:/Wifi/wifi.elf.bak%u",i);add(target,0,"kept",4);}upload("mc0:/Wifi/wifi.elf",image,128);before=renames;T(commit(image,128)==200&&renames==before+1&&nodes[find("mc0:/Wifi/wifi.elf")].size==128);T(find("mc0:/Wifi/wifi.elf.bak10")<0);
 setup();{const char*original="#keep\r\nIEEE;password\r\nOther;another-password\r\n";uint8_t last[4]={192,168,1,43},manual[4]={192,168,1,99};unsigned wr;
  add("mc0:/Wifi/bilinenaglar.csv",0,original,strlen(original));T(!wifi_known_save(&fs,"mc0:/Wifi/bilinenaglar.csv",(const uint8_t*)"IEEE",4,last,manual));
  i=find("mc0:/Wifi/bilinenaglar.csv");T(nodes[i].size>strlen(original)&&!memcmp(nodes[i].data,"#keep\r\nIEEE;password;",21));T(find("mc0:/Wifi/bilinenaglar.csv.ipbak")>=0);wr=writes;T(!wifi_known_save(&fs,"mc0:/Wifi/bilinenaglar.csv",(const uint8_t*)"IEEE",4,last,manual)&&writes==wr);
  last[3]=44;fail_write=1;T(wifi_known_save(&fs,"mc0:/Wifi/bilinenaglar.csv",(const uint8_t*)"IEEE",4,last,manual)<0);T(nodes[i].size>0);fail_write=0;fail_rename=renames+2;T(wifi_known_save(&fs,"mc0:/Wifi/bilinenaglar.csv",(const uint8_t*)"IEEE",4,last,manual)<0);T(find("mc0:/Wifi/bilinenaglar.csv")>=0);
 }
 setup();{WifiRuntime rt,reloaded;RobotControl robot;char json[3501],reply[4096];int length;const char*header="POST /settings HTTP/1.0\r\nX-PS2-Control: 1\r\n\r\n";unsigned deletes;
  robot_init(&robot);wifi_runtime_init(&rt,&fs,&robot,0);T(!strcmp(rt.settings,"mc0:/Wifi/ayarlar.json")&&!strcmp(rt.log,"mc0:/Wifi/wifi.log")&&robot.program_mode);
  rt.program.config.left=2;rt.program.config.right=3;length=rp_json(&rt.program.config,json,sizeof(json));T(length>0);
  T(wifi_runtime_api(&rt,"POST","/settings",get,(uint8_t*)json,length,0,reply,sizeof(reply))==403);
  robot.applied=5;T(wifi_runtime_api(&rt,"POST","/settings",header,(uint8_t*)json,length,0,reply,sizeof(reply))==409);robot.applied=0;
  T(wifi_runtime_api(&rt,"POST","/settings",header,(uint8_t*)json,length,0,reply,sizeof(reply))==200&&find(rt.settings)>=0);
  wifi_runtime_init(&reloaded,&fs,&robot,0);T(reloaded.program.config.left==2&&reloaded.program.config.right==3&&!reloaded.config_error);
  http_runtime=&rt;transport("GET /settings HTTP/1.1\r\n\r\n",0,0,0,0);T(strstr(out,"200 OK")&&strstr(out,"\"bindings\""));http_runtime=0;
  {int id=find(rt.settings);add("mc0:/Wifi/ayarlar.json.bak",0,nodes[id].data,nodes[id].size);nodes[id].data[0]='!';wifi_runtime_init(&reloaded,&fs,&robot,0);T(reloaded.config_error&&reloaded.program.config.left==2);nodes[id].data[0]='{';}
  wifi_runtime_clock(&rt,1704067200,0);wifi_runtime_error(&rt,1000,"USB error\r\nfake entry");wifi_runtime_tick(&rt,2000);T(!rt.queued&&!rt.log_failed&&rt.log_bytes>0);
  T(req("/?view=mc0:/Wifi/wifi.log")==200&&strstr(out,"2024-01-01T00:00:01Z")&&strstr(out,"USB error  fake entry"));
  i=find(rt.log);nodes[i].size=WIFI_LOG_LIMIT;wifi_runtime_tick(&rt,3599999);T(!rt.log_warning);deletes=removed;wifi_runtime_tick(&rt,3600000);T(rt.log_warning&&rt.log_bytes==WIFI_LOG_LIMIT&&removed==deletes);
  T(wifi_runtime_api(&rt,"GET","/telemetry",get,0,0,3600001,reply,sizeof(reply))==200&&strstr(reply,"\"logWarning\":1")&&strstr(reply,"\"battery\":null"));
  robot.applied=1;for(i=0;i<20;i++)wifi_runtime_error(&rt,3600002,"deferred fault");wifi_runtime_tick(&rt,3602000);T(rt.queued==16&&rt.dropped==4);robot.applied=0;wifi_runtime_tick(&rt,3602001);T(!rt.queued&&rt.log_warning&&removed==deletes);
  T(wifi_runtime_api(&rt,"POST","/settings",header,(uint8_t*)"{}",2,3603000,reply,sizeof(reply))==422);
  fail_write=1;T(wifi_runtime_api(&rt,"POST","/settings",header,(uint8_t*)json,length,3603001,reply,sizeof(reply))==507&&rt.queued);fail_write=0;
 }
 setup();{WifiRuntime rt,loaded;RobotControl robot;char reply[4096];const char*b="{\"capacityMah\":3300,\"motors\":4,\"baseMw\":18000,\"motorMa\":2000,\"effPermille\":900,\"remainingUwh\":36630000}";const char*hdr="POST /battery HTTP/1.1\r\nX-PS2-Control: 1\r\n\r\n";unsigned before;
 robot_init(&robot);wifi_runtime_init(&rt,&fs,&robot,0);T(!rt.battery_set&&rt.battery_capacity==3300&&rt.battery_motors==4);
 T(wifi_runtime_api(&rt,"POST","/battery",get,(const uint8_t*)b,strlen(b),0,reply,sizeof(reply))==403);
 robot.applied=5;T(wifi_runtime_api(&rt,"POST","/battery",hdr,(const uint8_t*)b,strlen(b),0,reply,sizeof(reply))==409);robot.applied=0;
 T(wifi_runtime_api(&rt,"POST","/battery",hdr,(const uint8_t*)b,strlen(b),0,reply,sizeof(reply))==200&&rt.battery_set);
 before=writes;robot.applied=5;wifi_runtime_energy(&rt,100);T(rt.battery_power_mw==110133&&rt.battery_motor_100ms_uwh==616&&rt.battery_uwh==36626941&&writes==before);
 robot.applied=0;wifi_runtime_energy(&rt,200);T(rt.battery_power_mw==20555&&rt.battery_uwh<36626941);
 T(wifi_runtime_api(&rt,"GET","/?reload=1",get,0,0,200,reply,sizeof(reply))==200&&rt.reload_pending);rt.reload_pending=0;
 robot.applied=1;T(wifi_runtime_api(&rt,"GET","/?reload=1",get,0,0,200,reply,sizeof(reply))==409&&!rt.reload_pending);robot.applied=0;
 wifi_runtime_tick(&rt,300000);T(!rt.battery_dirty&&writes>before);wifi_runtime_init(&loaded,&fs,&robot,300000);T(loaded.battery_set&&loaded.battery_uwh==rt.battery_uwh);
 T(wifi_runtime_api(&rt,"GET","/battery",get,0,0,300000,reply,sizeof(reply))==200&&strstr(reply,"\"configured\":1"));
 T(wifi_runtime_api(&rt,"POST","/battery",hdr,(const uint8_t*)"{}",2,300000,reply,sizeof(reply))==422);
 }
 setup();{WifiRuntime rt,loaded;RobotControl robot;char result[4096];robot_init(&robot);wifi_runtime_init(&rt,&fs,&robot,0);robot.ready=1;
  T(wifi_runtime_api(&rt,"GET","/?git=ileri10",get,0,0,100,result,sizeof(result))==200&&robot.timed&&robot.timed_until==10100);robot_tick(&robot,0,1,1,10100);T(!robot.desired&&!robot.timed);
  T(wifi_runtime_api(&rt,"GET","/?git=sag5",get,0,0,20000,result,sizeof(result))==200&&robot.web==ROBOT_RIGHT);robot_tick(&robot,0,1,0,20001);T(!robot.web);
  T(wifi_runtime_api(&rt,"GET","/?git=ileri-5",get,0,0,0,result,sizeof(result))==400);T(wifi_runtime_api(&rt,"GET","/?git=sol999999999999",get,0,0,0,result,sizeof(result))==400);
  T(wifi_runtime_api(&rt,"GET","/?shutdown=-r-t4",get,0,0,100,result,sizeof(result))==200&&rt.system_action==2&&rt.system_due==4100&&robot.system_locked&&!robot.web);
  T(wifi_runtime_api(&rt,"GET","/?reload=1",get,0,0,200,result,sizeof(result))==409);
  T(wifi_runtime_api(&rt,"GET","/?shutdown=-s",get,0,0,300,result,sizeof(result))==200&&rt.system_action==1&&rt.system_due==300);
  T(wifi_runtime_api(&rt,"GET","/?shutdown=-s-t",get,0,0,300,result,sizeof(result))==400);T(wifi_runtime_api(&rt,"GET","/?shutdown=-x-t3",get,0,0,300,result,sizeof(result))==400);
  T(!wifi_runtime_channel(&rt,6));wifi_runtime_init(&loaded,&fs,&robot,0);T(loaded.last_channel==6);T(wifi_runtime_channel(&rt,14)<0);
  T(wifi_runtime_api(&rt,"GET","/?display=1",get,0,0,400,result,sizeof(result))==200&&rt.display_requested&&rt.display_enabled);
  rt.pad_connected=1;rt.pad_buttons=0x300;T(wifi_runtime_api(&rt,"GET","/display",get,0,0,400,result,sizeof(result))==200&&strstr(result,"\"L2\":1")&&strstr(result,"\"R2\":1"));
  T(wifi_runtime_api(&rt,"GET","/?display=0",get,0,0,400,result,sizeof(result))==200&&!rt.display_enabled);
 }
 T(!opened&&!fs.readers);setup();printf("FILES_TEST PASS checks=%u RAM-only tree-text-hex-media-range-TCP-NUL-retry-upload-CRC-ELF-PC-backup-proof-no-card-backup-commit-retry-boot-protection-CSV-settings-log-hourly-warning-energy-reload-timed-shutdown-channel\n",checks);return 0;
}
