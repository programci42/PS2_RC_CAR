const fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),cc='C:/msys64/mingw64/bin/gcc.exe';
const dir=fs.mkdtempSync(path.join(os.tmpdir(),'rtl-bringup-')),exe=path.join(dir,'bringup.exe');
const env={...process.env,PATH:path.dirname(cc)+path.delimiter+process.env.PATH};
for(const [cmd,args] of [[cc,['-std=c99','-Wall','-Wextra','-Werror','-Iinclude','src/rtl_core.c','src/rtl_bringup.c','tests/bringup.c','-o',exe]],[exe,['third_party/realtek-firmware/rtl8192cfw.bin']]]){
 const r=spawnSync(cmd,args,{cwd:root,encoding:'utf8',env});
 process.stdout.write(r.stdout||'');process.stderr.write(r.stderr||'');
 if(r.status!==0)throw Error(String(r.error||r.status));
 if(cmd===exe)fs.writeFileSync(path.join(root,'reports/bringup-host.txt'),r.stdout);
}
const lifecycle=path.join(dir,'boot-lifecycle.exe');
for(const [cmd,args]of[[cc,['-std=c99','-Wall','-Wextra','-Werror','-Iinclude','-Itests','src/rtl_core.c','src/rtl_bringup.c','tests/iop_boot_lifecycle.c','-o',lifecycle]],[lifecycle,[]]]){
 const r=spawnSync(cmd,args,{cwd:root,encoding:'utf8',env});process.stdout.write(r.stdout||'');process.stderr.write(r.stderr||'');
 if(r.status!==0)throw Error(String(r.error||r.status));if(cmd===lifecycle)fs.writeFileSync(path.join(root,'reports/iop-boot-lifecycle.txt'),r.stdout);
}
