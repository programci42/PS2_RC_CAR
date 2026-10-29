/* Native MC RPC; fileXio only for USB mass. No format or recursive delete. */
#define NEWLIB_PORT_AWARE
#include <tamtypes.h>
#include <kernel.h>
#include <delaythread.h>
#include <libmc.h>
#include <fileXio_rpc.h>
#include <io_common.h>
#include <stdio.h>
#include <string.h>
#include "wifi_files_native.h"
#include "wifi_seed.h"
static int ready,fault;
static uint8_t data[4096] __attribute__((aligned(64)));
static sceMcTblGetDir table[64] __attribute__((aligned(64)));
static iox_dirent_t dent __attribute__((aligned(64)));
static struct {int used,fd,port,mass;uint32_t pos;} handles[8];
static int syncmc(int issued){int i,cmd,result;if(fault||issued<0){return -1;}for(i=0;i<5000;i++){int done=mcSync(MC_NOWAIT,&cmd,&result);if(done==1){return result;}if(done<0){return -1;}DelayThread(1000);}fault=1;return -1;}
static int portof(const char*p){return !strncmp(p,"mc0:/",5)?0:!strncmp(p,"mc1:/",5)?1:-1;}
static int usable(int port){int type=0,free_space=0,formatted=0,r;if(!ready||fault){return -1;}r=syncmc(mcGetInfo(port,0,&type,&free_space,&formatted));return r>=-1&&type==MC_TYPE_PS2&&formatted?0:-1;}
static int stage_fd=-1,stage_port;static uint32_t stage_position;static char stage_path[WF_PATH];
int wifi_files_native_flush(void){int r=0,c;if(stage_fd<0)return 0;r=syncmc(mcFlush(stage_fd));c=syncmc(mcClose(stage_fd));stage_fd=-1;stage_path[0]=0;return r<0||c<0?-1:0;}
void wifi_files_native_devices(char*out,unsigned cap){unsigned n=0,port;n=snprintf(out,cap,"[");for(port=0;port<2;port++){int type=0,space=0,formatted=0,r=-1;if(ready&&!fault)r=syncmc(mcGetInfo(port,0,&type,&space,&formatted));n+=snprintf(out+n,cap-n,"%s{\"port\":%u,\"result\":%d,\"type\":%d,\"formatted\":%d,\"freeKb\":%d,\"manufacturer\":null,\"serial\":null}",port?",":"",port,r,type,formatted,space);}snprintf(out+n,cap-n,"]");}
static int stat_file(void*ctx,const char*p,WfEntry*e){int port=portof(p),r;(void)ctx;memset(e,0,sizeof(*e));
 if(stage_fd>=0&&!strcmp(p,stage_path)&&wifi_files_native_flush())return -1;
 if(port>=0){if(usable(port)){return -1;}if(!p[5]){e->dir=1;return 0;}memset(table,0,sizeof(table));SyncDCache(table,table+64);r=syncmc(mcGetDir(port,0,p+4,0,1,UNCACHED_SEG(table)));if(r!=1){return -1;}
  {const sceMcTblGetDir*t=UNCACHED_SEG(table);e->size=t->FileSizeByte;e->dir=!!(t->AttrFile&MC_ATTR_SUBDIR);memcpy(e->name,t->EntryName,32);e->name[32]=0;}return 0;
 }else {iox_stat_t st __attribute__((aligned(64)));if(wifi_storage_usb_init()<0){return -1;}memset(&st,0,sizeof(st));if(fileXioGetStat(p,&st)<0||st.hisize||FIO_S_ISLNK(st.mode)){return -1;}e->size=st.size;e->dir=FIO_S_ISDIR(st.mode);return 0;}}
static int list_files(void*ctx,const char*p,WfEntry*out,unsigned cap){int port=portof(p),n,total=0,mode=0,fd;(void)ctx;
 if(port>=0){char pattern[WF_PATH+3];if(usable(port)){return -1;}snprintf(pattern,sizeof(pattern),"%s%s*",p+4,p[strlen(p)-1]=='/'?"":"/");
  for(;;){int i;const sceMcTblGetDir*t;memset(table,0,sizeof(table));SyncDCache(table,table+64);n=syncmc(mcGetDir(port,0,pattern,mode,64,UNCACHED_SEG(table)));if(n<0){return -1;}if(!n)break;mode=1;t=UNCACHED_SEG(table);
   for(i=0;i<n;i++){if(total>=(int)cap){return -2;}memset(out+total,0,sizeof(*out));memcpy(out[total].name,t[i].EntryName,32);out[total].name[32]=0;out[total].size=t[i].FileSizeByte;out[total].dir=!!(t[i].AttrFile&MC_ATTR_SUBDIR);total++;}
  }return total;
 }
 if(wifi_storage_usb_init()<0){return -1;}fd=fileXioDopen(p);if(fd<0){return -1;}
 for(;;){memset(&dent,0,sizeof(dent));SyncDCache(&dent,(&dent)+1);n=fileXioDread(fd,UNCACHED_SEG(&dent));if(n<=0)break;
  {const iox_dirent_t*d=UNCACHED_SEG(&dent);if(total>=(int)cap){fileXioDclose(fd);return -2;}if(d->stat.hisize||FIO_S_ISLNK(d->stat.mode)){fileXioDclose(fd);return -1;}memset(out+total,0,sizeof(*out));memcpy(out[total].name,d->name,255);out[total].size=d->stat.size;out[total].dir=FIO_S_ISDIR(d->stat.mode);total++;}
 }fileXioDclose(fd);return n<0?-1:total;
}
static int open_file(void*ctx,const char*p){unsigned i;int port=portof(p),fd;(void)ctx;for(i=0;i<8&&handles[i].used;i++);if(i==8){return -1;}
 if(stage_fd>=0&&!strcmp(p,stage_path)&&wifi_files_native_flush())return -1;
 if(port>=0){if(usable(port)){return -1;}fd=syncmc(mcOpen(port,0,p+4,FIO_O_RDONLY));}
 else {if(wifi_storage_usb_init()<0){return -1;}fd=fileXioOpen(p,FIO_O_RDONLY,0);}if(fd<0){return -1;}
 handles[i].used=1;handles[i].fd=fd;handles[i].port=port;handles[i].mass=port<0;handles[i].pos=0;return i;
}
static int read_file(void*ctx,int id,uint32_t offset,void*out,unsigned n){int r;(void)ctx;if(id<0||id>=8||!handles[id].used||fault||n>sizeof(data)){return -1;}
 if(handles[id].pos!=offset){r=handles[id].mass?fileXioLseek(handles[id].fd,offset,SEEK_SET):syncmc(mcSeek(handles[id].fd,offset,SEEK_SET));if(r!=(int)offset){return -1;}handles[id].pos=offset;}
 memset(data,0,sizeof(data));SyncDCache(data,data+sizeof(data));r=handles[id].mass?fileXioRead(handles[id].fd,UNCACHED_SEG(data),n):syncmc(mcRead(handles[id].fd,UNCACHED_SEG(data),n));
 if(r<0||(unsigned)r>n){return -1;}memcpy(out,UNCACHED_SEG(data),r);handles[id].pos+=r;return r;
}
static int close_file(void*ctx,int id){int r;(void)ctx;if(id<0||id>=8||!handles[id].used){return -1;}r=handles[id].mass?fileXioClose(handles[id].fd):syncmc(mcClose(handles[id].fd));handles[id].used=0;return r;}
static int write_file(void*ctx,const char*p,uint32_t offset,const void*bytes,unsigned n,int create){int port=portof(p),fd,r,closed;(void)ctx;if(n>sizeof(data)||fault){return -1;}
 if(port>=0&&strlen(p)>5&&!strcmp(p+strlen(p)-5,".part")){
  if(stage_fd>=0&&(create||strcmp(p,stage_path)||port!=stage_port)){if(wifi_files_native_flush())return -1;}
  if(stage_fd<0){if(usable(port))return -1;stage_fd=syncmc(mcOpen(port,0,p+4,FIO_O_RDWR|(create?(FIO_O_CREAT|FIO_O_TRUNC):0)));if(stage_fd<0)return -1;stage_port=port;stage_position=0;snprintf(stage_path,sizeof(stage_path),"%s",p);}
  if(stage_position!=offset){if(syncmc(mcSeek(stage_fd,offset,SEEK_SET))!=(int)offset)return -1;stage_position=offset;}
  if(!n)return 0;
  memcpy(data,bytes,n);r=syncmc(mcWrite(stage_fd,data,n));if(r>0)stage_position+=r;return r;
 }
 if(port>=0){if(usable(port)){return -1;}fd=syncmc(mcOpen(port,0,p+4,FIO_O_RDWR|(create?(FIO_O_CREAT|FIO_O_TRUNC):0)));if(fd<0){return -1;}
  r=syncmc(mcSeek(fd,offset,SEEK_SET));if(r==(int)offset){if(n){memcpy(data,bytes,n);r=syncmc(mcWrite(fd,data,n));}else r=0;}
  else r=-1;
  if(r==(int)n&&syncmc(mcFlush(fd))<0){r=-1;}closed=syncmc(mcClose(fd));return closed<0?-1:r;
 }
 if(wifi_storage_usb_init()<0){return -1;}fd=fileXioOpen(p,FIO_O_RDWR|(create?(FIO_O_CREAT|FIO_O_TRUNC):0),0666);if(fd<0){return -1;}
 r=fileXioLseek(fd,offset,SEEK_SET);if(r==(int)offset)r=n?fileXioWrite(fd,bytes,n):0;else r=-1;closed=fileXioClose(fd);return closed<0?-1:r;
}
static int rename_file(void*ctx,const char*a,const char*b){int port=portof(a);(void)ctx;if(port>=0){const char*base=strrchr(b,'/');if(portof(b)!=port||!base){return -1;}return syncmc(mcRename(port,0,a+4,base+1));}if(wifi_storage_usb_init()<0){return -1;}return fileXioRename(a,b);}
static int remove_file(void*ctx,const char*p){int port=portof(p);(void)ctx;if(stage_fd>=0&&!strcmp(p,stage_path)&&wifi_files_native_flush())return -1;if(port>=0){if(usable(port)){return -1;}return syncmc(mcDelete(port,0,p+4));}if(wifi_storage_usb_init()<0){return -1;}return fileXioRemove(p);}
void wifi_files_native_init(WifiFiles*f,int mc_ready,const char*running){WfOps io={0,stat_file,list_files,open_file,read_file,close_file,write_file,rename_file,remove_file};ready=mc_ready;fault=0;memset(handles,0,sizeof(handles));wifi_files_init(f,&io,running);}
