const fs=require('fs');
for(const [file,header,macro,limit] of [['controls','robot_web_asset.h','ROBOT_WEB_ASSET',27500],['tools','ps2_tools_asset.h','PS2_TOOLS_ASSET',8000]]){
 const html=fs.readFileSync('web/'+file+'.html','utf8');
 for(const m of html.matchAll(/<script>([\s\S]*?)<\/script>/g))new Function(m[1]);
 if(Buffer.byteLength(html)>limit)throw Error(file+' exceeds embedded page budget');
 fs.writeFileSync('include/'+header,'/* Generated from web/'+file+'.html. */\n#define '+macro+' '+JSON.stringify(html)+'\n');
 console.log(file,Buffer.byteLength(html),'bytes');
}
const relayJs=fs.readFileSync('web/relay-test.js','utf8');new Function(relayJs);fs.writeFileSync('include/relay_test_asset.h','/* Generated from web/relay-test.js. */\n#define RELAY_TEST_ASSET '+JSON.stringify(relayJs)+'\n');
const all=JSON.parse(fs.readFileSync('web/translations.json','utf8'));
const pages=relayJs+fs.readFileSync('web/controls.html','utf8')+fs.readFileSync('web/tools.html','utf8')+fs.readFileSync('src/rtl_http.c','utf8');
const dictionary=Object.fromEntries(Object.entries(all).filter(([en])=>en.length>1&&(pages.includes(en)||['Language saved.','Ready','Uploading','Completed','Timed out','Cancelled','Error','Compile error','Compiling / running','Relay ',' On',' Off','Stopped','Gamepad','Web','Left: R','Right: R',' · Control: '].includes(en))));
const js='const PS2_TRANSLATIONS='+JSON.stringify(dictionary)+';\n'+fs.readFileSync('web/i18n-runtime.js','utf8');new Function(js);
if(Buffer.byteLength(js)>27000)throw Error('i18n asset exceeds response budget');
fs.writeFileSync('include/wifi_i18n_asset.h','/* Generated web language dictionary. */\n#define WIFI_I18N_ASSET '+JSON.stringify(js)+'\n');
console.log('i18n',Buffer.byteLength(js),'bytes',Object.keys(dictionary).length,'entries');
const cfiles=['ee/wifi.c','ee/wifi_physical.h','ee/robot_usb.h','ee/wifi_render.c','src/wifi_app.c','src/wifi_runtime.c','src/rtl_http.c'];
const phrases=new Set([...Object.keys(all),...cfiles.flatMap(f=>[...fs.readFileSync(f,'utf8').matchAll(/WIFI_TR\(("(?:[^"\\]|\\.)*")\)/g)].map(m=>JSON.parse(m[1])))]);
const missing=[...phrases].filter(s=>!all[s]);if(missing.length)throw Error('Missing C translations: '+missing.join(', '));
fs.writeFileSync('include/wifi_i18n.h','#ifndef WIFI_I18N_H\n#define WIFI_I18N_H\n#include <string.h>\nextern unsigned wifi_language; /* 0 Turkish, 1 English. */\nstatic inline const char*wifi_tr(const char*s){static const struct{const char*en,*tr;} t[]={\n'+[...phrases].map(en=>'{'+JSON.stringify(en)+','+JSON.stringify(all[en])+'}').join(',\n')+'\n};unsigned i;if(!wifi_language)for(i=0;i<sizeof(t)/sizeof(t[0]);i++)if(!strcmp(s,t[i].en))return t[i].tr;return s;}\n#define WIFI_TR(s) wifi_tr(s)\n#endif\n');
