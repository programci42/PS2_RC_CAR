/* SPDX-License-Identifier: MIT. Restricted device filesystem service. */
#ifndef WIFI_FILES_H
#define WIFI_FILES_H
#include <stdint.h>
#define WF_PATH 256
#define WF_CHUNK 2048
typedef struct {char name[256];uint32_t size;unsigned dir;} WfEntry;
typedef struct {
 void *ctx;
 int (*stat)(void*,const char*,WfEntry*);
 int (*list)(void*,const char*,WfEntry*,unsigned);
 int (*open)(void*,const char*);
 int (*read)(void*,int,uint32_t,void*,unsigned);
 int (*close)(void*,int);
 int (*write)(void*,const char*,uint32_t,const void*,unsigned,int create);
 int (*rename)(void*,const char*,const char*);
 int (*remove)(void*,const char*);
} WfOps;
typedef struct {
 int status;char type[64],extra[768];uint32_t size;
 void *stream;int (*read)(void*,uint32_t,void*,unsigned);void (*close)(void*);
 void (*progress)(void*,uint32_t);
} WfReply;
typedef struct {
 WfOps io;char running[WF_PATH];
 struct {unsigned active;char path[WF_PATH],stage[WF_PATH];uint32_t size,received,crc,at;} upload;
 unsigned readers;
 struct {char path[WF_PATH];uint32_t size,done;unsigned active,generation;} download;
} WifiFiles;
void wifi_files_init(WifiFiles*,const WfOps*,const char *running);
int wifi_files_path(const char *encoded,char out[WF_PATH]);
int wifi_files_protected(const WifiFiles*,const char*,int deleting);
int wifi_files_request(WifiFiles*,const char *method,const char *target,const char *headers,const uint8_t *body,unsigned n,uint32_t now,WfReply*);
void wifi_files_tick(WifiFiles*,uint32_t now);
int wifi_files_busy(const WifiFiles*);
#endif
