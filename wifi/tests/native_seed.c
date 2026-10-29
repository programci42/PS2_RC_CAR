/* Actual EE native MC/USB adapter with deterministic public RPC doubles. */
#define RTL_SEED_HOST_TEST
#include "../ee/wifi_seed.c"
#include <stdlib.h>
unsigned char iomanx_irx[1],filexio_irx[1],bdm_irx[1],fatfs_irx[1],usbmass_irx[1];
unsigned int size_iomanx_irx=1,size_filexio_irx=1,size_bdm_irx=1,size_fatfs_irx=1,size_usbmass_irx=1;
static uint8_t model_mc[64],model_usb[64];
static uint8_t dma_mc[128],dma_usb[128];static int mc_mode,usb_mode;
static uint8_t dma_known[4160];static void *known_dma_address;static int csv_exists;
static const char csv_text[]="SSID;Sifre\r\nIEEE;password\r\n";
static int exists_mc,exists_usb,type_mc=2,format_mc=1,model_size=64,position,queued_result;
static int write_fail,timeout_mc;static unsigned modules,creates,usb_reads,checks;
#define CHECK(x) do{checks++;if(!(x)){printf("NATIVE_SEED_TEST FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int mcSync(int mode,int*cmd,int*result){(void)mode;*cmd=0;if(timeout_mc)return 0;*result=queued_result;return 1;}
int mcGetInfo(int port,int slot,int*type,int*free,int*format){CHECK(port==mc_port&&slot==0);*type=type_mc;*free=128;*format=format_mc;queued_result=format_mc?0:-2;return 0;}
int mcOpen(int port,int slot,const char*path,int mode){CHECK(port==mc_port&&!slot);position=0;
    if(mc_known_path[0]&&!strcmp(path,mc_known_path)){CHECK(mode==FIO_O_RDONLY);mc_mode=mode;queued_result=csv_exists?24:-4;return 0;}
    CHECK(!strcmp(path,mc_seed_path));
    CHECK(mode==FIO_O_RDONLY||mode==FIO_O_RDWR||mode==(FIO_O_RDWR|FIO_O_CREAT));mc_mode=mode;
    if(exists_mc)queued_result=99;
    else if(mode&FIO_O_CREAT){exists_mc=1;model_size=0;creates++;queued_result=99;}
    else queued_result=-4;
    return 0;}
int mcRead(int fd,void*out,int size){int n=model_size-position;
    if(fd==24){CHECK(out==dma_known&&size==4097);memcpy(out,csv_text,sizeof(csv_text)-1);queued_result=sizeof(csv_text)-1;return 0;}
    CHECK(fd==99&&mc_mode==FIO_O_RDONLY&&out==dma_mc);if(n>size)n=size;if(n<0)n=0;memcpy(out,model_mc+position,n);position+=n;queued_result=n;return 0;}
int mcWrite(int fd,const void*data,int size){CHECK(fd==99&&size==64&&position==0&&(mc_mode&FIO_O_WRONLY));if(write_fail)queued_result=-3;else{memcpy(model_mc,data,64);model_size=64;position=64;queued_result=64;}return 0;}
int mcSeek(int fd,int offset,int origin){CHECK(fd==99);position=origin==SEEK_END?model_size+offset:offset;queued_result=position;return 0;}
int mcFlush(int fd){CHECK(fd==99);queued_result=0;return 0;}
int mcClose(int fd){CHECK(fd==99||fd==24);queued_result=0;return 0;}
int mcMkDir(int port,int slot,const char*path){CHECK(!port&&!slot&&!strcmp(path,"/RTL_WIFI"));queued_result=0;return 0;}
void DelayThread(unsigned us){(void)us;}
int SifExecModuleBuffer(void*p,unsigned n,int a,const char*v,int*r){(void)p;(void)n;(void)a;(void)v;*r=0;modules++;return 1;}
int fileXioInit(void){return 0;}
int fileXioOpen(const char*path,int mode,int flags){(void)flags;CHECK(mode==FIO_O_RDONLY||mode==FIO_O_RDWR);usb_mode=mode;
    if(csv_exists&&!strcmp(path,"mass:/Release/bilinenaglar.csv"))return 23;
    return exists_usb&&!strcmp(path,"mass:/Release/Wifi.seed")?22:-2;}
int fileXioRead(int fd,void*out,int size){
    if(fd==23){CHECK(size==4097&&usb_mode==FIO_O_RDONLY&&out==known_dma_address);memcpy(dma_known,csv_text,sizeof(csv_text)-1);return sizeof(csv_text)-1;}
    CHECK(fd==22&&size==65&&usb_mode==FIO_O_RDONLY&&out==usb_buffer);memcpy(dma_usb,model_usb,64);usb_reads++;return 64;}
int fileXioWrite(int fd,void*data,int size){CHECK(fd==22&&size==64&&usb_mode==FIO_O_RDWR);memcpy(model_usb,data,64);return 64;}
int fileXioClose(int fd){CHECK(fd==22||fd==23);return 0;}
void *seed_dma_alias(void *p){CHECK(p==mc_buffer||p==usb_buffer||p==known_dma_address);return p==mc_buffer?dma_mc:(p==usb_buffer?dma_usb:dma_known);}
void SyncDCache(void *start,void *end){
    if(start!=mc_buffer&&start!=usb_buffer){CHECK((uint8_t*)end-(uint8_t*)start==4160);known_dma_address=start;return;}
    CHECK((uint8_t*)end-(uint8_t*)start==128);}
static void reset(void){unsigned i;ready=mc_fault=usb_ready=usb_attempted=card_seen_formatted=0;
    launch_seed[0]=source_path[0]=0;exists_mc=0;exists_usb=1;type_mc=2;format_mc=1;model_size=64;
    write_fail=timeout_mc=csv_exists=0;known_dma_address=0;modules=creates=usb_reads=0;for(i=0;i<64;i++){model_usb[i]=(uint8_t)(i+1);model_mc[i]=0;}}
int main(void){char *argv[]={"mass:/Release/Wifi.elf"};uint8_t a[32],b[32],old[64];
    reset();wifi_seed_init(1,1,argv);CHECK(!modules);CHECK(!wifi_seed_nonce(0,a));
    CHECK(exists_mc&&model_size==64&&modules==5&&creates==1&&usb_reads>=2);
    memcpy(old,model_mc,64);usb_reads=0;CHECK(!wifi_seed_nonce(0,b));
    CHECK(memcmp(a,b,32)&&memcmp(old,model_mc,64)&&!usb_reads&&creates==1);
    reset();exists_mc=1;model_size=63;wifi_seed_init(1,1,argv);
    CHECK(wifi_seed_nonce(0,a)==RTL_NONCE_INVALID&&!creates&&!modules);
    reset();type_mc=0;wifi_seed_init(1,1,argv);CHECK(wifi_seed_nonce(0,a)==RTL_NONCE_NO_CARD&&!creates&&!modules);
    reset();format_mc=0;wifi_seed_init(1,1,argv);CHECK(wifi_seed_nonce(0,a)==RTL_NONCE_UNFORMATTED&&!creates);
    reset();exists_usb=0;wifi_seed_init(1,1,argv);CHECK(wifi_seed_nonce(0,a)==RTL_NONCE_NO_SOURCE&&!creates);
    reset();write_fail=1;wifi_seed_init(1,1,argv);CHECK(wifi_seed_nonce(0,a)==RTL_NONCE_WRITE&&creates==1);
    reset();timeout_mc=1;wifi_seed_init(1,1,argv);CHECK(wifi_seed_nonce(0,a)==RTL_NONCE_IO_BUSY&&mc_fault&&!creates);
    {uint8_t csv[4096];int n;reset();csv_exists=1;wifi_seed_init(1,1,argv);
     n=wifi_seed_read_known(csv,sizeof(csv));CHECK(n==(int)sizeof(csv_text)-1&&!memcmp(csv,csv_text,n)&&modules==5&&!creates);
     CHECK(wifi_seed_read_known(csv,sizeof(csv))==n&&modules==5);
     CHECK(wifi_seed_read_known(csv,16)==-1);}
    {char *mcargv[]={"mc0:/Wifi/Wifi.elf"};uint8_t csv[4096];unsigned i;
     reset();exists_mc=1;csv_exists=1;for(i=0;i<64;i++)model_mc[i]=(uint8_t)(i+1);
     wifi_seed_init(1,1,mcargv);CHECK(launch_on_mc&&!strcmp(mc_seed_path,"/Wifi/RTL_WIFI/SEED.DAT"));
     CHECK(wifi_seed_read_known(csv,sizeof(csv))==(int)sizeof(csv_text)-1&&!memcmp(csv,csv_text,sizeof(csv_text)-1));
     CHECK(!wifi_seed_nonce(0,a)&&!modules&&!usb_reads&&!creates);
     {char source[256];CHECK(!wifi_seed_known_path(source,sizeof(source))&&!strcmp(source,"mc0:/Wifi/bilinenaglar.csv"));}
     mcargv[0]="mc1:/Wifi/Wifi.elf";wifi_seed_init(1,1,mcargv);CHECK(mc_port==1&&launch_on_mc);
     CHECK(wifi_seed_read_known(csv,sizeof(csv))==(int)sizeof(csv_text)-1&&!memcmp(csv,csv_text,sizeof(csv_text)-1));{char source[256];CHECK(!wifi_seed_known_path(source,sizeof(source))&&!strcmp(source,"mc1:/Wifi/bilinenaglar.csv"));}}
    printf("NATIVE_SEED_TEST PASS checks=%u actual-EE-adapter mocked-MC-USB\n",checks);return 0;
}
