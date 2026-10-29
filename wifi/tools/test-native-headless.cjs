// Real EE startup/PADMAN/driver loop, no Wi-Fi device and no memory card writes.
const fs=require('fs'),path=require('path'),cp=require('child_process');
const root=path.resolve(__dirname,'..'),cc='C:/ps2dev/ee/bin/mips64r5900el-ps2-elf-gcc.exe';
const env={...process.env,PATH:['C:/msys64/usr/bin','C:/msys64/mingw32/bin','C:/ps2dev/bin','C:/ps2dev/ee/bin',process.env.PATH].join(path.delimiter),PLAY_RTL8192CU:'0',PLAY_WIFI_PAD_TEST:'1',PLAY_WIFI_PAD_BAD_PASSWORD:'0'};
function run(exe,args,timeout=60000){let r=cp.spawnSync(exe,args,{cwd:root,env,encoding:'utf8',timeout,maxBuffer:2000000});if(r.stdout)process.stdout.write(r.stdout);if(r.stderr)process.stderr.write(r.stderr);if(r.status!==0)throw Error(r.error?.message||'exit '+r.status);}
run(cc,['-D_EE','-G0','-O2','-Wall','-IC:/ps2dev/ps2sdk/ee/include','-IC:/ps2dev/ps2sdk/common/include','-I.','-Iinclude','-DRTL_WIFI_NATIVE_PAD_TEST','-c','ee/wifi.c','-o','reports/native-headless-pad.o']);
const link=fs.readFileSync(path.join(root,'tests/native-link-command.txt'),'utf8').split(/\r?\n/).find(l=>l.startsWith('mips64r5900el-ps2-elf-gcc ')&&l.includes(' -o rtl8192cu-wifi.elf '));if(!link)throw Error('Verified production link command missing');
let args=link.split(/\s+/).slice(1).map(a=>a==='rtl8192cu-wifi.elf'?'reports/native-headless-pad.elf':a==='ee/wifi.o'?'reports/native-headless-pad.o':a);if(!args.includes('src/ps2_compile.o'))args.push('src/ps2_compile.o');run(cc,args);
run(process.env.PS2_TEST_EMULATOR||path.join(root,'emulator-bin/play-rtl8192cu-nettest.exe'),[path.join(root,'reports/native-headless-pad.elf'),path.join(root,'reports/native-headless-pad.log')]);
const log=fs.readFileSync(path.join(root,'reports/native-headless-pad.log'),'utf8');if(!log.includes('NATIVE_HEADLESS_PAD_TEST PASS'))throw Error('Native pad assertion not reached');console.log(log.trim());
