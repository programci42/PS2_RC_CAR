#include <stdint.h>
#define MC_NOWAIT 1
#define MC_TYPE_PS2 2
#define sceMcResNoEntry (-4)
#define sceMcResNoFormat (-2)
#define sceMcResChangedCard (-1)
#define FIO_O_RDONLY 1
#define FIO_O_WRONLY 2
#define FIO_O_RDWR 3
#define FIO_O_CREAT 0x200
void *seed_dma_alias(void*);
#define UNCACHED_SEG(p) seed_dma_alias(p)
void SyncDCache(void *,void *);
int mcSync(int,int*,int*);
int mcGetInfo(int,int,int*,int*,int*);
int mcOpen(int,int,const char*,int);
int mcRead(int,void*,int);
int mcWrite(int,const void*,int);
int mcClose(int);
int mcSeek(int,int,int);
int mcFlush(int);
int mcMkDir(int,int,const char*);
void DelayThread(unsigned);
int SifExecModuleBuffer(void*,unsigned,int,const char*,int*);
int fileXioInit(void);
int fileXioOpen(const char*,int,int);
int fileXioRead(int,void*,int);
int fileXioWrite(int,void*,int);
int fileXioClose(int);
