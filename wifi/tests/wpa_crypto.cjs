const fs=require('node:fs'),path=require('node:path'),os=require('node:os'),crypto=require('node:crypto');
const {spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),cc='C:/msys64/mingw64/bin/gcc.exe';
const dir=fs.mkdtempSync(path.join(os.tmpdir(),'rtl-wpa-')),exe=path.join(dir,'wpa.exe');
const env={...process.env,PATH:path.dirname(cc)+path.delimiter+process.env.PATH};
// Independent Node/OpenSSL reference, public deterministic fixture, not live keys.
const pmk=crypto.pbkdf2Sync('password','IEEE',4096,32,'sha1');
const ap=Buffer.from([1,2,3,4,5,6]),sta=Buffer.from([16,17,18,19,20,21]);
const an=Buffer.from(Array.from({length:32},(_,i)=>i)),sn=Buffer.from(Array.from({length:32},(_,i)=>128+i));
const label=Buffer.from('Pairwise key expansion\0');
const ptk=Buffer.concat(Array.from({length:4},(_,i)=>crypto.createHmac('sha1',pmk).update(Buffer.concat([label,ap,sta,an,sn,Buffer.from([i])])).digest())).subarray(0,64);
const eapol=Buffer.alloc(107);eapol[0]=2;eapol[1]=3;eapol.writeUInt16BE(103,2);eapol[4]=2;eapol.writeUInt16BE(0x010a,5);eapol.writeUInt16BE(16,7);eapol[16]=1;sn.copy(eapol,17);eapol.writeUInt16BE(8,97);Buffer.from('0001020304050607','hex').copy(eapol,99);
const mic=crypto.createHmac('sha1',ptk.subarray(0,16)).update(eapol).digest().subarray(0,16);
const arr=(name,b)=>`static const unsigned char ${name}[${b.length}]={${Array.from(b).join(',')}};\n`;
fs.writeFileSync(path.join(root,'tests/wpa_fixtures.h'),'/* Independent Node/OpenSSL synthetic fixture. No user credentials. */\n'+arr('fixture_ptk',ptk)+arr('fixture_eapol',eapol)+arr('fixture_mic',mic));
const libs=['aes','ccm','cipher','cipher_wrap','md','sha1','pkcs5','nist_kw','constant_time','platform','platform_util'];
const flags=['-std=c99','-O2','-Wall','-Wextra','-Werror','-DMBEDTLS_CONFIG_FILE="rtl_mbedtls_config.h"','-Iinclude','-Itests','-Ithird_party/mbedtls/include','-Ithird_party/mbedtls/library'];
for(const [cmd,args]of[[cc,[...flags,'src/rtl_wpa_crypto.c','tests/wpa_crypto.c',...libs.map(n=>'third_party/mbedtls/library/'+n+'.c'),'-o',exe]],[exe,[]]]){
 const r=spawnSync(cmd,args,{cwd:root,encoding:'utf8',env});process.stdout.write(r.stdout||'');process.stderr.write(r.stderr||'');
 if(r.status!==0)throw Error(String(r.error||r.status));if(cmd===exe)fs.writeFileSync(path.join(root,'reports/wpa-crypto-host.txt'),r.stdout);
}
