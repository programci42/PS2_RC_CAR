#include "ps2_job_api.h"
#include "ps2_compile.h"
#include <ctype.h>
#define JOB_ARENA_SIZE (256*1024u)
#define JOB_UPLOAD_SIZE (64*1024u)
static unsigned char job_arena[JOB_ARENA_SIZE] __attribute__((aligned(64)));
static unsigned char job_upload[JOB_UPLOAD_SIZE] __attribute__((aligned(64)));
static unsigned char job_stack[16384] __attribute__((aligned(16)));
static char job_output[4096];static volatile unsigned job_output_length,job_done;
static unsigned job_size,job_received,job_started,job_entry,job_state;static int job_thread_id=-1,job_result;
static unsigned job_source_mode,job_compile_failed,job_code_bytes;
static unsigned job_u32(const unsigned char*p){return p[0]|(p[1]<<8)|(p[2]<<16)|((unsigned)p[3]<<24);}
static int job_pointer(const char*p){unsigned v=(unsigned)p;return (v>=(unsigned)job_arena&&v<(unsigned)job_arena+sizeof(job_arena))||(v>=(unsigned)job_stack&&v<(unsigned)job_stack+sizeof(job_stack));}
static void job_print(const char*p){unsigned n=job_output_length;if(!p)return;while(n+1<sizeof(job_output)&&job_pointer(p)&&*p){char c=*p++;job_output[n++]=(c=='\r')?' ':c;}job_output[n]=0;job_output_length=n;}
static void job_number(int v){char text[32];snprintf(text,sizeof(text),"%d",v);job_print(text);}
static void job_yield(unsigned n){DelayThread((n>100?100:n?n:1)*1000);}
static unsigned job_millis(void){return millis();}
static const Ps2JobApi job_api={1,job_print,job_number,job_millis,job_yield};
static void job_worker(void*unused){(void)unused;
 if(job_source_mode){char error[160];int n=ps2_compile((const char*)job_upload,job_source_mode==2,job_arena,sizeof(job_arena),(unsigned)job_arena,error,sizeof(error));
  if(n<0){job_compile_failed=1;job_result=-1;snprintf(job_output,sizeof(job_output),"Compile error: %s",error);job_output_length=strlen(job_output);job_done=1;SleepThread();return;}
  job_code_bytes=n;job_entry=(unsigned)job_arena;FlushCache(0);FlushCache(2);}
 job_result=((int(*)(const Ps2JobApi*))job_entry)(&job_api);job_done=1;SleepThread();}
static void job_stop(unsigned state){if(job_thread_id>=0){TerminateThread(job_thread_id);DeleteThread(job_thread_id);job_thread_id=-1;}job_state=state;}
static void job_tick(unsigned now){if(job_state==2){if(job_done)job_stop(job_compile_failed?7:3);else if((unsigned)(now-job_started)>=2000)job_stop(4);}}
static int job_load(void){unsigned count,i,offset,entry,base=(unsigned)job_arena,executable=0;
 if(job_received!=job_size||job_size<52||memcmp(job_upload,"\177ELF\1\1\1",7)||job_upload[16]!=2||job_upload[18]!=8||job_upload[42]!=32||job_upload[43])return -1;
 offset=job_u32(job_upload+28);count=job_upload[44]|(job_upload[45]<<8);entry=job_u32(job_upload+24);
 if(!count||count>16||offset>job_size||count*32>job_size-offset)return -1;
 /* Validate every segment before writing executable memory. */
 for(i=0;i<count;i++){const unsigned char*p=job_upload+offset+i*32;unsigned src=job_u32(p+4),addr=job_u32(p+8),file=job_u32(p+16),mem=job_u32(p+20);
  if(job_u32(p)!=1)continue;
  if(addr<base||mem>JOB_ARENA_SIZE||addr-base>JOB_ARENA_SIZE-mem||file>mem||src>job_size||file>job_size-src)return -1;
  if((job_u32(p+24)&1)&&entry>=addr&&entry-addr<file)executable=1;
 }
 if(!executable||(entry&3))return -1;
 memset(job_arena,0,sizeof(job_arena));
 for(i=0;i<count;i++){const unsigned char*p=job_upload+offset+i*32;if(job_u32(p)==1)memcpy((void*)job_u32(p+8),job_upload+job_u32(p+4),job_u32(p+16));}
 FlushCache(0);FlushCache(2);job_entry=entry;return 0;
}
static void job_json(char*out,unsigned cap){unsigned i,n=0;static char text[24576];
 for(i=0;i<job_output_length&&n+7<sizeof(text);i++){unsigned c=(unsigned char)job_output[i];if(c=='"'||c=='\\'){text[n++]='\\';text[n++]=c;}else if(c<32){n+=snprintf(text+n,sizeof(text)-n,"\\u%04x",c);}else text[n++]=c;}text[n]=0;
 snprintf(out,cap,"{\"state\":%u,\"result\":%d,\"received\":%u,\"size\":%u,\"arena\":%u,\"arenaSize\":%u,\"output\":\"%s\",\"timeoutMs\":2000,\"nativeCode\":true,\"compilerLocation\":\"PS2\",\"sourceLanguage\":%u,\"codeBytes\":%u}",job_state,job_result,job_received,job_size,(unsigned)job_arena,JOB_ARENA_SIZE,text,job_source_mode,job_code_bytes);
}
static int job_auth(const char*h){const char*p=h;const char*k="x-ps2-exec: 1";unsigned i;while((p=strstr(p,"\r\n"))!=0){p+=2;if(!strncmp(p,"\r\n",2))break;for(i=0;k[i]&&p[i]&&tolower((unsigned char)p[i])==k[i];i++);if(!k[i]&&p[i]=='\r')return 1;}return 0;}
static int job_http(const char*method,const char*target,const char*headers,const uint8_t*body,unsigned n,unsigned now,char*out,unsigned cap){unsigned value;ee_thread_t t;
 if(!strcmp(target,"/exec/status")||!strcmp(target,"/?exec=status")){if(strcmp(method,"GET"))return 405;job_json(out,cap);return 200;}
 if(strncmp(target,"/exec/",6))return 0;
 if(strcmp(method,"POST")||!job_auth(headers)||strstr(headers,"cross-site"))return 403;
 if(!strcmp(target,"/exec/cancel")){job_stop(5);job_json(out,cap);return 200;}
 if(job_state==2||robot.applied||robot.desired||wifi_files_busy(&file_service)||runtime.system_action)return 409;
 if(!strcmp(target,"/exec/source?lang=cpp")||!strcmp(target,"/exec/source?lang=asm")){
  if(!n||n>3072||memchr(body,0,n))return 400;
  memcpy(job_upload,body,n);job_upload[n]=0;job_source_mode=strstr(target,"lang=asm")?2:1;job_compile_failed=job_code_bytes=0;job_done=0;job_output_length=0;job_output[0]=0;job_size=job_received=n;job_state=1;goto start;
 }
 if(!strncmp(target,"/exec/begin?size=",17)){if(sscanf(target+17,"%u",&value)!=1||!value||value>JOB_UPLOAD_SIZE)return 400;job_size=value;job_received=0;job_state=1;job_output_length=0;job_output[0]=0;job_done=0;job_result=0;}
 else if(!strncmp(target,"/exec/chunk?offset=",19)){if(job_state!=1||sscanf(target+19,"%u",&value)!=1||value!=job_received||!n||n>2048||n>job_size-job_received)return 409;memcpy(job_upload+job_received,body,n);job_received+=n;}
 else if(!strcmp(target,"/exec/run")){if(job_state!=1||job_load())return 422;job_source_mode=job_compile_failed=0;
 start:
  memset(&t,0,sizeof(t));t.func=job_worker;t.stack=job_stack;t.stack_size=sizeof(job_stack);t.gp_reg=&_gp;t.initial_priority=96;
  job_thread_id=CreateThread(&t);if(job_thread_id<0)return 503;job_started=now;job_state=2;
  if(StartThread(job_thread_id,0)<0){job_stop(6);return 503;}
 }else return 404;
 job_json(out,cap);return 200;
}
