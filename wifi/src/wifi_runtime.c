#include "wifi_runtime.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>
#include "wifi_version.h"
#ifdef _EE
#include <kernel.h>
#endif
static void battery_path(WifiRuntime*r,char*p){const char*s=strrchr(r->settings,'/');snprintf(p,WF_PATH,"%.*spil.json",s?(int)(s-r->settings+1):0,r->settings);}
static int battery_json(WifiRuntime*r,char*out,unsigned cap){return snprintf(out,cap,"{\"capacityMah\":%u,\"motors\":%u,\"baseMw\":%u,\"motorMa\":%u,\"effPermille\":%u,\"remainingUwh\":%lu}",r->battery_capacity,r->battery_motors,r->battery_base_mw,r->battery_motor_ma,r->battery_eff,(unsigned long)r->battery_uwh);}
static int battery_parse(WifiRuntime*r,const char*s){unsigned c,m,b,a,e;unsigned long rem;int end=0;if(sscanf(s,"{\"capacityMah\":%u,\"motors\":%u,\"baseMw\":%u,\"motorMa\":%u,\"effPermille\":%u,\"remainingUwh\":%lu}%n",&c,&m,&b,&a,&e,&rem,&end)!=6||s[end]||c<100||c>20000||m<1||m>8||b<1000||b>100000||a<100||a>50000||e<500||e>1000||rem>c*11100u)return -1;
 r->battery_capacity=c;r->battery_motors=m;r->battery_base_mw=b;r->battery_motor_ma=a;r->battery_eff=e;r->battery_uwh=rem;r->battery_set=1;return 0;}
void wifi_runtime_energy(WifiRuntime*r,uint32_t now){uint32_t elapsed=now-r->battery_at;unsigned mask=r->robot->applied,coils=0,motor_sides=0,i;uint64_t used;r->battery_at=now;
 if(!r->battery_set)return;
 for(i=0;i<8;i++)coils+=(mask>>i)&1;
 if(mask&(3u<<(r->robot->left*2)))motor_sides++;
 if(mask&(3u<<(r->robot->right*2)))motor_sides++;
 /* Configurable estimates, NOT electrical measurements. Relay board 5V,
    idle 100mA + 70mA/coil; motor current is battery-side at nominal 11.1V. */
 r->battery_power_mw=(unsigned)(((uint64_t)r->battery_base_mw+5u*(100+coils*70))*1000/r->battery_eff)+(unsigned)((uint64_t)11100*r->battery_motor_ma*r->battery_motors*motor_sides/2000);
 r->battery_motor_100ms_uwh=(unsigned)((uint64_t)11100*r->battery_motor_ma/36000);
 r->battery_fraction+=(uint64_t)r->battery_power_mw*elapsed;used=r->battery_fraction/3600;r->battery_fraction%=3600;
 if(used){r->battery_uwh=used>=r->battery_uwh?0:r->battery_uwh-(uint32_t)used;r->battery_dirty=1;}
}
int wifi_runtime_replace(WifiFiles*f,const char*path,const void*data,unsigned n){char temp[WF_PATH],backup[WF_PATH];uint8_t verify[4096];WfEntry e;int fd,ok,old;
 if(n>sizeof(verify)||wifi_files_busy(f)||!f->io.stat){return -1;}
 if(snprintf(temp,sizeof(temp),"%s.new",path)>=(int)sizeof(temp)||snprintf(backup,sizeof(backup),"%s.bak",path)>=(int)sizeof(backup)){return -1;}
 if(f->io.stat(f->io.ctx,temp,&e)==0){return -1;}old=f->io.stat(f->io.ctx,path,&e)==0;if(old&&e.dir){return -1;}
 if(f->io.write(f->io.ctx,temp,0,data,n,1)!=(int)n){return -1;}
 fd=f->io.open(f->io.ctx,temp);if(fd<0){return -1;}ok=f->io.read(f->io.ctx,fd,0,verify,n)==(int)n&&!memcmp(data,verify,n);f->io.close(f->io.ctx,fd);if(!ok){return -1;}
 if(old){if(f->io.stat(f->io.ctx,backup,&e)==0&&(e.dir||f->io.remove(f->io.ctx,backup)<0)){return -1;}if(f->io.rename(f->io.ctx,path,backup)<0){return -1;}}
 if(f->io.rename(f->io.ctx,temp,path)<0){if(old)f->io.rename(f->io.ctx,backup,path);{return -1;}}return 0;
}
static void check_log(WifiRuntime*r,uint32_t now){WfEntry e;if(r->files->io.stat){if(r->files->io.stat(r->files->io.ctx,r->log,&e)==0){if(!e.dir)r->log_bytes=e.size;else r->log_failed=1;}else{char folder[WF_PATH];char*slash;snprintf(folder,sizeof(folder),"%s",r->log);slash=strrchr(folder,'/');if(slash){if(slash>strchr(folder,':' )+1)*slash=0;else slash[1]=0;if(!r->files->io.stat(r->files->io.ctx,folder,&e)&&e.dir)r->log_bytes=0;else r->log_failed=1;}}}r->log_warning=r->log_bytes>=WIFI_LOG_LIMIT;r->checked_at=now;}
void wifi_runtime_clock(WifiRuntime*r,int64_t epoch,uint32_t now){if(epoch>=946684800LL&&epoch<4102444800LL){r->epoch=epoch;r->epoch_at=now;r->clock_valid=1;}}
void wifi_runtime_error(WifiRuntime*r,uint32_t now,const char*message){char date[64],clean[150];unsigned i=0;time_t t;struct tm*tm;
 if(!r||!r->files){return;}while(message[i]&&i<sizeof(clean)-1){unsigned char c=message[i];clean[i]=c<32?' ':c;i++;}clean[i]=0;
 if(r->clock_valid){t=(time_t)(r->epoch+(uint32_t)(now-r->epoch_at)/1000);tm=gmtime(&t);if(tm)strftime(date,sizeof(date),"%Y-%m-%dT%H:%M:%SZ",tm);else strcpy(date,"RTC-invalid");}
 else snprintf(date,sizeof(date),"RTC-unavailable uptime=%lus",(unsigned long)(now/1000));
 if(r->queued==16){r->dropped++;{return;}}snprintf(r->queue[r->queued++],sizeof(r->queue[0]),"[%s] ERROR %s\r\n",date,clean);
}
static int load_config(WifiFiles*f,const char*path,RpConfig*out){char buf[RP_JSON_MAX+1];WfEntry e;int fd,n;if(!f->io.stat||f->io.stat(f->io.ctx,path,&e)<0)return 0;if(e.dir||e.size>RP_JSON_MAX){return -1;}fd=f->io.open(f->io.ctx,path);if(fd<0){return -1;}n=f->io.read(f->io.ctx,fd,0,buf,e.size);f->io.close(f->io.ctx,fd);return n==(int)e.size&&!rp_parse(buf,n,out)?1:-1;}
int wifi_runtime_channel(WifiRuntime*r,unsigned channel){char path[WF_PATH],json[40];const char*s=strrchr(r->settings,'/');int n,fd,end=0;WfEntry e;snprintf(path,sizeof(path),"%.*skanal.json",s?(int)(s-r->settings+1):0,r->settings);
 if(channel){if(channel>13){return -1;}if(channel==r->last_channel){return 0;}n=snprintf(json,sizeof(json),"{\"channel\":%u}",channel);if(wifi_runtime_replace(r->files,path,json,n)){return -1;}r->last_channel=channel;return 0;}
 if(r->files->io.stat&&r->files->io.stat(r->files->io.ctx,path,&e)==0&&!e.dir&&e.size<sizeof(json)&&(fd=r->files->io.open(r->files->io.ctx,path))>=0){n=r->files->io.read(r->files->io.ctx,fd,0,json,e.size);r->files->io.close(r->files->io.ctx,fd);if(n==(int)e.size){json[n]=0;if(sscanf(json,"{\"channel\":%u}%n",&channel,&end)==1&&!json[end]&&channel>=1&&channel<=13)r->last_channel=channel;}}
 return (int)r->last_channel;
}
void wifi_runtime_init(WifiRuntime*r,WifiFiles*f,RobotControl*robot,uint32_t now){char folder[WF_PATH],backup[WF_PATH];const char*s;int result;RpConfig cfg;
 memset(r,0,sizeof(*r));r->files=f;r->robot=robot;rp_default(&r->program,robot->left,robot->right);snprintf(folder,sizeof(folder),"%s",f->running);s=strrchr(folder,'/');if(s)folder[s-folder+1]=0;
 snprintf(r->log,sizeof(r->log),"%.240swifi.log",folder);
 /* Settings always on MC0, using the ELF's directory. */
 s=strchr(folder,':');snprintf(r->settings,sizeof(r->settings),"mc0:%.235sayarlar.json",s?s+1:"/Wifi/");
 result=load_config(f,r->settings,&cfg);if(result!=1){int recovered;snprintf(backup,sizeof(backup),"%.250s.bak",r->settings);recovered=load_config(f,backup,&cfg);if(recovered==1){r->config_error=1;wifi_runtime_error(r,now,"Recovered ayarlar.json backup; check settings.");result=1;}else if(result<0){r->config_error=1;wifi_runtime_error(r,now,"Invalid ayarlar.json; default buttons loaded.");}}
 if(result==1){r->program.config=cfg;robot->left=cfg.left;robot->right=cfg.right;}
 robot->program_mode=1;check_log(r,now);
 wifi_runtime_channel(r,0);
 r->battery_capacity=3300;r->battery_motors=4;r->battery_base_mw=18000;r->battery_motor_ma=2000;r->battery_eff=900;r->battery_at=r->battery_saved_at=now;
 {char path[WF_PATH],json[300];WfEntry e;int fd,n;battery_path(r,path);if(f->io.stat&&f->io.stat(f->io.ctx,path,&e)==0&&!e.dir&&e.size<sizeof(json)&&(fd=f->io.open(f->io.ctx,path))>=0){n=f->io.read(f->io.ctx,fd,0,json,e.size);f->io.close(f->io.ctx,fd);if(n==(int)e.size){json[n]=0;if(battery_parse(r,json))wifi_runtime_error(r,now,"Invalid battery estimate settings.");}}}
}
void wifi_runtime_tick(WifiRuntime*r,uint32_t now){WfEntry e;unsigned n;int exists;char batch[16*224];
 if(!r->files||!r->files->io.stat||r->robot->applied||r->robot->desired||r->robot->pad||r->robot->web||wifi_files_busy(r->files)){return;}
 if((uint32_t)(now-r->checked_at)>=3600000u)check_log(r,now);
 if(r->battery_dirty&&(uint32_t)(now-r->battery_saved_at)>=300000u){char path[WF_PATH],json[300];int size;battery_path(r,path);size=battery_json(r,json,sizeof(json));r->battery_saved_at=now;if(!wifi_runtime_replace(r->files,path,json,size))r->battery_dirty=0;else wifi_runtime_error(r,now,"Cannot save battery estimate.");}
 if(!r->queued||(uint32_t)(now-r->flush_at)<1000){return;}r->flush_at=now;
 exists=r->files->io.stat(r->files->io.ctx,r->log,&e)==0;if(exists&&e.dir){r->log_failed=1;{return;}}
 r->log_bytes=exists?e.size:0;r->log_warning=r->log_bytes>=WIFI_LOG_LIMIT;
 n=0;{unsigned i;for(i=0;i<r->queued;i++){unsigned len=strlen(r->queue[i]);memcpy(batch+n,r->queue[i],len);n+=len;}}
 if(r->files->io.write(r->files->io.ctx,r->log,exists?e.size:0,batch,n,!exists)!=(int)n){r->log_failed=1;{return;}}
 r->log_bytes=(exists?e.size:0)+n;r->queued=0;r->log_failed=0;if(r->log_bytes>=WIFI_LOG_LIMIT)r->log_warning=1;
}
static int auth(const char*h){const char*p=h;const char*k="x-ps2-control: 1";unsigned i;while((p=strstr(p,"\r\n"))!=0){p+=2;if(!strncmp(p,"\r\n",2))break;for(i=0;k[i]&&p[i]&&tolower((unsigned char)p[i])==k[i];i++);if(!k[i]&&p[i]=='\r')return 1;}return 0;}
static int seconds_value(const char*s,unsigned max,unsigned*out){unsigned v=0,n=0;while(*s){if(*s<'0'||*s>'9'||++n>6)return -1;v=v*10+*s++-'0';if(v>max)return -1;}if(!n)return -1;*out=v;return 0;}
int wifi_runtime_api(void*ctx,const char*method,const char*target,const char*headers,const uint8_t*body,unsigned n,uint32_t now,char*out,unsigned cap){WifiRuntime*r=ctx;RpConfig cfg;int len;
 if(!strcmp(target,"/display")){if(strcmp(method,"GET"))return 405;snprintf(out,cap,"{\"version\":\"%s\",\"displayEnabled\":%u,\"displayRequested\":%u,\"displayToggles\":%u,\"padConnected\":%u,\"padButtons\":%u,\"L2\":%u,\"R2\":%u,\"padThread\":%u,\"padTicks\":%u,\"padMaxGapMs\":%u,\"padPortOpen\":%u,\"padState\":%d,\"padReopens\":%u,\"padInitStage\":%u,\"padBytes\":%u,\"padRaw\":%u,\"padDma\":%u}",WIFI_VERSION,r->display_enabled,r->display_requested,r->display_toggles,r->pad_connected,r->pad_buttons,!!(r->pad_buttons&0x100),!!(r->pad_buttons&0x200),r->pad_thread,r->pad_ticks,r->pad_max_gap_ms,r->pad_port_open,r->pad_state,r->pad_reopens,r->pad_init_stage,r->pad_bytes,r->pad_raw,r->pad_dma);return 200;}
 if(!strcmp(target,"/?display=1")||!strcmp(target,"/?display=0")){if(strcmp(method,"GET"))return 405;if(strstr(headers,"cross-site"))return 403;r->display_enabled=target[10]=='1';r->display_requested=1;snprintf(out,cap,"{\"displayRequested\":true,\"enabled\":%u}",r->display_enabled);return 200;}
 if(!strncmp(target,"/?git=",6)){const char*v=target+6;unsigned dir=0,secs=0,k;int result;static const char*names[]={"ileri","geri","sol","sag"};if(strcmp(method,"GET"))return 405;if(strstr(headers,"cross-site"))return 403;
  for(k=0;k<4;k++)if(!strncmp(v,names[k],strlen(names[k]))){dir=k+1;v+=strlen(names[k]);break;}
  if(!dir||seconds_value(v,3600,&secs)||!secs){snprintf(out,cap,"Invalid motion. Example: ?git=ileri10 (1-3600 s).");return 400;}
  if(wifi_files_busy(r->files)){return 409;}result=robot_timed(r->robot,dir,secs,now);if(result){snprintf(out,cap,"Robot not ready or gamepad/system action has priority.");return result==-1?503:409;}
  snprintf(out,cap,"{\"movement\":%u,\"seconds\":%u,\"scheduled\":true}",dir,secs);return 200;
 }
 if(!strncmp(target,"/?shutdown=",11)){const char*v=target+11;unsigned seconds=0,action;if(strcmp(method,"GET"))return 405;if(strstr(headers,"cross-site"))return 403;
  if(v[0]!='-'||(v[1]!='s'&&v[1]!='r')){return 400;}action=v[1]=='s'?1:2;v+=2;
  if(*v&&(strncmp(v,"-t",2)||seconds_value(v+2,86400,&seconds)))return 400;
  if(wifi_files_busy(r->files)||r->reload_pending){snprintf(out,cap,"Wait for transfer or restart to finish.");return 409;}
  r->system_action=action;r->system_due=now+seconds*1000u;r->system_requested=now;r->robot->system_locked=1;r->robot->halt=1;r->robot->web=r->robot->timed=r->robot->desired=0;
  snprintf(out,cap,"{\"systemAction\":\"%s\",\"seconds\":%u,\"scheduled\":true}",action==1?"shutdown":"restart",seconds);return 200;
 }
 if(!strcmp(target,"/?reload=1")){if(strcmp(method,"GET"))return 405;if(r->system_action||r->robot->applied||r->robot->desired||r->robot->pad||r->robot->web||wifi_files_busy(r->files)){snprintf(out,cap,"Stop robot and finish transfer before reopening.");return 409;}
  if(r->battery_set&&r->battery_dirty){char path[WF_PATH],json[300];battery_path(r,path);len=battery_json(r,json,sizeof(json));if(wifi_runtime_replace(r->files,path,json,len)){snprintf(out,cap,"Cannot save battery estimate; reopening cancelled.");return 507;}r->battery_dirty=0;r->battery_saved_at=now;}
  r->reload_pending=1;r->reload_at=now;r->robot->halt=1;snprintf(out,cap,"{\"reload\":true,\"message\":\"ELF will reopen; Wi-Fi will reconnect.\"}");return 200;}
 if(!strcmp(target,"/battery")){if(!strcmp(method,"GET")){len=battery_json(r,out,cap);if(len>0&&(unsigned)len<cap)snprintf(out+len-1,cap-len+1,",\"configured\":%u}",r->battery_set);return 200;}if(strcmp(method,"POST")||!auth(headers)||!n||n>=300)return 403;
  {char json[300],path[WF_PATH];WifiRuntime candidate=*r;memcpy(json,body,n);json[n]=0;if(battery_parse(&candidate,json))return 422;if(wifi_files_busy(r->files)||r->robot->applied||r->robot->desired||r->robot->pad||r->robot->web)return 409;battery_path(r,path);len=battery_json(&candidate,json,sizeof(json));if(wifi_runtime_replace(r->files,path,json,len))return 507;
   r->battery_set=1;r->battery_capacity=candidate.battery_capacity;r->battery_motors=candidate.battery_motors;r->battery_base_mw=candidate.battery_base_mw;r->battery_motor_ma=candidate.battery_motor_ma;r->battery_eff=candidate.battery_eff;r->battery_uwh=candidate.battery_uwh;r->battery_fraction=0;r->battery_at=r->battery_saved_at=now;r->battery_dirty=0;snprintf(out,cap,"{\"saved\":true}");return 200;}}
 if(!strcmp(target,"/settings")){
  if(!strcmp(method,"GET")||!strcmp(method,"HEAD")){return rp_json(&r->program.config,out,cap)<0?503:200;}
  if(strcmp(method,"POST")){return 405;}if(!auth(headers)){snprintf(out,cap,"Control authorization header required.");return 403;}
  if(r->robot->applied||r->robot->desired||r->robot->pad||r->robot->web||wifi_files_busy(r->files)){snprintf(out,cap,"Stop robot and release buttons before saving.");return 409;}
  if(rp_parse((const char*)body,n,&cfg)){snprintf(out,cap,"Invalid settings. SELECT/START reserved; opposite relays interlocked; max 16 actions.");return 422;}
  {char json[RP_JSON_MAX+1];len=rp_json(&cfg,json,sizeof(json));if(len<0||wifi_runtime_replace(r->files,r->settings,json,len)){wifi_runtime_error(r,now,"Cannot save settings JSON file.");snprintf(out,cap,"Cannot save to MC0; previous settings preserved.");return 507;}}
  /* Publish bindings as one short transaction to the independent pad worker.
     No disk I/O or parsing runs with interrupts disabled. */
#ifdef _EE
  DIntr();
#endif
  r->robot->halt=1;memset(&r->program,0,sizeof(r->program));r->program.config=cfg;r->robot->left=cfg.left;r->robot->right=cfg.right;r->robot->program_mask=0;r->config_error=0;
#ifdef _EE
  EIntr();
#endif
  snprintf(out,cap,"{\"saved\":true}");return 200;
 }
 if(!strcmp(target,"/telemetry")){if(strcmp(method,"GET")&&strcmp(method,"HEAD")){return 405;}
  len=snprintf(out,cap,"{\"logBytes\":%lu,\"logLimit\":%u,\"logWarning\":%u,\"logWriteError\":%u,\"logQueued\":%u,\"logDropped\":%u,\"configError\":%u,\"heapBytes\":%u,\"ramBytes\":%u,\"battery\":null,\"temperature\":null,\"cpu\":null,\"clockValid\":%u,\"batteryEstimated\":%u,\"batterySet\":%u,\"powerMw\":%u,\"motor100msUwh\":%u,\"rxKbps\":%u,\"txKbps\":%u,\"signalValid\":%u,\"signalQuality\":%u}",(unsigned long)r->log_bytes,WIFI_LOG_LIMIT,r->log_warning,r->log_failed,r->queued,r->dropped,r->config_error,r->heap_bytes,r->ram_bytes,r->clock_valid,r->battery_capacity?(unsigned)((uint64_t)r->battery_uwh*100/(r->battery_capacity*11100u)):0,r->battery_set,(unsigned)r->battery_power_mw,(unsigned)r->battery_motor_100ms_uwh,r->rx_kbps,r->tx_kbps,r->signal_valid,r->signal_quality);
  if(len>0&&(unsigned)len<cap){char clean[72];unsigned i;for(i=0;r->last_request[i]&&i<sizeof(clean)-1;i++){unsigned char c=r->last_request[i];clean[i]=(c<32||c=='"'||c=='\\')?' ':c;}clean[i]=0;snprintf(out+len-1,cap-len+1,",\"lastRequest\":\"%s\",\"version\":\"%s\",\"displayEnabled\":%u,\"displayToggles\":%u,\"padConnected\":%u,\"padButtons\":%u}",clean,WIFI_VERSION,r->display_enabled,r->display_toggles,r->pad_connected,r->pad_buttons);}return 200;}
 if(!strcmp(target,"/clock")){char text[32],*end;long long epoch;if(strcmp(method,"POST")||!auth(headers)||!n||n>=sizeof(text))return 403;memcpy(text,body,n);text[n]=0;epoch=strtoll(text,&end,10);if(*end||epoch<946684800LL||epoch>=4102444800LL)return 422;wifi_runtime_clock(r,epoch,now);snprintf(out,cap,"{\"clockSet\":true}");return 200;}
 return 0;
}
