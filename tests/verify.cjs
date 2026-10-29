// Compile and exercise the actual application functions, with USB/time/MC stubs.
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const { spawnSync } = require('node:child_process');
const root = path.resolve(__dirname, '..');
const source = fs.readFileSync(path.join(root, 'main.c'), 'utf8');
function fn(name) {
  const match = source.match(new RegExp('static [^\\n]*\\b' + name + '\\([^\\n]*\\)\\r?\\n\\{[\\s\\S]*?\\r?\\n\\}'));
  if (!match) throw Error('Function not found: ' + name);
  return match[0];
}
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ps2-relay-test-'));
const fixture = `
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "relay_policy.h"
#include "seda_relay_rpc.h"
typedef unsigned char u8;
${source.match(/typedef struct \{[\s\S]*?\} AppSettings;/)[0]}
#define DEAD_TIME_MAX_MS 1000
#define SETTINGS_FILE "settings.txt"
#define SETTINGS_DIR "."
#define mkdir(a,b) ((void)0)
static int initialize_memory_card(void) { return 0; }
static AppSettings settings;
#include "ui_locale.h"
static int settings_loaded, usb_send_mode, startup_test_state, drive_inhibited, relay_fault, dead_time_pending;
static int selected_setting, ui_state;
#define UI_NORMAL 0
#define SETTING_COUNT 7
#define DEAD_TIME_STEP_MS 10
#define PAD_SELECT 1
#define PAD_START 8
#define PAD_UP 16
#define PAD_RIGHT 32
#define PAD_DOWN 64
#define PAD_LEFT 128
#define PAD_CROSS 16384
typedef unsigned int u32;
static u8 relay_state[8];
static const u8 relay_on_command[8]={1,2,3,4,5,6,7,8};
static const u8 relay_off_command[8]={17,18,19,20,21,22,23,24};
static int commands[256], count, failure_at, status, waits[64], wait_count;
static int relay_send_command(u8 c) { commands[count++]=c; return count==failure_at ? SEDA_RELAY_ERR_USB : 0; }
static int relay_get_status(void) { return status; }
static void DelayThread(int us) { waits[wait_count++]=us; }
static void show_idle_screen(const char *s) { (void)s; }
static void show_settings_screen(const char *s) { (void)s; }
static void enter_hex_input(int target) { (void)target; }
static void show_relay_event(int a,int b,u8 c) { (void)a;(void)b;(void)c; }
${['hex_value','parse_hex4','hex4_to_string','parse_decimal_after','parse_hex_after','apply_settings','load_settings_from_mc','save_settings_to_mc','handle_settings_input','relay_set','relays_all_off','startup_relay_check','relay_dead_time_wait','apply_motor_relays'].map(fn).join('\n')}
static void reset(void) {
 memset(relay_state,0,sizeof(relay_state)); count=wait_count=failure_at=relay_fault=dead_time_pending=0;
 startup_test_state=0; drive_inhibited=1; status=SEDA_RELAY_STATUS_READY;
 settings=(AppSettings){9,0x178e,0x7d9,0,100,0,1,0};
}
int main(void) {
 int l,r,b,i,checks=0;
 for(l=0;l<4;l++) for(r=0;r<4;r++) for(b=0;b<16;b++) {
   int mask[8], expected[8]={0};
   motor_relay_mask(mask,l,r,b&1,(b>>1)&1,(b>>2)&1,(b>>3)&1);
   if(l!=r) { expected[l*2]=(b&3)==1;expected[l*2+1]=(b&3)==2;expected[r*2]=((b>>2)&3)==1;expected[r*2+1]=((b>>2)&3)==2; }
   assert(memcmp(mask,expected,sizeof(mask))==0); checks++;
   if(l!=r) { assert(next_group(l,r,1)!=r);assert(next_group(l,r,-1)!=r); }
 }
 for(l=-2;l<7;l++) for(r=-2;r<7;r++) { int a=l,c=r;normalize_groups(&a,&c);assert(valid_group(a)&&valid_group(c)&&a!=c);checks++; }
 reset();status=0;startup_relay_check();assert(count==0&&startup_test_state==0);
 status=2;startup_relay_check();assert(count==24&&wait_count==1&&waits[0]==100000&&startup_test_state==1&&drive_inhibited);
 for(i=0;i<8;i++){assert(commands[i]==17+i);assert(commands[8+i]==1+i);assert(commands[16+i]==17+i);assert(!relay_state[i]);}
 startup_relay_check();assert(count==24);checks++;
 for(i=1;i<=24;i++) {reset();failure_at=i;startup_relay_check();assert(startup_test_state==-1&&drive_inhibited);int old=count;apply_motor_relays(1,0,1,0);assert(count==old);checks++;}
 reset();assert(relays_all_off()==0&&count==8);checks++;
 for(l=0;l<4;l++) for(r=0;r<4;r++) if(l!=r) {
   reset();settings.left_group=l;settings.right_group=r;startup_test_state=1;
   apply_motor_relays(1,0,1,0);assert(relay_state[l*2]&&relay_state[r*2]);
   count=wait_count=0;apply_motor_relays(0,1,0,1);
   assert(count==4&&commands[0]>=17&&commands[1]>=17&&commands[2]<=8&&commands[3]<=8);
   assert(wait_count==1&&waits[0]==100000&&relay_state[l*2+1]&&relay_state[r*2+1]);checks++;
 }
 reset();startup_test_state=1;apply_motor_relays(1,0,1,0);failure_at=count+1;apply_motor_relays(0,1,0,1);assert(drive_inhibited);for(i=0;i<8;i++)assert(!relay_state[i]);checks++;
 reset();startup_test_state=1;failure_at=1;apply_motor_relays(1,0,1,0);assert(drive_inhibited);for(i=0;i<8;i++)assert(!relay_state[i]);checks++;
 reset();settings.left_group=3;settings.right_group=2;settings.language=1;settings.dead_time_ms=250;
 assert(save_settings_to_mc()==0);reset();assert(load_settings_from_mc()==0);assert(settings.left_group==3&&settings.right_group==2&&settings.language==1&&settings.dead_time_ms==250);checks++;
 FILE *f=fopen(SETTINGS_FILE,"wb");assert(f);fputs("ROLECFG v1\\nmode=9\\nvid=178e\\npid=07d9\\nautoload=0\\ndeadms=100\\n",f);fclose(f);
 reset();assert(load_settings_from_mc()==0);assert(settings.left_group==0&&settings.right_group==1&&settings.language==0);checks++;
 settings.left_group=3;settings.right_group=3;settings.language=9;settings.dead_time_ms=9999;apply_settings();assert(settings.left_group==3&&settings.right_group==0&&settings.language==0&&settings.dead_time_ms==1000);checks++;
 assert(strcmp(ui_translate("SETTINGS\\n\\n",1),"AYARLAR\\n\\n")==0);checks++;
 for(l=0;l<4;l++) for(r=0;r<4;r++) if(l!=r) for(b=0;b<2;b++) {
  reset();settings.left_group=l;settings.right_group=r;selected_setting=4;
  int seen=0;for(i=0;i<3;i++){handle_settings_input(b?PAD_LEFT:PAD_RIGHT);assert(settings.left_group!=r);seen|=1<<settings.left_group;}
  assert(seen==(15^(1<<r))&&settings.left_group==l);checks++;
  selected_setting=5;seen=0;for(i=0;i<3;i++){handle_settings_input(b?PAD_LEFT:PAD_RIGHT);assert(settings.right_group!=l);seen|=1<<settings.right_group;}
  assert(seen==(15^(1<<l))&&settings.right_group==r);checks++;
 }
 reset();selected_setting=6;handle_settings_input(PAD_RIGHT);assert(settings.language==1);handle_settings_input(PAD_LEFT);assert(settings.language==0);checks++;
 printf("PASS: %d relay, startup/failure, direction-change and configuration cases\\n", checks);
 return 0;
}
`;
fs.writeFileSync(path.join(dir, 'verify.c'), fixture);
const cc = process.env.CC || (process.platform === 'win32' ? 'C:/msys64/mingw64/bin/gcc.exe' : 'cc');
const env = { ...process.env, PATH: path.dirname(cc) + path.delimiter + process.env.PATH };
const exe = path.join(dir, process.platform === 'win32' ? 'verify.exe' : 'verify');
for (const [command,args] of [[cc,['-std=c99','-Wall','-Wextra','-Wno-unused-function','-I',root,path.join(dir,'verify.c'),'-o',exe]],[exe,[]]]) {
 const result = spawnSync(command,args,{cwd:dir,env,encoding:'utf8'});
 if(result.stdout) process.stdout.write(result.stdout);
 if(result.stderr) process.stderr.write(result.stderr);
 if(result.status!==0) {console.error('Test files:',dir);process.exit(result.status||1);}
}
// All translated printf placeholders must preserve their types and order.
const table=fs.readFileSync(path.join(root,'ui_locale.h'),'utf8');
let translations=0;
for(const m of table.matchAll(/\{"((?:[^"\\]|\\.)*)","((?:[^"\\]|\\.)*)"\}/g)) {
 const spec=s=>[...s.matchAll(/%(?:[-+0 #]*\d*(?:\.\d+)?[diuxXscf]|%)/g)].map(x=>x[0].slice(-1)).join(',');
 if(spec(m[1])!==spec(m[2])) throw Error('Translation format mismatch: '+m[1]);
 translations++;
}
console.log('PASS: '+translations+' English/Turkish translation format pairs');
