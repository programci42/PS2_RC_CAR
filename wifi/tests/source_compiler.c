#include "ps2_compile.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
static struct {uint8_t before[16],arena[262144],after[16];} memory;
static uint32_t seed=42;
static uint32_t rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main(void){char source[3073],error[200];unsigned i,j;static const char chars[]="abcdefghijklmnopqrstuvwxyz0123456789 +-*/%|&^=!<>;(){}\n\t\"\\:$.,#";memset(&memory,0x5a,sizeof(memory));
 assert(ps2_compile("int main(){int x=6;print(x*7);return x*7;}",0,memory.arena,sizeof(memory.arena),0x1400000,error,sizeof(error))>0);
 assert(ps2_compile("li $v0,42\nret\n",1,memory.arena,sizeof(memory.arena),0x1400000,error,sizeof(error))>0);
 assert(ps2_compile("class A {};",0,memory.arena,sizeof(memory.arena),0x1400000,error,sizeof(error))<0&&error[0]);
 for(i=0;i<4000;i++){unsigned n=rnd()%3072;for(j=0;j<n;j++)source[j]=chars[rnd()%(sizeof(chars)-1)];source[n]=0;ps2_compile(source,i&1,memory.arena,sizeof(memory.arena),0x1400000,error,sizeof(error));for(j=0;j<16;j++)assert(memory.before[j]==0x5a&&memory.after[j]==0x5a);}
 printf("SOURCE_COMPILER_HOST_TEST PASS valid C++-subset/assembler, explicit errors, 4000 bounded random sources, canaries intact\n");return 0;}
