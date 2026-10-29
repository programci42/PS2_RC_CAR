/* Public synthetic seed states; tests the actual bootstrap/ratchet policy. */
#include "rtl_nonce.h"
#include <stdio.h>
#include <string.h>
typedef struct {uint8_t data[64];int exists,read_error,write_error,create_error,corrupt;unsigned reads,writes,creates;} Store;
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){printf("NONCE_SETUP_TEST FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int rd(void*p,uint8_t*out){Store*s=p;s->reads++;if(s->read_error)return s->read_error;if(!s->exists)return RTL_NONCE_MISSING;memcpy(out,s->data,64);return 0;}
static int wr(void*p,const uint8_t*data){Store*s=p;s->writes++;if(s->write_error)return s->write_error;if(!s->exists)return RTL_NONCE_MISSING;memcpy(s->data,data,64);if(s->corrupt)s->data[0]^=1;return 0;}
static int cr(void*p,const uint8_t*data){Store*s=p;s->creates++;if(s->exists)return RTL_NONCE_INVALID;if(s->create_error)return s->create_error;s->exists=1;memcpy(s->data,data,64);return 0;}
static void init(Store*s,int exists){unsigned i;memset(s,0,sizeof(*s));s->exists=exists;for(i=0;i<64;i++)s->data[i]=(uint8_t)(i+1);}
static int zero(const uint8_t*p,unsigned n){unsigned v=0;while(n--)v|=*p++;return !v;}
int nonce_setup_test(void){Store target,source;uint8_t a[32],b[32],c[32],saved[64];
    RtlNonceIo t={&target,rd,wr,cr},s={&source,rd,wr,0};
    init(&target,1);init(&source,1);memcpy(saved,target.data,64);
    CHECK(!rtl_nonce_setup(&t,&s,a));CHECK(!zero(a,32)&&!source.reads&&!source.writes&&!target.creates&&target.writes==1);
    CHECK(memcmp(saved,target.data,64));CHECK(!rtl_nonce_setup(&t,0,b)&&memcmp(a,b,32));
    init(&target,0);init(&source,1);CHECK(!rtl_nonce_setup(&t,&s,a));
    CHECK(target.exists&&target.creates==1&&target.writes==1&&source.writes==1);
    CHECK(memcmp(target.data,source.data,64));CHECK(!rtl_nonce_setup(&t,&s,b)&&memcmp(a,b,32));
    /* Lost card after multiple sessions: a newly consumed source must create
     * a different child chain, never an older state of the old MC chain. */
    target.exists=0;CHECK(!rtl_nonce_setup(&t,&s,c));CHECK(memcmp(c,a,32)&&memcmp(c,b,32));
    init(&target,0);init(&source,1);source.write_error=RTL_NONCE_WRITE;
    memset(a,255,32);CHECK(rtl_nonce_setup(&t,&s,a)==RTL_NONCE_WRITE);CHECK(!target.exists&&!target.creates&&zero(a,32));
    init(&target,0);init(&source,1);target.create_error=RTL_NONCE_WRITE;
    CHECK(rtl_nonce_setup(&t,&s,a)==RTL_NONCE_WRITE);CHECK(!target.exists&&source.writes==1&&zero(a,32));
    target.create_error=0;CHECK(!rtl_nonce_setup(&t,&s,a)&&source.writes==2);
    init(&target,0);init(&source,1);target.write_error=RTL_NONCE_WRITE;
    CHECK(rtl_nonce_setup(&t,&s,a)==RTL_NONCE_WRITE&&target.exists&&zero(a,32));
    target.write_error=0;source.reads=0;CHECK(!rtl_nonce_setup(&t,&s,a)&&!source.reads);
    init(&target,1);init(&source,1);target.corrupt=1;
    CHECK(rtl_nonce_setup(&t,&s,a)==RTL_NONCE_VERIFY&&zero(a,32)&&!source.reads&&!target.creates);
    init(&target,1);init(&source,1);memset(target.data,0,64);
    CHECK(rtl_nonce_setup(&t,&s,a)==RTL_NONCE_INVALID&&zero(a,32)&&!source.reads&&!target.writes&&!target.creates);
    init(&target,1);init(&source,1);target.read_error=RTL_NONCE_READ;
    CHECK(rtl_nonce_setup(&t,&s,a)==RTL_NONCE_READ&&!source.reads&&!target.creates&&zero(a,32));
    init(&target,0);init(&source,0);CHECK(rtl_nonce_setup(&t,&s,a)==RTL_NONCE_NO_SOURCE&&zero(a,32));
    CHECK(strstr(rtl_nonce_error(RTL_NONCE_NO_SOURCE),"Wifi.seed")!=0);
    printf("NONCE_SETUP_TEST PASS checks=%u bootstrap-no-overwrite-verified-persistence\n",checks);return 0;
}
