#include "robot_program.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
const char*const rp_names[RP_KEYS]={"CROSS","CIRCLE","SQUARE","TRIANGLE","UP","DOWN","LEFT","RIGHT","L1","R1","L2","R2","L3","R3"};
const uint16_t rp_buttons[RP_KEYS]={0x4000,0x2000,0x8000,0x1000,0x10,0x40,0x80,0x20,0x400,0x800,0x100,0x200,2,4};
const char*rp_action(unsigned i){static const char*const a[]={"stop","wait","on1","on2","on3","on4","on5","on6","on7","on8","off1","off2","off3","off4","off5","off6","off7","off8"};return i<18?a[i]:"";}
int rp_mask_safe(unsigned m){unsigned i;if(m>255)return 0;for(i=0;i<4;i++)if(((m>>(i*2))&3)==3)return 0;return 1;}
void rp_retarget(RpBinding*b,unsigned oldl,unsigned oldr,unsigned newl,unsigned newr){unsigned i;for(i=0;i<b->count;i++){unsigned op=b->op[i],relay,group,base;if(op<2||op>=18)continue;base=op<10?2:10;relay=op-base;group=relay/2;if(group==oldl)group=newl;else if(group==oldr)group=newr;b->op[i]=base+group*2+(relay&1);}}
void rp_default(RobotProgram*p,unsigned l,unsigned r){unsigned i;memset(p,0,sizeof(*p));if(l>3||r>3||l==r){l=0;r=1;}p->config.left=l;p->config.right=r;
 for(i=0;i<4;i++){RpBinding*b=p->config.key+i;b->count=3;b->op[0]=0;b->op[1]=2+l*2+(i==2||i==3);b->op[2]=2+r*2+(i==1||i==3);}}
typedef struct {const char*p,*end;} Json;
static void ws(Json*j){while(j->p<j->end&&isspace((unsigned char)*j->p))j->p++;}
static int token(Json*j,char c){ws(j);if(j->p==j->end||*j->p!=c){return -1;}j->p++;return 0;}
static int string(Json*j,char*s,unsigned cap){unsigned n=0;if(token(j,'"')){return -1;}while(j->p<j->end&&*j->p!='"'){unsigned c=(unsigned char)*j->p++;if(c<32||c=='\\'||c>=127||n+1>=cap){return -1;}s[n++]=c;}s[n]=0;return token(j,'"');}
static int number(Json*j,unsigned*out){unsigned n=0,d=0;ws(j);while(j->p<j->end&&isdigit((unsigned char)*j->p)){if(++d>2){return -1;}n=n*10+*j->p++-'0';}if(!d){return -1;}*out=n;return 0;}
static int binding(Json*j,RpConfig*c,unsigned*seen){char key[32],name[32];unsigned fields=0,index=RP_KEYS,k,n=0;uint8_t ops[RP_STEPS];
 if(token(j,'{')){return -1;}for(;;){if(string(j,key,sizeof(key))||token(j,':')){return -1;}
  if(!strcmp(key,"button")){if(fields&1||string(j,name,sizeof(name))){return -1;}fields|=1;for(k=0;k<RP_KEYS;k++)if(!strcmp(name,rp_names[k]))index=k;}
  else if(!strcmp(key,"actions")){if(fields&2||token(j,'[')){return -1;}fields|=2;ws(j);if(j->p<j->end&&*j->p!=']')for(;;){if(n==RP_STEPS||string(j,name,sizeof(name))){return -1;}for(k=0;k<18&&strcmp(name,rp_action(k));k++);if(k==18){return -1;}ops[n++]=k;ws(j);if(j->p==j->end){return -1;}if(*j->p!=',')break;j->p++;}if(token(j,']')){return -1;}}
  else {return -1;}ws(j);if(j->p==j->end){return -1;}if(*j->p!=',')break;j->p++;
 }if(token(j,'}')||fields!=3||index==RP_KEYS||(*seen&(1u<<index))){return -1;}*seen|=1u<<index;c->key[index].count=n;memcpy(c->key[index].op,ops,n);return 0;
}
int rp_parse(const char*text,unsigned n,RpConfig*out){Json j;RpConfig c;unsigned fields=0,seen=0,k,i,v;char key[32];if(!text||n>RP_JSON_MAX||memchr(text,0,n)){return -1;}memset(&c,0,sizeof(c));c.right=1;j.p=text;j.end=text+n;if(token(&j,'{')){return -1;}
 for(;;){if(string(&j,key,sizeof(key))||token(&j,':')){return -1;}
  if(!strcmp(key,"version")){if(fields&1||number(&j,&v)||v!=1){return -1;}fields|=1;}
  else if(!strcmp(key,"left")||!strcmp(key,"right")){unsigned bit=!strcmp(key,"left")?2:4;if(fields&bit||number(&j,&v)||v>3){return -1;}fields|=bit;if(bit==2)c.left=v;else c.right=v;}
  else if(!strcmp(key,"bindings")){if(fields&8||token(&j,'[')){return -1;}fields|=8;ws(&j);if(j.p<j.end&&*j.p!=']')for(;;){if(binding(&j,&c,&seen)){return -1;}ws(&j);if(j.p==j.end){return -1;}if(*j.p!=',')break;j.p++;}if(token(&j,']')){return -1;}}
  else {return -1;}ws(&j);if(j.p==j.end){return -1;}if(*j.p!=',')break;j.p++;
 }if(token(&j,'}')){return -1;}ws(&j);if(j.p!=j.end||(fields&9)!=9||c.left==c.right){return -1;}
 for(i=0;i<RP_KEYS;i++){unsigned mask=0;for(k=0;k<c.key[i].count;k++){v=c.key[i].op[k];if(!v)mask=0;else if(v>=2&&v<10)mask|=1u<<(v-2);else if(v>=10)mask&=~(1u<<(v-10));if(!rp_mask_safe(mask)){return -1;}}}
 *out=c;return 0;
}
int rp_json(const RpConfig*c,char*out,unsigned cap){unsigned i,k,used=0;int n;
#define ADD(...) do{n=snprintf(out+used,cap>used?cap-used:0,__VA_ARGS__);if(n<0||(unsigned)n>=cap-used){return -1;}used+=n;}while(0)
 ADD("{\"version\":1,\"left\":%u,\"right\":%u,\"bindings\":[",c->left,c->right);
 for(i=0;i<RP_KEYS;i++){ADD("%s{\"button\":\"%s\",\"actions\":[",i?",":"",rp_names[i]);for(k=0;k<c->key[i].count;k++)ADD("%s\"%s\"",k?",":"",rp_action(c->key[i].op[k]));ADD("]}");}ADD("]}");return used;
#undef ADD
}
unsigned rp_tick(RobotProgram*p,uint32_t buttons,int enabled,unsigned applied,int settled,uint32_t now){unsigned i,index=RP_KEYS,count=0;uint32_t selected=0;
 if(enabled)for(i=0;i<RP_KEYS;i++)if((buttons&rp_buttons[i])&&p->config.key[i].count){index=i;count++;selected|=rp_buttons[i];}
 if(count!=1){p->target=0;p->held=0;p->waiting=p->barrier=p->pc=0;p->fault=count>1;return 0;}
 if(selected!=p->held){p->held=selected;p->index=index;p->pc=0;p->target=0;p->barrier=1;p->waiting=p->fault=0;}
 if(p->fault)return 0;
 if(p->barrier){if(applied!=p->target||!settled)return p->target;p->barrier=0;}
 if(p->waiting){if((int32_t)(now-p->until)<0)return p->target;p->waiting=0;}
 while(p->pc<p->config.key[index].count){unsigned op=p->config.key[index].op[p->pc];
  if(op==1){/* A wait begins only after prior relay writes were acknowledged. */
   if(applied!=p->target||!settled){p->barrier=1;return p->target;}p->pc++;p->waiting=1;p->until=now+100;return p->target;}
  p->pc++;if(!op){p->target=0;if(applied||!settled){p->barrier=1;return 0;}}
  else if(op<10)p->target|=1u<<(op-2);else p->target&=~(1u<<(op-10));
  if(!rp_mask_safe(p->target)){p->target=0;p->fault=1;return 0;}
 }return p->target;
}
