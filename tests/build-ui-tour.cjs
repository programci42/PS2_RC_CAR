// A separate emulator-only screen tour. Never install this ELF on the vehicle.
// Uses actual rendering functions from main.c; USB, controller and MC are not exercised.
const fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'..');
const src=fs.readFileSync(path.join(root,'main.c'),'utf8');
const fn=name=>{
 const m=src.match(new RegExp('static [^\\n]*\\b'+name+'\\([^\\n]*\\)\\r?\\n\\{[\\s\\S]*?\\r?\\n\\}'));
 if(!m)throw Error(name);return m[0];
};
const dest=path.join(__dirname,'ui-tour');fs.mkdirSync(dest,{recursive:true});
const prefix=src.slice(0,src.indexOf('static void log_printf'));
let render=['log_printf','cls','print_header','show_idle_screen','show_password_screen','show_settings_screen','show_hex_input_screen','show_relay_event','show_program_screen'].map(fn).join('\n');
render=render.replace('log_printf("----------------------------\\n");','log_printf("----------------------------\\n");\n    scr_setXY(34,0);scr_printf("UI TOUR %s %02d/09",settings.language?"TR":"EN",tour_page+1);scr_setXY(0,3);');
// The screen output is identical; omit duplicate terminal output, which requires SIF.
render=render.replace('vprintf(ui_translate(format, settings.language), args);','/* No host stdout in a screen-only tour. */');
const running=src.match(/        cls\(\);\r?\n        print_header\(\);\r?\n        log_printf\("RUNNING PROGRAM[\s\S]*?        log_printf\("SELECT: emergency stop\\n"\);/)[0];
fs.writeFileSync(path.join(dest,'tour.c'),prefix+'\nstatic int tour_page;\nstatic int relay_get_status(void){return 0;}\n'+render+`
static void show_running_tour(void) { int i=0,duration=2,action=PROGRAM_ACTION_CROSS; ${running} }
#ifndef TOUR_LANGUAGE
#define TOUR_LANGUAGE 0
#endif
#ifndef TOUR_PAGE
#define TOUR_PAGE 0
#endif
int main(void) {
 int lang,page;
 init_scr();
 scr_setCursor(0);
 for(lang=TOUR_LANGUAGE;lang<=TOUR_LANGUAGE;lang++) for(page=TOUR_PAGE;page<=TOUR_PAGE;page++) {
  tour_page=page;settings.language=lang; settings.left_group=2; settings.right_group=3;
  settings_loaded=0; usb_init_attempted=0;startup_test_state=0;
  switch(page) {
   case 0: show_idle_screen("Waiting for USB startup check");break;
   case 1: password_pos=0;password_failed=0;show_password_screen();break;
   case 2: password_pos=0;password_failed=1;show_password_screen();break;
   case 3: selected_setting=4;show_settings_screen(NULL);break;
   case 4: hex_target=1;hex_pos=4;strcpy(hex_buffer,"178E");show_hex_input_screen();break;
   case 5: hex_target=2;hex_pos=4;strcpy(hex_buffer,"07D9");show_hex_input_screen();break;
   case 6: program_step_count=5;program_steps[0]=0;program_steps[1]=0;program_steps[2]=1;program_steps[3]=2;program_steps[4]=4;show_program_screen("Step recorded");break;
   case 7: program_step_count=5;show_running_tour();break;
   case 8: show_relay_event(4,1,5);break;
  }
  SleepThread();
 }
 return 0;
}
`);
fs.writeFileSync(path.join(dest,'Makefile'),`EE_BIN = UI.ELF
EE_OBJS = tour.o
EE_INCS = -I../..
EE_LIBS = -lpad -ldebug -lc
EE_CFLAGS = -Wno-unused-function -Wno-unused-variable
all: $(EE_BIN)
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
`);
console.log('UI tour source generated separately:',dest);
if(process.argv.includes('--build-all')) {
 const {spawnSync}=require('node:child_process');
 const cc='C:/ps2dev/ee/bin/mips64r5900el-ps2-elf-gcc.exe';
 const env={...process.env,PATH:'C:/msys64/mingw32/bin'+path.delimiter+process.env.PATH};
 for(let lang=0;lang<2;lang++) for(let page=0;page<9;page++) {
  const out=path.join(dest,`TOUR_${lang?'TR':'EN'}_${String(page+1).padStart(2,'0')}.ELF`);
  const result=spawnSync(cc,['-D_EE','-G0','-O2','-Wno-unused-function','-Wno-unused-variable',
   '-IC:/ps2dev/ps2sdk/ee/include','-IC:/ps2dev/ps2sdk/common/include','-I'+root,
   '-DTOUR_LANGUAGE='+lang,'-DTOUR_PAGE='+page,'-TC:/ps2dev/ps2sdk/ee/startup/linkfile',
   '-LC:/ps2dev/ps2sdk/ee/lib','-Wl,-zmax-page-size=128',path.join(dest,'tour.c'),'-ldebug','-lc','-o',out],
   {cwd:root,env,encoding:'utf8'});
  if(result.status!==0)throw Error(result.stderr||String(result.error));
  console.log('Built',path.basename(out));
 }
}
