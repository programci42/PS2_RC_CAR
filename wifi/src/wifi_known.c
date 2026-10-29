#include "wifi_known.h"
#include "rtl_wpa_crypto.h"
#include <string.h>
#include <stdio.h>
#include "wifi_files.h"
static size_t row_password(const uint8_t*p,size_t n,size_t sep,uint8_t last[4],uint8_t manual[4]){
 size_t end=n,k;char ip[32];memset(last,0,4);memset(manual,0,4);
 k=end;while(k>sep+1&&p[k-1]!=';')k--;
 if(k>sep+1&&end-k<sizeof(ip)&&end-k>7&&!memcmp(p+k,"static:",7)){memcpy(ip,p+k+7,end-k-7);ip[end-k-7]=0;if(!wifi_ip_parse(ip,manual))end=k-1;}
 k=end;while(k>sep+1&&p[k-1]!=';')k--;
 if(k>sep+1&&end-k<sizeof(ip)){memcpy(ip,p+k,end-k);ip[end-k]=0;if(!strcmp(ip,"0.0.0.0")||!wifi_ip_parse(ip,last))end=k-1;}
 return end-sep-1;
}
static int known_scan(const uint8_t *csv,size_t n,const RtlScanList *scan,unsigned *selected,uint8_t *pmk,uint8_t *flags){
    size_t start=0,end,sep,len,pass;unsigned i;int matches=0;
    if(!csv||!scan||n>4096||scan->count>RTL_SCAN_CAPACITY)return -1;
    if(pmk)memset(pmk,0,32);
    if(flags)memset(flags,0,RTL_SCAN_CAPACITY);
    if(n>=3&&!memcmp(csv,"\xef\xbb\xbf",3))start=3;
    while(start<n){
        end=start;while(end<n&&csv[end]!='\n')end++;
        len=end-start;if(len&&csv[start+len-1]=='\r')len--;
        if(len&&csv[start]!='#'){
            for(sep=0;sep<len&&csv[start+sep]!=';';sep++);
            if(sep>0&&sep<=32&&sep<len){
                {uint8_t last[4],manual[4];pass=row_password(csv+start,len,sep,last,manual);}
                if(pass>=8&&pass<=63&&!memchr(csv+start,0,len))
                    for(i=0;i<scan->count;i++)if(scan->ap[i].wpa2_ccmp_psk&&scan->ap[i].ssid_length==sep&&
                      !memcmp(scan->ap[i].ssid,csv+start,sep)){
                        if(flags){if(!flags[i])matches++;flags[i]=1;}
                        else {if(rtl_wpa_pmk(csv+start+sep+1,pass,scan->ap[i].ssid,sep,pmk))return -1;
                            *selected=i;return 1;}
                    }
            }
        }
        start=end<n?end+1:n;
    }
    return matches;
}
int wifi_known_config(const uint8_t*csv,size_t n,const uint8_t*ssid,unsigned sl,uint8_t last[4],uint8_t manual[4]){
 size_t start=n>=3&&!memcmp(csv,"\xef\xbb\xbf",3)?3:0,end,len;memset(last,0,4);memset(manual,0,4);if(n>4096||!sl||sl>32)return -1;
 while(start<n){end=start;while(end<n&&csv[end]!='\n')end++;len=end-start;if(len&&csv[start+len-1]=='\r')len--;if(len>sl&&csv[start+sl]==';'&&!memcmp(csv+start,ssid,sl)){row_password(csv+start,len,sl,last,manual);return 1;}start=end<n?end+1:n;}return 0;
}
int wifi_known_update(const uint8_t*csv,size_t n,const uint8_t*ssid,unsigned sl,const uint8_t last[4],const uint8_t manual[4],uint8_t*out,unsigned cap){
 size_t start=n>=3&&!memcmp(csv,"\xef\xbb\xbf",3)?3:0,end,len,at=n,cut=n,base=0;char suffix[100];uint8_t a[4],b[4];int count;
 if(n>4096||!sl||sl>32||memchr(ssid,';',sl)||memchr(ssid,'\n',sl)||memchr(ssid,'\r',sl)||memchr(ssid,0,sl))return -1;
 while(start<n){end=start;while(end<n&&csv[end]!='\n')end++;len=end-start;if(len&&csv[start+len-1]=='\r')len--;if(len>sl&&csv[start+sl]==';'&&!memcmp(csv+start,ssid,sl)){at=start;cut=end<n?end+1:n;base=sl+1+row_password(csv+start,len,sl,a,b);break;}start=end<n?end+1:n;}
 count=snprintf(suffix,sizeof(suffix),";%u.%u.%u.%u",last[0],last[1],last[2],last[3]);
 /* 0.0.0.0 is a placeholder before first connection. */
 if(manual[0])count+=snprintf(suffix+count,sizeof(suffix)-count,";static:%u.%u.%u.%u",manual[0],manual[1],manual[2],manual[3]);
 {unsigned used=0,extra=at==n&&n&&csv[n-1]!='\n';size_t total=at+extra+(base?base:sl+1)+count+2+(n-cut);if(total>4096||total>=cap)return -1;
  memcpy(out,csv,at);used=at;if(extra)out[used++]='\n';if(base){memcpy(out+used,csv+at,base);used+=base;}else{memcpy(out+used,ssid,sl);used+=sl;out[used++]=';';}memcpy(out+used,suffix,count);used+=count;out[used++]='\r';out[used++]='\n';memcpy(out+used,csv+cut,n-cut);used+=n-cut;out[used]=0;return used;}
}
int wifi_known_save(WifiFiles*f,const char*path,const uint8_t*ssid,unsigned sl,const uint8_t last[4],const uint8_t manual[4]){
 static uint8_t old[4097],next[4097],verify[4097];char stage[WF_PATH],backup[WF_PATH];WfEntry e;int fd,n=0,len,exists,ok;
 if(wifi_files_busy(f)||!f->io.stat){return -1;}exists=f->io.stat(f->io.ctx,path,&e)==0;
 if(exists){if(e.dir||e.size>4096)return -1;fd=f->io.open(f->io.ctx,path);if(fd<0)return -1;n=f->io.read(f->io.ctx,fd,0,old,e.size);f->io.close(f->io.ctx,fd);if(n!=(int)e.size)return -1;}
 len=wifi_known_update(old,n,ssid,sl,last,manual,next,sizeof(next));if(len<0)return -1;if(len==n&&!memcmp(old,next,n))return 0;
 if(snprintf(stage,sizeof(stage),"%s.ipnew",path)>=(int)sizeof(stage)||snprintf(backup,sizeof(backup),"%s.ipbak",path)>=(int)sizeof(backup)||f->io.stat(f->io.ctx,stage,&e)==0)return -1;
 if(f->io.write(f->io.ctx,stage,0,next,len,1)!=len)return -1;
 fd=f->io.open(f->io.ctx,stage);if(fd<0)return -1;ok=f->io.read(f->io.ctx,fd,0,verify,len)==len&&!memcmp(next,verify,len);f->io.close(f->io.ctx,fd);if(!ok)return -1;
 if(exists){if(f->io.stat(f->io.ctx,backup,&e)==0&&(e.dir||f->io.remove(f->io.ctx,backup)<0))return -1;if(f->io.rename(f->io.ctx,path,backup)<0)return -1;}
 if(f->io.rename(f->io.ctx,stage,path)<0){if(exists)f->io.rename(f->io.ctx,backup,path);return -1;}return 0;
}
int wifi_known_select(const uint8_t *csv,size_t n,const RtlScanList *scan,unsigned *selected,uint8_t pmk[32]){
    if(!selected||!pmk)return -1;
    return known_scan(csv,n,scan,selected,pmk,0);
}
int wifi_known_mark(const uint8_t *csv,size_t n,const RtlScanList *scan,uint8_t flags[RTL_SCAN_CAPACITY]){
    if(!flags)return -1;
    return known_scan(csv,n,scan,0,0,flags);
}
