const fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..');
const dir=fs.mkdtempSync(path.join(os.tmpdir(),'rtl8192cu-test-'));
const cc=process.env.CC||(process.platform==='win32'?'C:/msys64/mingw64/bin/gcc.exe':'cc');
const exe=path.join(dir,process.platform==='win32'?'test.exe':'test');
const env={...process.env,PATH:path.dirname(cc)+path.delimiter+process.env.PATH};
for(const [command,args] of [[cc,['-std=c99','-Wall','-Wextra','-Werror','-DRTL_HOST_TEST','-I'+path.join(root,'include'),path.join(root,'src/rtl_core.c'),path.join(root,'tests/selftest.c'),'-o',exe]],[exe,[]]]){
 const result=spawnSync(command,args,{encoding:'utf8',cwd:dir,env});
 process.stdout.write(result.stdout||'');process.stderr.write(result.stderr||'');
 if(result.status!==0)throw Error('Test failed: '+command+' '+String(result.error||result.status));
 if(command===exe){fs.mkdirSync(path.join(root,'reports'),{recursive:true});fs.writeFileSync(path.join(root,'reports/host-test.txt'),result.stdout);}
}
const lifecycle=path.join(dir,process.platform==='win32'?'lifecycle.exe':'lifecycle');
for(const [command,args]of[[cc,['-std=c99','-Wall','-Wextra','-Werror','-I'+path.join(root,'include'),'-I'+path.join(root,'tests'),path.join(root,'tests/iop_lifecycle.c'),path.join(root,'src/rtl_core.c'),'-o',lifecycle]],[lifecycle,[]]]){
 const result=spawnSync(command,args,{encoding:'utf8',cwd:dir,env});
 process.stdout.write(result.stdout||'');process.stderr.write(result.stderr||'');
 if(result.status!==0)throw Error('IOP lifecycle test failed: '+String(result.error||result.status));
 if(command===lifecycle)fs.writeFileSync(path.join(root,'reports/iop-lifecycle.txt'),result.stdout);
}
