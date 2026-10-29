const fs=require('fs'),path=require('path'),crypto=require('crypto'),{spawnSync}=require('child_process');
const root=path.resolve(__dirname,'..');
const arr=(n,b)=>`static const unsigned char ${n}[${b.length}]={${Array.from(b).join(',')}};\n`;
const pmk=crypto.pbkdf2Sync('password','IEEE',4096,32,'sha1');
const ap=Buffer.from('021122334455','hex'),sta=Buffer.from('0266778899aa','hex');
const an=Buffer.alloc(32,0x31),sn=Buffer.alloc(32,0x72);
const ptk=Buffer.concat([0,1,2,3].map(i=>crypto.createHmac('sha1',pmk).update(Buffer.concat([Buffer.from('Pairwise key expansion\0'),ap,sta,an,sn,Buffer.from([i])])).digest()));
const rsn=Buffer.from('30140100000fac040100000fac040100000fac020000','hex');
function key(info,replay,data=Buffer.alloc(0),keylen=16){
 let p=Buffer.alloc(99+data.length);p[0]=2;p[1]=3;p.writeUInt16BE(p.length-4,2);p[4]=2;p.writeUInt16BE(info,5);p.writeUInt16BE(keylen,7);p[16]=replay;an.copy(p,17);p.writeUInt16BE(data.length,97);data.copy(p,99);
 if(info&256)crypto.createHmac('sha1',ptk.subarray(0,16)).update(p).digest().copy(p,81,0,16);return p;
}
const gtk=Buffer.alloc(16,0xa5),kde=Buffer.concat([Buffer.from('dd16000fac010100','hex'),gtk]);
const clear=Buffer.concat([rsn,kde,Buffer.from([0xdd,0])]);
const wrap=crypto.createCipheriv('id-aes128-wrap',ptk.subarray(16,32),Buffer.alloc(8,0xa6));
const m3=key(0x13ca,2,Buffer.concat([wrap.update(clear),wrap.final()]));
function ccmp(){
 let h=Buffer.alloc(24);h[0]=8;h[1]=0x42;sta.copy(h,4);ap.copy(h,10);ap.copy(h,16);
 let aad=Buffer.concat([Buffer.from([8,0x42]),h.subarray(4,22),Buffer.alloc(2)]);
 let nonce=Buffer.concat([Buffer.alloc(1),ap,Buffer.from([0,0,0,0,0,1])]);
 let plain=Buffer.from('aaaa030000000800deadbeef','hex');
 let c=crypto.createCipheriv('aes-128-ccm',ptk.subarray(32,48),nonce,{authTagLength:8});c.setAAD(aad,{plaintextLength:plain.length});
 return Buffer.concat([h,Buffer.from([1,0,0,32,0,0,0,0]),c.update(plain),c.final(),c.getAuthTag()]);
}
function checksum(b){let sum=0;for(let i=0;i<b.length;i+=2)sum+=(b[i]<<8)|(b[i+1]||0);while(sum>>16)sum=(sum&65535)+(sum>>16);return ~sum&65535;}
function dhcp(type,xid=0x12345678){let b=Buffer.alloc(300);b[0]=2;b[1]=1;b[2]=6;b.writeUInt32BE(xid,4);Buffer.from([192,0,2,25]).copy(b,16);sta.copy(b,28);Buffer.from('63825363','hex').copy(b,236);
 Buffer.from([53,1,type,54,4,192,0,2,1,1,4,255,255,255,0,3,4,192,0,2,1,51,4,0,0,0,60,255]).copy(b,240);
 let u=Buffer.alloc(8);u.writeUInt16BE(67);u.writeUInt16BE(68,2);u.writeUInt16BE(308,4);let ip=Buffer.alloc(20);ip[0]=0x45;ip.writeUInt16BE(328,2);ip[8]=64;ip[9]=17;Buffer.from([192,0,2,1]).copy(ip,12);ip.fill(255,16,20);ip.writeUInt16BE(checksum(ip),10);
 return Buffer.concat([sta,ap,Buffer.from([8,0]),ip,u,b]);}
function encryptedEthernet(eth,pn,group=false){
 let h=Buffer.alloc(24);h[0]=8;h[1]=0x42;eth.copy(h,4,0,6);ap.copy(h,10);eth.copy(h,16,6,12);
 let aad=Buffer.concat([Buffer.from([8,0x42]),h.subarray(4,22),Buffer.alloc(2)]);
 let nonce=Buffer.concat([Buffer.alloc(1),ap,Buffer.from([0,0,0,0,0,pn])]);
 let plain=Buffer.concat([Buffer.from('aaaa03000000','hex'),eth.subarray(12)]);
 let c=crypto.createCipheriv('aes-128-ccm',group?gtk:ptk.subarray(32,48),nonce,{authTagLength:8});c.setAAD(aad,{plaintextLength:plain.length});
 return Buffer.concat([h,Buffer.from([pn,0,0,group?96:32,0,0,0,0]),c.update(plain),c.final(),c.getAuthTag()]);
}
let ping=Buffer.alloc(14+20+12);sta.copy(ping);ap.copy(ping,6);ping[12]=8;let pip=ping.subarray(14);pip[0]=0x45;pip.writeUInt16BE(32,2);pip[8]=64;pip[9]=1;Buffer.from([192,0,2,1,192,0,2,25]).copy(pip,12);pip.writeUInt16BE(checksum(pip.subarray(0,20)),10);let ic=pip.subarray(20);ic[0]=8;ic[5]=42;ic[7]=7;ic.writeUInt16BE(checksum(ic),2);
let beacon=Buffer.alloc(36);beacon[0]=0x80;beacon.fill(255,4,10);ap.copy(beacon,10);ap.copy(beacon,16);beacon[32]=100;beacon[34]=0x11;beacon=Buffer.concat([beacon,Buffer.from([0,4]),Buffer.from('IEEE'),Buffer.from([3,1,6]),rsn]);
let fixtures='/* Independent Node/OpenSSL generated public test vectors. */\n';
const claim=Buffer.alloc(42);claim.fill(255,0,6);ap.copy(claim,6);claim.writeUInt16BE(0x806,12);claim.writeUInt16BE(1,14);claim.writeUInt16BE(0x800,16);claim[18]=6;claim[19]=4;claim.writeUInt16BE(1,20);ap.copy(claim,22);Buffer.from([192,0,2,99]).copy(claim,28);Buffer.from([192,0,2,99]).copy(claim,38);
fixtures+=arr('fx_wire_static_claim',encryptedEthernet(claim,3,true));claim[31]=claim[41]=25;fixtures+=arr('fx_wire_lease_claim',encryptedEthernet(claim,3,true));fixtures+=arr('fx_wire_ping4',encryptedEthernet(ping,4));
const reoffer=dhcp(2,0x72727273),reack=dhcp(5,0x72727273);reoffer[61]=reack[61]=26;fixtures+=arr('fx_wire_reoffer',encryptedEthernet(reoffer,4));fixtures+=arr('fx_wire_reack',encryptedEthernet(reack,5));
const tkip_rsn=Buffer.from(rsn);tkip_rsn[7]=2;
const tkip_gtk=Buffer.from(Array.from({length:32},(_,i)=>i+1));
const tkip_clear=Buffer.concat([tkip_rsn,Buffer.from('dd26000fac010100','hex'),tkip_gtk,Buffer.from([0xdd,0])]);
const twrap=crypto.createCipheriv('id-aes128-wrap',ptk.subarray(16,32),Buffer.alloc(8,0xa6));
const tkip_m3=key(0x13ca,2,Buffer.concat([twrap.update(tkip_clear),twrap.final()]));
const tkip_newgtk=Buffer.from(Array.from({length:32},(_,i)=>0xa0+i));
function groupMessage(replay,material){let c=crypto.createCipheriv('id-aes128-wrap',ptk.subarray(16,32),Buffer.alloc(8,0xa6));let data=Buffer.concat([Buffer.from([221,6+material.length,0,15,172,1,1,0]),material]);return key(0x1382,replay,Buffer.concat([c.update(data),c.final()]),32);}
const tkip_rekey=groupMessage(3,tkip_newgtk),tkip_samekey=groupMessage(4,tkip_newgtk),tkip_badgtk=groupMessage(5,Buffer.alloc(16,0x55));
const tkip_beacon=Buffer.from(beacon);tkip_beacon[tkip_beacon.length-rsn.length+7]=2;
for(const [n,b]of Object.entries({pmk,ap,sta,sn,rsn,m1:key(0x8a,1),m3,ccmp:ccmp(),offer:dhcp(2),ack:dhcp(5),beacon,tkip_rsn,tkip_gtk,tkip_m3,tkip_beacon,tkip_newgtk,tkip_rekey,tkip_samekey,tkip_badgtk,wire_offer:encryptedEthernet(dhcp(2,0x72727272),1),wire_ack:encryptedEthernet(dhcp(5,0x72727272),2),wire_ping:encryptedEthernet(ping,3)}))fixtures+=arr('fx_'+n,b);
fs.writeFileSync(path.join(root,'tests/physical_fixtures.h'),fixtures);
const py='C:/Users/prgrm/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe';
const generated=spawnSync(py,['tests/tkip_fixtures.py'],{cwd:root,encoding:'utf8',input:JSON.stringify({ap:ap.toString('hex'),gtk:tkip_gtk.toString('hex'),offer:dhcp(2,0x72727272).toString('hex'),ack:dhcp(5,0x72727272).toString('hex')})});
if(generated.status!==0)throw Error(generated.stderr); // offline fixture generation only
const cc='C:/msys64/mingw64/bin/gcc.exe',env={...process.env,PATH:path.dirname(cc)+path.delimiter+process.env.PATH};
const libs=['aes','ccm','cipher','cipher_wrap','md','sha1','pkcs5','nist_kw','constant_time','platform','platform_util'];
let args=['-std=c99','-O2','-Wall','-Wextra','-Werror','-Iinclude','-Itests','-Ithird_party/mbedtls/include','-Ithird_party/mbedtls/library','-DMBEDTLS_CONFIG_FILE="rtl_mbedtls_config.h"',...['rtl_core','rtl_radio','rtl_wire','rtl_tkip','rtl_tkip_guard','rtl_supplicant','rtl_dhcp','rtl_wpa_crypto','rtl_nonce','rtl_scan','rtl_echo','rtl_http','rtl_acd','wifi_controls','wifi_files','robot_program','wifi_runtime','robot_control','wifi_app','wifi_known'].map(n=>'src/'+n+'.c'),'tests/physical_protocol.c','tests/physical_flow.c','tests/http.c','tests/robot.c','tests/files.c','tests/network_ui.c','tests/program.c','tests/nonce_setup.c','tests/wifi_known.c',...libs.map(n=>'third_party/mbedtls/library/'+n+'.c'),'-o','reports/physical-protocol-test.exe'];
for(const [cmd,a]of [[cc,args],[path.join(root,'reports/physical-protocol-test.exe'),[]]]){
 let r=spawnSync(cmd,a,{cwd:root,env,encoding:'utf8'});process.stdout.write(r.stdout||'');process.stderr.write(r.stderr||'');if(r.status!==0)throw Error(String(r.error||r.status));
 if(a.length===0)fs.writeFileSync(path.join(root,'reports/physical-protocol-host.txt'),r.stdout);
}

const nativeArgs=[...args.filter((_,i)=>i<args.indexOf('tests/physical_protocol.c')),'tests/native_seed.c',...libs.map(n=>'third_party/mbedtls/library/'+n+'.c'),'-o','reports/native-seed-test.exe'];
for(const [cmd,a]of [[cc,nativeArgs],[path.join(root,'reports/native-seed-test.exe'),[]]]){let r=spawnSync(cmd,a,{cwd:root,env,encoding:'utf8'});process.stdout.write(r.stdout||'');process.stderr.write(r.stderr||'');if(r.status!==0)throw Error(String(r.error||r.status));if(!a.length)fs.writeFileSync(path.join(root,'reports/native-seed-host.txt'),r.stdout);}
for(const [cmd,a]of [[cc,['-std=c99','-O2','-Wall','-Wextra','-Werror','-Iinclude','src/robot_control.c','src/robot_program.c','src/wifi_files.c','tests/robot_usb.c','-o','reports/robot-usb-test.exe']],[path.join(root,'reports/robot-usb-test.exe'),[]]]){const r=spawnSync(cmd,a,{cwd:root,env,encoding:'utf8'});process.stdout.write(r.stdout||'');process.stderr.write(r.stderr||'');if(r.status!==0)throw Error(String(r.error||r.status));if(!a.length)fs.writeFileSync('reports/robot-usb-test.txt',r.stdout);}
for(const [cmd,a]of [[cc,['-std=c99','-O2','-Wall','-Wextra','-Werror','-Itests/render_stub','-Iinclude','-Iee','ee/wifi_render.c','src/wifi_app.c','src/rtl_scan.c','tests/render.c','-o','reports/render-test.exe']],[path.join(root,'reports/render-test.exe'),[]]]){const r=spawnSync(cmd,a,{cwd:root,env,encoding:'utf8'});process.stdout.write(r.stdout||'');process.stderr.write(r.stderr||'');if(r.status!==0)throw Error(String(r.error||r.status));if(!a.length)fs.writeFileSync('reports/render-test.txt',r.stdout);}
