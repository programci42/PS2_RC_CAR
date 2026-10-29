#include "ps2_compile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define V 2
#define A 4
#define T 8
#define API 16
#define FP 17
#define SP 29
#define RA 31
typedef struct {const char*s,*p;uint8_t*out;unsigned cap,n,data,base,depth;int token,value,failed;char text[256],name[32][32];unsigned vars;char*error;unsigned errorcap;} C;
enum {ID=256,NUM,STR,EQ,NE,LE,GE,SHL,SHR,AND,OR};
static void fail(C*c,const char*m){if(!c->failed)snprintf(c->error,c->errorcap,"Position %u: %s",(unsigned)(c->p-c->s),m);c->failed=1;}
static unsigned word(unsigned op,unsigned rs,unsigned rt,unsigned imm){return op<<26|rs<<21|rt<<16|(imm&65535);}
static unsigned reg(unsigned rs,unsigned rt,unsigned rd,unsigned shift,unsigned fn){return rs<<21|rt<<16|rd<<11|shift<<6|fn;}
static void emit(C*c,uint32_t w){if(c->n+4>c->data||c->n>=65536){fail(c,"Code area is full");return;}c->out[c->n++]=w;c->out[c->n++]=w>>8;c->out[c->n++]=w>>16;c->out[c->n++]=w>>24;}
static void li(C*c,unsigned r,unsigned v){emit(c,word(15,0,r,v>>16));emit(c,word(13,r,r,v));}
static unsigned branch(C*c,unsigned op,unsigned rs,unsigned rt){unsigned p=c->n;emit(c,word(op,rs,rt,0));emit(c,0);return p;}
static void patch(C*c,unsigned p,unsigned to){int d;if(c->failed||p+4>c->cap)return;d=((int)to-(int)p-4)/4;if(d<-32768||d>32767){fail(c,"Branch target out of range");return;}c->out[p]=d;c->out[p+1]=d>>8;}
static void prologue(C*c){emit(c,word(9,SP,SP,-256));emit(c,word(43,SP,RA,252));emit(c,word(43,SP,API,248));emit(c,word(43,SP,FP,244));emit(c,reg(SP,0,FP,0,33));emit(c,reg(A,0,API,0,33));}
static void epilogue(C*c){emit(c,word(35,FP,RA,252));emit(c,word(35,FP,API,248));emit(c,reg(FP,0,SP,0,33));emit(c,word(35,SP,FP,244));emit(c,word(9,SP,SP,256));emit(c,reg(RA,0,0,0,8));emit(c,0);}
static void next(C*c){const char*p=c->p;unsigned k;for(;;){while(isspace((unsigned char)*p))p++;if(p[0]=='/'&&p[1]=='/'){while(*p&&*p!='\n')p++;continue;}if(p[0]=='/'&&p[1]=='*'){p+=2;while(*p&&!(p[0]=='*'&&p[1]=='/'))p++;if(!*p){c->p=p;fail(c,"Unterminated comment");break;}p+=2;continue;}break;}
 c->text[0]=0;c->token=(unsigned char)*p;if(!*p){c->p=p;return;}
 if(isalpha((unsigned char)*p)||*p=='_'){for(k=0;isalnum((unsigned char)*p)||*p=='_';p++){if(k+1<sizeof(c->text))c->text[k++]=*p;}c->text[k]=0;c->token=ID;}
 else if(isdigit((unsigned char)*p)){char*end;c->value=(int)strtoul(p,&end,0);p=end;c->token=NUM;}
 else if(*p=='"'){p++;for(k=0;*p&&*p!='"';p++){int ch=(unsigned char)*p;if(ch=='\\'){p++;ch=*p;if(!ch)break;if(ch=='n')ch='\n';else if(ch=='r')ch='\r';else if(ch=='t')ch='\t';}if(k+1<sizeof(c->text))c->text[k++]=ch;else fail(c,"Text cannot exceed 255 bytes");}c->text[k]=0;if(*p=='"')p++;else fail(c,"Unterminated string");c->token=STR;}
 else{int t=0;if(p[1]){if(p[0]=='='&&p[1]=='=')t=EQ;else if(p[0]=='!'&&p[1]=='=')t=NE;else if(p[0]=='<'&&p[1]=='=')t=LE;else if(p[0]=='>'&&p[1]=='=')t=GE;else if(p[0]=='<'&&p[1]=='<')t=SHL;else if(p[0]=='>'&&p[1]=='>' )t=SHR;else if(p[0]=='&'&&p[1]=='&')t=AND;else if(p[0]=='|'&&p[1]=='|')t=OR;}if(t){c->token=t;p+=2;}else p++;}c->p=p;}
static int is(C*c,const char*s){return c->token==ID&&!strcmp(c->text,s);}
static void need(C*c,int t){if(c->token!=t){fail(c,"Unexpected token");return;}next(c);}
static int var(C*c,const char*n){unsigned i;for(i=0;i<c->vars;i++)if(!strcmp(c->name[i],n))return i;return -1;}
static void string_value(C*c,const char*s){unsigned n=strlen(s)+1;if(c->data<n||c->data-n<=c->n+16){fail(c,"String area is full");return;}c->data-=n;memcpy(c->out+c->data,s,n);li(c,V,c->base+c->data);}
static void call(C*c,unsigned offset){emit(c,reg(V,0,A,0,33));emit(c,word(35,API,25,offset));emit(c,reg(25,0,RA,0,9));emit(c,0);}
static int prec(int t){switch(t){case OR:return 1;case AND:return 2;case '|':return 3;case '^':return 4;case '&':return 5;case EQ:case NE:return 6;case '<':case '>':case LE:case GE:return 7;case SHL:case SHR:return 8;case '+':case '-':return 9;case '*':case '/':case '%':return 10;default:return 0;}}
static void expr(C*,int);
static void atom(C*c){char name[32];int i,t=c->token;if(++c->depth>24){fail(c,"Expression nesting too deep");c->depth--;return;}
 if(t==NUM){li(c,V,c->value);next(c);}
 else if(t==STR){string_value(c,c->text);next(c);}
 else if(t=='('){next(c);expr(c,1);need(c,')');}
 else if(t=='-'||t=='!'||t=='~'||t=='+'){next(c);atom(c);if(t=='-')emit(c,reg(0,V,V,0,35));else if(t=='!')emit(c,word(11,V,V,1));else if(t=='~')emit(c,reg(V,0,V,0,39));}
 else if(t==ID){snprintf(name,sizeof(name),"%.31s",c->text);next(c);if(c->token=='('){unsigned offset=0;int text=0;
  if(!strcmp(name,"print")||!strcmp(name,"number"))offset=8;else if(!strcmp(name,"yield_ms"))offset=16;else if(!strcmp(name,"millis"))offset=12;else{fail(c,"Unsupported function");goto done;}
  next(c);if(offset!=12){text=c->token==STR;expr(c,1);}need(c,')');if(text&&offset==8)offset=4;call(c,offset);if(offset!=12)li(c,V,0);
 }else if(!strcmp(name,"true")||!strcmp(name,"false"))li(c,V,!strcmp(name,"true"));else{i=var(c,name);if(i<0)fail(c,"Undefined variable");else emit(c,word(35,FP,V,i*4));}}
 else fail(c,"Expected expression");
 done:c->depth--;}
static void expr(C*c,int min){int op,p;atom(c);while(!c->failed&&(p=prec(c->token))>=min&&p){unsigned b,end;op=c->token;next(c);
 if(op==AND||op==OR){b=branch(c,op==AND?4:5,V,0);expr(c,p+1);emit(c,reg(0,V,V,0,43));end=branch(c,4,0,0);patch(c,b,c->n);li(c,V,op==OR);patch(c,end,c->n);continue;}
 emit(c,word(9,SP,SP,-16));emit(c,word(43,SP,V,0));expr(c,p+1);emit(c,word(35,SP,T,0));emit(c,word(9,SP,SP,16));
 switch(op){case '+':emit(c,reg(T,V,V,0,33));break;case '-':emit(c,reg(T,V,V,0,35));break;case '*':emit(c,reg(T,V,0,0,24));emit(c,reg(0,0,V,0,18));break;case '/':case '%':emit(c,reg(T,V,0,0,26));emit(c,reg(0,0,V,0,op=='/'?18:16));break;
 case '&':case '|':case '^':emit(c,reg(T,V,V,0,op=='&'?36:op=='|'?37:38));break;case SHL:case SHR:emit(c,reg(V,T,V,0,op==SHL?4:7));break;
 case EQ:case NE:emit(c,reg(T,V,V,0,38));if(op==EQ)emit(c,word(11,V,V,1));else emit(c,reg(0,V,V,0,43));break;
 default:emit(c,reg(op=='>'||op==LE?V:T,op=='>'||op==LE?T:V,V,0,42));if(op==LE||op==GE)emit(c,word(14,V,V,1));break;}}
}
static int assignment(C*c){C copy=*c;next(&copy);return copy.token=='=';}
static void statement(C*c){unsigned b,e,start;int i;char name[32];if(++c->depth>24){fail(c,"Block nesting too deep");c->depth--;return;}
 if(c->token=='{'){next(c);while(c->token&&c->token!='}'&&!c->failed)statement(c);need(c,'}');}
 else if(is(c,"return")){next(c);expr(c,1);need(c,';');epilogue(c);}
 else if(is(c,"if")){next(c);need(c,'(');expr(c,1);need(c,')');b=branch(c,4,V,0);statement(c);if(is(c,"else")){e=branch(c,4,0,0);patch(c,b,c->n);next(c);statement(c);patch(c,e,c->n);}else patch(c,b,c->n);}
 else if(is(c,"while")){next(c);start=c->n;need(c,'(');expr(c,1);need(c,')');b=branch(c,4,V,0);statement(c);e=branch(c,4,0,0);patch(c,e,start);patch(c,b,c->n);}
 else if(is(c,"int")||is(c,"bool")){next(c);if(c->token!=ID){fail(c,"Variable name required");goto done;}snprintf(name,sizeof(name),"%.31s",c->text);if(c->vars==32||var(c,name)>=0){fail(c,"Variable limit exceeded or duplicate name");goto done;}i=c->vars++;strcpy(c->name[i],name);next(c);if(c->token=='='){next(c);expr(c,1);}else li(c,V,0);emit(c,word(43,FP,V,i*4));need(c,';');}
 else if(c->token==';')next(c);
 else{if(c->token==ID&&assignment(c)){snprintf(name,sizeof(name),"%.31s",c->text);next(c);if(c->token=='='){next(c);i=var(c,name);if(i<0)fail(c,"Undefined variable");expr(c,1);emit(c,word(43,FP,V,(unsigned)i*4));need(c,';');goto done;}}expr(c,1);need(c,';');}
 done:c->depth--;}
static int rr(const char*s){static const char*n[]={"zero","at","v0","v1","a0","a1","a2","a3","t0","t1","t2","t3","t4","t5","t6","t7","s0","s1","s2","s3","s4","s5","s6","s7","t8","t9","k0","k1","gp","sp","fp","ra"};int i;if(*s=='$')s++;for(i=0;i<32;i++)if(!strcmp(s,n[i]))return i;if(isdigit((unsigned char)*s)){char*end;long v=strtol(s,&end,10);if(!*end&&v>=0&&v<32)return v;}return -1;}
typedef struct{char name[48];unsigned at;} Label;
static void assembly(C*c){Label labels[128];unsigned count=0,pass;const char*source=c->s;unsigned start=c->n;
 for(pass=0;pass<2&&!c->failed;pass++){const char*p=source;c->n=start;while(*p&&!c->failed){char line[512],op[32],a[64]="",b[64]="",d[64]="";unsigned k=0,j;int r1,r2,r3;long imm=0;char*colon,*s;
  while(*p&&*p!='\n'){if(k+1<sizeof(line))line[k++]=*p;p++;}if(*p)p++;line[k]=0;c->p=p;s=line;while(isspace((unsigned char)*s))s++;if(!*s||*s=='#'||(s[0]=='/'&&s[1]=='/'))continue;
  colon=strchr(s,':');if(colon&&strcspn(s," \t\"")>=(unsigned)(colon-s)){*colon=0;if(!pass){if(count>=128){fail(c,"Label limit exceeded");break;}for(j=0;j<count;j++)if(!strcmp(labels[j].name,s))fail(c,"Duplicate label");snprintf(labels[count].name,48,"%.47s",s);labels[count++].at=c->n;}s=colon+1;while(isspace((unsigned char)*s))s++;if(!*s)continue;}
  if(*s=='.'&&strncmp(s,".word",5))continue;
  if(!strncmp(s,"text ",5)){char*str=strchr(s,'"');if(!str){fail(c,"text requires a string");break;}C t=*c;t.p=str;next(&t);if(t.token!=STR){fail(c,"Invalid text");break;}if(!pass){li(c,V,0);}else string_value(c,t.text);call(c,4);continue;}
  for(j=0;s[j];j++){if(s[j]=='#'){s[j]=0;break;}if(s[j]==','||s[j]=='('||s[j]==')')s[j]=' ';}
  if(sscanf(s,"%31s %63s %63s %63s",op,a,b,d)<1)continue;
  r1=rr(a);r2=rr(b);r3=rr(d);imm=strtol(b,0,0);
  if(!strcmp(op,"nop"))emit(c,0);else if(!strcmp(op,"ret"))epilogue(c);
  else if(!strcmp(op,".word"))emit(c,(uint32_t)strtoul(a,0,0));
  else if(!strcmp(op,"li")&&r1>=0)li(c,r1,imm);
  else if(!strcmp(op,"move")&&r1>=0&&r2>=0)emit(c,reg(r2,0,r1,0,33));
  else if(!strcmp(op,"print")&&r1>=0){emit(c,reg(r1,0,V,0,33));call(c,8);}
  else if((!strcmp(op,"addu")||!strcmp(op,"subu")||!strcmp(op,"and")||!strcmp(op,"or")||!strcmp(op,"xor")||!strcmp(op,"slt"))&&r1>=0&&r2>=0&&r3>=0)emit(c,reg(r2,r3,r1,0,!strcmp(op,"addu")?33:!strcmp(op,"subu")?35:!strcmp(op,"and")?36:!strcmp(op,"or")?37:!strcmp(op,"xor")?38:42));
  else if((!strcmp(op,"addiu")||!strcmp(op,"ori")||!strcmp(op,"andi")||!strcmp(op,"xori"))&&r1>=0&&r2>=0)emit(c,word(!strcmp(op,"addiu")?9:!strcmp(op,"ori")?13:!strcmp(op,"andi")?12:14,r2,r1,strtol(d,0,0)));
  else if(!strcmp(op,"lui")&&r1>=0)emit(c,word(15,0,r1,imm));
  else if((!strcmp(op,"sll")||!strcmp(op,"srl")||!strcmp(op,"sra"))&&r1>=0&&r2>=0)emit(c,reg(0,r2,r1,strtol(d,0,0)&31,!strcmp(op,"sll")?0:!strcmp(op,"srl")?2:3));
  else if((!strcmp(op,"lw")||!strcmp(op,"sw")||!strcmp(op,"lb")||!strcmp(op,"lbu")||!strcmp(op,"sb"))&&r1>=0&&r3>=0)emit(c,word(!strcmp(op,"lw")?35:!strcmp(op,"sw")?43:!strcmp(op,"lb")?32:!strcmp(op,"lbu")?36:40,r3,r1,imm));
  else if(!strcmp(op,"mult")&&r1>=0&&r2>=0)emit(c,reg(r1,r2,0,0,24));
  else if((!strcmp(op,"mflo")||!strcmp(op,"mfhi"))&&r1>=0)emit(c,reg(0,0,r1,0,!strcmp(op,"mflo")?18:16));
  else if(!strcmp(op,"b")||!strcmp(op,"beq")||!strcmp(op,"bne")){const char*target=!strcmp(op,"b")?a:d;unsigned at;if(strcmp(op,"b")&&(r1<0||r2<0)){fail(c,"Invalid branch register");break;}at=branch(c,!strcmp(op,"bne")?5:4,!strcmp(op,"b")?0:r1,!strcmp(op,"b")?0:r2);if(pass){for(j=0;j<count;j++)if(!strcmp(labels[j].name,target))break;if(j==count)fail(c,"Label not found");else patch(c,at,labels[j].at);}}
  else fail(c,"Unsupported MIPS instruction/register");
 }}
}
int ps2_compile(const char*source,int asm_mode,uint8_t*out,unsigned cap,uint32_t base,char*error,unsigned errorcap){C c;memset(&c,0,sizeof(c));c.s=c.p=source;c.out=out;c.cap=c.data=cap;c.base=base;c.error=error;c.errorcap=errorcap;if(!source||!out||cap<4096||!error||!errorcap)return -1;error[0]=0;prologue(&c);
 if(asm_mode)assembly(&c);else{next(&c);if(is(&c,"int")){C save=c;next(&c);if(is(&c,"main")){next(&c);need(&c,'(');need(&c,')');}else c=save;}while(c.token&&!c.failed)statement(&c);}
 li(&c,V,0);epilogue(&c);return c.failed?-1:(int)c.n;
}
