const fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),cc='C:/msys64/mingw64/bin/gcc.exe';
const dir=fs.mkdtempSync(path.join(os.tmpdir(),'rtl-wifi-app-')),exe=path.join(dir,'app.exe');
const env={...process.env,PATH:path.dirname(cc)+path.delimiter+process.env.PATH};
for(const [cmd,args]of[[cc,['-std=c99','-O2','-Wall','-Wextra','-Werror','-Iinclude','src/wifi_app.c','src/rtl_scan.c','tests/wifi_app.c','-o',exe]],[exe,[]]]){
 const r=spawnSync(cmd,args,{cwd:root,encoding:'utf8',env});process.stdout.write(r.stdout||'');process.stderr.write(r.stderr||'');
 if(r.status!==0)throw Error(String(r.error||r.status));if(cmd===exe)fs.writeFileSync(path.join(root,'reports/wifi-app-host.txt'),r.stdout);
}
const store=path.join(dir,'store.exe');
for(const [cmd,args]of[[cc,['-std=c99','-O2','-Wall','-Wextra','-Werror','-Iinclude','src/wifi_app.c','src/rtl_scan.c','src/wifi_store.c','tests/wifi_store.c','-o',store]],[store,[dir]]]){
 const r=spawnSync(cmd,args,{cwd:root,encoding:'utf8',env});process.stdout.write(r.stdout||'');process.stderr.write(r.stderr||'');
 if(r.status!==0)throw Error(String(r.error||r.status));if(cmd===store)fs.writeFileSync(path.join(root,'reports/wifi-store-host.txt'),r.stdout);
}
