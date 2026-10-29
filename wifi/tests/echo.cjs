const fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),cc='C:/msys64/mingw64/bin/gcc.exe';
const dir=fs.mkdtempSync(path.join(os.tmpdir(),'rtl-echo-')),exe=path.join(dir,'echo.exe');
const env={...process.env,PATH:path.dirname(cc)+path.delimiter+process.env.PATH};
// Independent packet encoder/checksum reference. Documentation-only 192.0.2/24.
const localMac=Buffer.from('025053320025','hex'),remoteMac=Buffer.from('020102030401','hex');
const localIp=Buffer.from([192,0,2,25]),remoteIp=Buffer.from([192,0,2,1]);
function sum(b){let s=0;for(let i=0;i<b.length;i+=2)s+=b[i]*256+(b[i+1]||0);while(s>65535)s=(s&65535)+(s>>>16);return (~s)&65535;}
function ping(reply){const p=Buffer.alloc(14+20+8+31);(reply?remoteMac:localMac).copy(p);(reply?localMac:remoteMac).copy(p,6);p.writeUInt16BE(0x800,12);p[14]=0x45;p.writeUInt16BE(59,16);p.writeUInt16BE(0x4567,18);p.writeUInt16BE(0x4000,20);p[22]=64;p[23]=1;(reply?localIp:remoteIp).copy(p,26);(reply?remoteIp:localIp).copy(p,30);p[34]=reply?0:8;p.writeUInt16BE(0x1234,38);p.writeUInt16BE(7,40);for(let i=0;i<31;i++)p[42+i]=i+33;p.writeUInt16BE(sum(p.subarray(14,34)),24);p.writeUInt16BE(sum(p.subarray(34)),36);return p;}
function arp(reply){const p=Buffer.alloc(42);(reply?remoteMac:Buffer.alloc(6,255)).copy(p);(reply?localMac:remoteMac).copy(p,6);p.writeUInt16BE(0x806,12);p.writeUInt16BE(1,14);p.writeUInt16BE(0x800,16);p[18]=6;p[19]=4;p.writeUInt16BE(reply?2:1,20);(reply?localMac:remoteMac).copy(p,22);(reply?localIp:remoteIp).copy(p,28);if(reply)remoteMac.copy(p,32);(reply?remoteIp:localIp).copy(p,38);return p;}
const arr=(name,b)=>`static const unsigned char ${name}[${b.length}]={${Array.from(b).join(',')}};\n`;
fs.writeFileSync(path.join(root,'tests/echo_fixtures.h'),'/* Independent synthetic packets, RFC 792 / 826. No real LAN address. */\n'+arr('ping_request',ping(false))+arr('ping_expected',ping(true))+arr('arp_request',arp(false))+arr('arp_expected',arp(true)));
for(const [cmd,args]of[[cc,['-std=c99','-O2','-Wall','-Wextra','-Werror','-Iinclude','-Itests','src/rtl_echo.c','tests/echo.c','-o',exe]],[exe,[]]]){
 const r=spawnSync(cmd,args,{cwd:root,encoding:'utf8',env});process.stdout.write(r.stdout||'');process.stderr.write(r.stderr||'');
 if(r.status!==0)throw Error(String(r.error||r.status));if(cmd===exe)fs.writeFileSync(path.join(root,'reports/echo-packet-host.txt'),r.stdout);
}
