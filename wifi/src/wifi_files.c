#include "wifi_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
enum {WF_LIMIT=2*1024*1024,WF_ENTRIES=4096};
typedef struct {WifiFiles*f;char *memory;uint32_t size,start;int fd;unsigned hex,tracked;} Stream;
static int same(const char*a,const char*b){while(*a&&*b&&tolower((unsigned char)*a)==tolower((unsigned char)*b)){a++;b++;}return !*a&&!*b;}
static int nib(char c){if(c>='0'&&c<='9'){return c-'0';}c=tolower((unsigned char)c);return c>='a'&&c<='f'?c-'a'+10:-1;}
static int decode(const char*s,char*out,unsigned cap){unsigned n=0;while(*s){unsigned char c=*s++;
 if(c=='%'){int a,b;if(!s[0]||!s[1]||(a=nib(s[0]))<0||(b=nib(s[1]))<0){return -1;}c=(a<<4)|b;s+=2;}else if(c=='+')c=' ';
 if(c<32||c==127||n+1>=cap){return -1;}out[n++]=c;}out[n]=0;return 0;}
int wifi_files_path(const char*encoded,char out[WF_PATH]){char raw[WF_PATH];const char*p;unsigned used,part;
 if(decode(encoded,raw,sizeof(raw))){return -1;}
 if(!strncmp(raw,"mc0",3)&&(raw[3]==':'||!raw[3])){strcpy(out,"mc0:/");p=raw+3;}
 else if(!strncmp(raw,"mc1",3)&&(raw[3]==':'||!raw[3])){strcpy(out,"mc1:/");p=raw+3;}
 else if(!strncmp(raw,"mass0",5)&&(raw[5]==':'||!raw[5])){strcpy(out,"mass:/");p=raw+5;}
 else if(!strncmp(raw,"mass",4)&&(raw[4]==':'||!raw[4])){strcpy(out,"mass:/");p=raw+4;}else return -1;
 if(*p==':'){p++;}used=strlen(out);
 while(*p){while(*p=='/')p++;if(!*p)break;part=0;
  if(used&&out[used-1]!='/')out[used++]='/';
  while(*p&&*p!='/'){unsigned char c=*p++;if(c=='\\'||c==':'||c=='*'||c=='?'||used+1>=WF_PATH){return -1;}out[used++]=c;part++;}
  out[used]=0;if((part==1&&out[used-1]=='.')||(part==2&&out[used-1]=='.'&&out[used-2]=='.')){return -1;}
  if(out[0]=='m'&&out[1]=='c'&&part>31){return -1;}
 }out[used]=0;return 0;}
int wifi_files_protected(const WifiFiles*f,const char*p,int deleting){const char*base=strrchr(p,'/');char first[64];unsigned n=0;const char*s=strchr(p,':');
 if(!base||!base[1]){return 1;}base++;
 if(same(base,"Wifi.seed")||same(base,"SEED.DAT")||same(base,"NET0.DAT")||same(base,"NET1.DAT")){return 1;}
 if(deleting&&same(p,f->running)){return 1;}
 if(strncmp(p,"mc0:",4)&&strncmp(p,"mc1:",4)){return 0;}
 s++;while(*s=='/')s++;while(*s&&*s!='/'&&n+1<sizeof(first))first[n++]=toupper((unsigned char)*s++);first[n]=0;
 if(!strcmp(first,"BOOT")||!strcmp(first,"SYS-CONF")||!strcmp(first,"FMCB")||!strcmp(first,"FREEMCB.CNF")||!strcmp(first,"OSDSYS")){return 1;}
 if(n>=3&&first[0]=='B'&&(strstr(first,"EXEC-SYSTEM")||strstr(first,"EXEC-DVDPLAYER"))){return 1;}
 return 0;}
static uint32_t crc(uint32_t v,const uint8_t*p,unsigned n){unsigned j;while(n--){v^=*p++;for(j=0;j<8;j++)v=(v>>1)^(0xedb88320u&-(int)(v&1));}return v;}
static int append(char**p,unsigned*used,unsigned*cap,const char*format,...){va_list a;int n;char text[4096];va_start(a,format);n=vsnprintf(text,sizeof(text),format,a);va_end(a);if(n<0||(unsigned)n>=sizeof(text)||*used+(unsigned)n>=WF_LIMIT){return -1;}
 if(*used+n+1>*cap){unsigned size=*cap?*cap:4096;char*q;while(size<*used+n+1)size*=2;q=realloc(*p,size);if(!q){return -1;}*p=q;*cap=size;}memcpy(*p+*used,text,n+1);*used+=n;return 0;}
static void escape(const char*s,char*out,unsigned cap,int url){static const char h[]="0123456789ABCDEF";unsigned n=0;while(*s&&n+7<cap){unsigned char c=*s++;const char*t=0;
 if(url){if(isalnum(c)||c=='/'||c==':'||c=='-'||c=='_'||c=='.')out[n++]=c;else{out[n++]='%';out[n++]=h[c>>4];out[n++]=h[c&15];}continue;}
 if(c=='&')t="&amp;";else if(c=='<')t="&lt;";else if(c=='>')t="&gt;";else if(c=='\"')t="&quot;";
 if(t){unsigned z=strlen(t);memcpy(out+n,t,z);n+=z;}else out[n++]=c;}out[n]=0;}
static int read_stream(void*ctx,uint32_t off,void*out,unsigned n){Stream*s=ctx;if(off>=s->size){return 0;}if(n>s->size-off)n=s->size-off;
 if(s->hex&&n>512)n=512;
 if(s->memory){memcpy(out,s->memory+off,n);return n;}
 if(!s->hex){return s->f->io.read(s->f->io.ctx,s->fd,s->start+off,out,n);}
 {uint8_t bytes[256];char row[79];unsigned first=off/78,last=(off+n-1)/78,need=(last-first+1)*16,i,done=0;int got;
  got=s->f->io.read(s->f->io.ctx,s->fd,first*16,bytes,need);if(got<0){return -1;}
  for(i=first;i<=last;i++){unsigned j,k=10,start=i==first?off%78:0,count;snprintf(row,sizeof(row),"%08lX  ",(unsigned long)i*16);
   for(j=0;j<16;j++){unsigned pos=(i-first)*16+j;if(pos<(unsigned)got){snprintf(row+k,4,"%02X ",bytes[pos]);}else memcpy(row+k,"   ",3);k+=3;}
   row[k++]=' ';row[k++]='|';for(j=0;j<16;j++){unsigned pos=(i-first)*16+j;row[k++]=pos<(unsigned)got?(bytes[pos]>=32&&bytes[pos]<127?bytes[pos]:'.'):' ';}row[k++]='|';row[k++]='\n';
   count=78-start;if(count>n-done)count=n-done;memcpy((char*)out+done,row+start,count);done+=count;
  }return done;
 }}
static void progress_stream(void*ctx,uint32_t done){Stream*s=ctx;if(s->tracked==s->f->download.generation){s->f->download.done=done>s->size?s->size:done;}}
static void close_stream(void*ctx){Stream*s=ctx;if(s->tracked&&s->tracked==s->f->download.generation)s->f->download.active=0;if(s->fd>=0)s->f->io.close(s->f->io.ctx,s->fd);if(s->f->readers)s->f->readers--;free(s->memory);free(s);}
static int memory_reply(WifiFiles*f,WfReply*r,int status,const char*type,char*data,unsigned n){Stream*s=calloc(1,sizeof(*s));if(!s){free(data);return -1;}s->f=f;s->memory=data;s->fd=-1;s->size=n;f->readers++;r->status=status;snprintf(r->type,sizeof(r->type),"%s",type);r->size=n;r->stream=s;r->read=read_stream;r->close=close_stream;return 1;}
static int text(WifiFiles*f,WfReply*r,int status,const char*message){unsigned n=strlen(message);char*p=malloc(n+1);if(!p){return -1;}memcpy(p,message,n+1);return memory_reply(f,r,status,"text/plain; charset=utf-8",p,n);}
static int query(const char*target,const char*key,char*out,unsigned cap){const char*p=strchr(target,'?');unsigned len=strlen(key),n;if(!p){return 0;}p++;
 while(*p){const char*end=strchr(p,'&');if(!end)end=p+strlen(p);if((unsigned)(end-p)>len&&p[len]=='='&&!memcmp(p,key,len)){n=end-p-len-1;if(n>=cap){return -1;}memcpy(out,p+len+1,n);out[n]=0;return 1;}p=*end?end+1:end;}return 0;}
static int tree(WifiFiles*f,const char*path,unsigned depth,unsigned*total,char**out,unsigned*used,unsigned*cap){WfEntry*entries;unsigned capacity=64;int count,i;char escaped[1600],url[WF_PATH*3+1],child[WF_PATH];
 if(depth>32){return -1;}
 for(;;){entries=calloc(capacity,sizeof(*entries));if(!entries){return -1;}count=f->io.list(f->io.ctx,path,entries,capacity);if(count!=-2||capacity==WF_ENTRIES)break;free(entries);capacity*=2;}
 if(count<0){free(entries);return -1;}
 for(i=0;i<count;i++){unsigned j;if(!strcmp(entries[i].name,".")||!strcmp(entries[i].name,".."))continue;if(++*total>WF_ENTRIES){free(entries);return -1;}
  if(snprintf(child,sizeof(child),"%s%s%s",path,path[strlen(path)-1]=='/'?"":"/",entries[i].name)>=(int)sizeof(child)){free(entries);return -1;}
  escape(child,escaped,sizeof(escaped),0);escape(child,url,sizeof(url),1);
  for(j=0;j<depth;j++)if(append(out,used,cap,"  ")){goto bad;}
  if(entries[i].dir){if(append(out,used,cap,"+ %s/\n",escaped)||tree(f,child,depth+1,total,out,used,cap)){goto bad;}}
  else if(append(out,used,cap,"- <a href=\"/?view=%s\">%s</a> (%lu B) <a href=\"/?download=%s\">download</a>\n",url,escaped,(unsigned long)entries[i].size,url)){goto bad;}
 }free(entries);return 0;
bad:free(entries);return -1;}
static const char*mime(const char*path){const char*p=strrchr(path,'.');if(!p){return "application/octet-stream";}
 if(same(p,".png")){return "image/png";}if(same(p,".jpg")||same(p,".jpeg")){return "image/jpeg";}if(same(p,".gif")){return "image/gif";}if(same(p,".webp")){return "image/webp";}if(same(p,".bmp")){return "image/bmp";}
 if(same(p,".mp4")){return "video/mp4";}if(same(p,".webm")){return "video/webm";}if(same(p,".ogv")){return "video/ogg";}if(same(p,".mp3")){return "audio/mpeg";}if(same(p,".wav")){return "audio/wav";}
 if(same(p,".mov")){return "video/quicktime";}if(same(p,".avi")){return "video/x-msvideo";}if(same(p,".mkv")){return "video/x-matroska";}
 if(same(p,".txt")||same(p,".csv")||same(p,".json")||same(p,".ini")||same(p,".cfg")||same(p,".cnf")||same(p,".log")||same(p,".xml")||same(p,".html")||same(p,".js")||same(p,".c")||same(p,".h")||same(p,".md")){return "text/plain; charset=utf-8";}
 return "application/octet-stream";}
static const char*header(const char*request,const char*key){const char*p=request;unsigned n=strlen(key),i;while((p=strstr(p,"\r\n"))!=0){p+=2;if(!strncmp(p,"\r\n",2))break;for(i=0;i<n&&p[i]&&tolower((unsigned char)p[i])==key[i];i++);if(i==n){p+=n;while(*p==' ')p++;return p;}}return 0;}
static int looks_text(WifiFiles*f,const char*path){uint8_t b[512];int fd=f->io.open(f->io.ctx,path),n;unsigned i=0;
 if(fd<0){return 0;}n=f->io.read(f->io.ctx,fd,0,b,sizeof(b));f->io.close(f->io.ctx,fd);if(n<0){return 0;}
 while(i<(unsigned)n){unsigned c=b[i++],need,j;if(c<128){if(c<32&&c!='\t'&&c!='\r'&&c!='\n'){return 0;}if(c==127){return 0;}continue;}
  need=c>=0xc2&&c<=0xdf?1:c>=0xe0&&c<=0xef?2:c>=0xf0&&c<=0xf4?3:0;if(!need){return 0;}
  if(i+need>(unsigned)n){return n==(int)sizeof(b);}
  if((c==0xe0&&b[i]<0xa0)||(c==0xed&&b[i]>=0xa0)||(c==0xf0&&b[i]<0x90)||(c==0xf4&&b[i]>=0x90)){return 0;}
  for(j=0;j<need;j++)if((b[i++]&0xc0)!=0x80){return 0;}
 }return 1;
}
static int stream_file(WifiFiles*f,WfReply*r,const char*path,const char*headers,unsigned hex,unsigned attach){WfEntry e;Stream*s;uint32_t begin=0,end;const char*range;
 if(f->io.stat(f->io.ctx,path,&e)<0||e.dir){return text(f,r,404,"File not found.");}
 if(e.size>0x7fffffffu){return text(f,r,413,"File exceeds 2 GiB limit.");}
 s=calloc(1,sizeof(*s));if(!s){return -1;}s->f=f;s->fd=f->io.open(f->io.ctx,path);if(s->fd<0){free(s);return text(f,r,503,"Cannot open file.");}s->hex=hex;s->size=hex?((e.size+15)/16)*78:e.size;end=s->size?s->size-1:0;
 if(hex&&e.size>400000000u){f->io.close(f->io.ctx,s->fd);free(s);return text(f,r,413,"Binary view too large; use download.");}
 r->status=200;range=header(headers,"range:");
 if(range&&!hex){char*tail;unsigned long a,b;if(strncmp(range,"bytes=",6)){goto invalid;}range+=6;
  if(*range=='-'){b=strtoul(range+1,&tail,10);if(!b||(*tail!='\r')){goto invalid;}begin=b>=e.size?0:e.size-b;}
  else{a=strtoul(range,&tail,10);if(tail==range||*tail!='-'||a>=e.size){goto invalid;}begin=a;range=tail+1;if(*range!='\r'){b=strtoul(range,&tail,10);if(tail==range||*tail!='\r'||b<a){goto invalid;}if(b<end)end=b;}}
  if(!e.size){goto invalid;}r->status=206;snprintf(r->extra,sizeof(r->extra),"Content-Range: bytes %lu-%lu/%lu\r\n",(unsigned long)begin,(unsigned long)end,(unsigned long)e.size);
 }
 s->start=begin;if(!hex)s->size=e.size?end-begin+1:0;r->size=s->size;r->stream=s;r->read=read_stream;r->close=close_stream;f->readers++;
 s->tracked=++f->download.generation;f->download.active=1;f->download.size=s->size;f->download.done=0;snprintf(f->download.path,sizeof(f->download.path),"%s",path);r->progress=progress_stream;
 snprintf(r->type,sizeof(r->type),"%s",hex?"text/plain; charset=utf-8":mime(path));
 if(attach){char name[800];const char*base=strrchr(path,'/');escape(base?base+1:path,name,sizeof(name),1);snprintf(r->extra+strlen(r->extra),sizeof(r->extra)-strlen(r->extra),"Content-Disposition: attachment; filename*=UTF-8''%.600s\r\n",name);}
 return 1;
invalid:f->io.close(f->io.ctx,s->fd);free(s);snprintf(r->extra,sizeof(r->extra),"Content-Range: bytes */%lu\r\n",(unsigned long)e.size);return text(f,r,416,"Invalid byte range.");}
static unsigned le16(const uint8_t*p){return p[0]|((unsigned)p[1]<<8);}static uint32_t le32(const uint8_t*p){return le16(p)|((uint32_t)le16(p+2)<<16);}
static int elf_ok(WifiFiles*f,int fd,uint32_t size){uint8_t h[52],p[32];uint32_t ph,entry;unsigned count,i,found=0;
 if(f->io.read(f->io.ctx,fd,0,h,52)!=52||memcmp(h,"\177ELF\1\1\1",7)||le16(h+16)!=2||le16(h+18)!=8||le16(h+40)!=52||le16(h+42)!=32){return 0;}
 ph=le32(h+28);entry=le32(h+24);count=le16(h+44);if(!count||count>64||ph>size||count>(size-ph)/32){return 0;}
 for(i=0;i<count;i++){uint32_t off,addr,fs,ms;if(f->io.read(f->io.ctx,fd,ph+i*32,p,32)!=32){return 0;}if(le32(p)!=1)continue;off=le32(p+4);addr=le32(p+8);fs=le32(p+16);ms=le32(p+20);
  if(off>size||fs>size-off||fs>ms||addr<0x10000||addr>=0x2000000||ms>0x2000000-addr){return 0;}if((le32(p+24)&1)&&entry>=addr&&entry-addr<ms)found=1;
 }return found;}
void wifi_files_init(WifiFiles*f,const WfOps*ops,const char*running){memset(f,0,sizeof(*f));f->io=*ops;if(!running||wifi_files_path(running,f->running))strcpy(f->running,"mc0:/Wifi/Wifi.elf");}
int wifi_files_busy(const WifiFiles*f){return f->readers||f->upload.active;}
void wifi_files_tick(WifiFiles*f,uint32_t now){if(f->upload.active&&(uint32_t)(now-f->upload.at)>120000)f->upload.active=0;}
int wifi_files_request(WifiFiles*f,const char*method,const char*target,const char*headers,const uint8_t*body,unsigned n,uint32_t now,WfReply*r){
 char value[1024],path[WF_PATH],op[32];int q;memset(r,0,sizeof(*r));
 if(!strcmp(target,"/files/info")){char t[3000],url[WF_PATH*3+1],up[WF_PATH*3+1],down[WF_PATH*3+1];escape(f->running,url,sizeof(url),1);escape(f->upload.path,up,sizeof(up),1);escape(f->download.path,down,sizeof(down),1);snprintf(t,sizeof(t),"{\"pcBackupRequired\":1,\"upload\":%u,\"received\":%lu,\"size\":%lu,\"running\":\"%s\",\"uploadPath\":\"%s\",\"download\":%u,\"downloadSize\":%lu,\"downloadDone\":%lu,\"downloadPath\":\"%s\"}",f->upload.active,(unsigned long)f->upload.received,(unsigned long)f->upload.size,url,up,f->download.active,(unsigned long)f->download.size,(unsigned long)f->download.done,down);return text(f,r,200,t);}
 if(!strncmp(target,"/files/upload/",14)){
  const char*auth=header(headers,"x-ps2-files:");if(strcmp(method,"POST")||!auth||auth[0]!='1'||auth[1]!='\r'){return text(f,r,403,"Upload authorization header required.");}
  snprintf(op,sizeof(op),"%.*s",(int)strcspn(target+14,"?"),target+14);
  if(!strcmp(op,"begin")){char size_text[32],*tail;unsigned long size;WfEntry e;
   if(f->upload.active){return text(f,r,409,"Another transfer is active.");}
   if(query(target,"path",value,sizeof(value))!=1||wifi_files_path(value,path)||wifi_files_protected(f,path,0)){return text(f,r,403,"Invalid or protected target.");}
   if(same(path,f->running))snprintf(path,sizeof(path),"%s",f->running);
   if(query(target,"size",size_text,sizeof(size_text))!=1){return text(f,r,400,"File size required.");}size=strtoul(size_text,&tail,10);if(tail==size_text||*tail||size>0x7ffff000u){return text(f,r,413,"Invalid file size.");}
   if(f->io.stat(f->io.ctx,path,&e)==0&&e.dir){return text(f,r,409,"Target cannot be a directory.");}
   if(snprintf(f->upload.stage,sizeof(f->upload.stage),"%s.part",path)>=(int)sizeof(f->upload.stage)||((!strncmp(path,"mc",2))&&strlen(strrchr(path,'/')+1)>26)){return text(f,r,400,"Temporary filename too long.");}
   if(f->io.stat(f->io.ctx,f->upload.stage,&e)==0){return text(f,r,409,"A .part file already exists; inspect it before removal.");}
   if(f->io.write(f->io.ctx,f->upload.stage,0,0,0,1)<0){return text(f,r,507,"Cannot create temporary file; check space/device.");}
   strcpy(f->upload.path,path);f->upload.size=size;f->upload.received=0;f->upload.crc=0xffffffffu;f->upload.active=1;f->upload.at=now;return text(f,r,200,"Upload ready.");
  }
  if(!f->upload.active){return text(f,r,409,"No active upload.");}f->upload.at=now;
  if(!strcmp(op,"abort")){f->upload.active=0;f->io.remove(f->io.ctx,f->upload.stage);return text(f,r,200,"Transfer aborted; original file preserved.");}
  if(!strcmp(op,"chunk")){char offset_text[32],*tail;unsigned long off;
   if(query(target,"offset",offset_text,sizeof(offset_text))!=1){return text(f,r,400,"Offset required.");}off=strtoul(offset_text,&tail,10);
   if(*tail||!n||n>WF_CHUNK||off>f->upload.received||off>f->upload.size||n>f->upload.size-off){return text(f,r,409,"Invalid chunk order or size.");}
   if(off<f->upload.received){uint8_t verify[WF_CHUNK];int fd=f->io.open(f->io.ctx,f->upload.stage),ok=0;
    if(fd>=0){ok=off+n<=f->upload.received&&f->io.read(f->io.ctx,fd,off,verify,n)==(int)n&&!memcmp(verify,body,n);f->io.close(f->io.ctx,fd);}return text(f,r,ok?200:409,ok?"Chunk already received.":"Retransmitted chunk differs.");}
   if(f->io.write(f->io.ctx,f->upload.stage,off,body,n,0)!=(int)n){return text(f,r,507,"Cannot write chunk; original file preserved.");}
   f->upload.crc=crc(f->upload.crc,body,n);f->upload.received+=n;return text(f,r,200,"Chunk received.");
  }
  if(!strcmp(op,"commit")){char crc_text[32],*tail;uint8_t buffer[WF_CHUNK];uint32_t expected,actual=0xffffffffu,off=0;int fd,i,had_old;WfEntry e;
   if(f->upload.received!=f->upload.size||query(target,"crc32",crc_text,sizeof(crc_text))!=1||strlen(crc_text)!=8){return text(f,r,409,"Incomplete file or missing CRC32.");}expected=strtoul(crc_text,&tail,16);if(*tail||expected!=~f->upload.crc){return text(f,r,422,"Transfer CRC32 mismatch.");}
   if(f->io.stat(f->io.ctx,f->upload.stage,&e)<0||e.size!=f->upload.size){return text(f,r,422,"Temporary file size mismatch.");}
   fd=f->io.open(f->io.ctx,f->upload.stage);if(fd<0){return text(f,r,500,"Cannot open verification file.");}
   while(off<f->upload.size){unsigned want=f->upload.size-off;if(want>sizeof(buffer))want=sizeof(buffer);if(f->io.read(f->io.ctx,fd,off,buffer,want)!=(int)want)break;actual=crc(actual,buffer,want);off+=want;}
   i=off==f->upload.size&&~actual==expected;
   if(i){const char*ext=strrchr(f->upload.path,'.');if(ext&&same(ext,".elf"))i=elf_ok(f,fd,f->upload.size);}
   f->io.close(f->io.ctx,fd);if(!i){return text(f,r,422,"Read/ELF validation failed; original file preserved.");}
   had_old=f->io.stat(f->io.ctx,f->upload.path,&e)==0;
   if(had_old){uint32_t old_crc;unsigned size=e.size;
    if(query(target,"pcbackup",crc_text,sizeof(crc_text))!=1||strlen(crc_text)!=8){return text(f,r,428,"Back up the old file on the PC first; pcbackup CRC32 required.");}
    old_crc=strtoul(crc_text,&tail,16);if(*tail){return text(f,r,400,"Invalid PC backup CRC32.");}
    fd=f->io.open(f->io.ctx,f->upload.path);if(fd<0){return text(f,r,500,"Cannot read old file; unchanged.");}
    off=0;actual=0xffffffffu;while(off<size){unsigned want=size-off;if(want>sizeof(buffer))want=sizeof(buffer);if(f->io.read(f->io.ctx,fd,off,buffer,want)!=(int)want)break;actual=crc(actual,buffer,want);off+=want;}f->io.close(f->io.ctx,fd);
    if(off!=size||~actual!=old_crc){return text(f,r,409,"PC backup differs from old file; unchanged.");}
    if(f->io.remove(f->io.ctx,f->upload.path)<0){return text(f,r,500,"Cannot remove old file; unchanged.");}
   }
   if(f->io.rename(f->io.ctx,f->upload.stage,f->upload.path)<0){return text(f,r,500,"Commit failed; verified .part preserved. Retry commit; backup is on PC.");}
   f->upload.active=0;return text(f,r,200,"File verified and saved. No card backup created. Reopen ELF to use update.");
  }return text(f,r,400,"Unknown upload command.");
 }
 if(strcmp(method,"GET")&&strcmp(method,"HEAD")){return 0;}
 {static const char*keys[]={"ls","del","download","view","raw"};unsigned k;
  for(k=0;k<5;k++){q=query(target,keys[k],value,sizeof(value));if(!q)continue;if(q<0||wifi_files_path(value,path)){return text(f,r,400,"Invalid path. Use mc0:/ mc1:/ mass:/.");}
   if(same(path,f->running))snprintf(path,sizeof(path),"%s",f->running);
   if(k==0){char*out=0,escaped[1600];unsigned used=0,cap=0,total=0;escape(path,escaped,sizeof(escaped),0);
    if(append(&out,&used,&cap,"<!doctype html><meta charset=utf-8><title>PS2 file tree</title><style>body{font:14px monospace;background:#101827;color:#eef;padding:18px}a{color:#6dd}pre{white-space:pre-wrap}</style><h2>%s</h2><pre>",escaped)||tree(f,path,0,&total,&out,&used,&cap)||append(&out,&used,&cap,"</pre><p>%u entries</p>",total)){free(out);return text(f,r,507,"Cannot read tree or exceeded 4096 entries / depth 32 / 2 MiB view limit.");}return memory_reply(f,r,200,"text/html; charset=utf-8",out,used);
   }
   if(k==1){WfEntry e;const char*site=header(headers,"sec-fetch-site:");if(site&&!strncmp(site,"cross-site",10)){return text(f,r,403,"Cross-site delete request rejected.");}if(!strcmp(method,"HEAD")){return text(f,r,405,"HEAD cannot delete files.");}if(wifi_files_protected(f,path,1)){return text(f,r,403,"System/boot/seed/profile and running ELF are protected.");}
    if(f->io.stat(f->io.ctx,path,&e)<0){return text(f,r,404,"File not found.");}if(e.dir){return text(f,r,403,"Directory/bulk deletion is disabled.");}if(f->io.remove(f->io.ctx,path)<0){return text(f,r,500,"Cannot delete file.");}return text(f,r,200,"File deleted.");}
   if(k==3){const char*type=mime(path);if(!strncmp(type,"image/",6)||!strncmp(type,"video/",6)||!strncmp(type,"audio/",6)){char url[WF_PATH*3+1],*out=0;unsigned used=0,cap=0;escape(path,url,sizeof(url),1);
     const char*tag=!strncmp(type,"image/",6)?"img":!strncmp(type,"audio/",6)?"audio":"video";
     if(append(&out,&used,&cap,"<!doctype html><meta charset=utf-8><title>PS2 media</title><%s src=\"/?raw=%s\" controls preload=metadata style=\"max-width:100%%\">",tag,url)||(!strncmp(type,"image/",6)?0:append(&out,&used,&cap,"</%s>",tag))){free(out);return -1;}return memory_reply(f,r,200,"text/html; charset=utf-8",out,used);}
    if(!strncmp(type,"text/",5)||looks_text(f,path)){int result=stream_file(f,r,path,headers,0,0);if(result>0&&r->status==200)snprintf(r->type,sizeof(r->type),"text/plain; charset=utf-8");return result;}return stream_file(f,r,path,headers,1,0);}
   return stream_file(f,r,path,headers,0,k==2);
  }
 }return 0;
}
