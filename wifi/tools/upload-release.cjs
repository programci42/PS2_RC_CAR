// Only updates the running Wifi ELF, via the app's staged/verified upload API.
const http=require('node:http'),fs=require('node:fs'),crypto=require('node:crypto'),path=require('node:path');
const {archive}=require('./pc-backup.cjs');
const root=path.resolve(__dirname,'..'),base=new URL(process.argv[2]||'http://192.168.1.43/');
if(base.protocol!=='http:'||base.hostname!=='192.168.1.43')throw Error('Expected the user-provided PS2 address.');
const bytes=fs.readFileSync(path.join(root,'Release/Wifi.elf')),version=fs.readFileSync(path.join(root,'VERSION'),'utf8').trim();
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
let crc=0xffffffff;for(const b of bytes){crc^=b;for(let i=0;i<8;i++)crc=(crc>>>1)^((crc&1)?0xedb88320:0);}crc=(crc^0xffffffff)>>>0;
function requestOnce(route,body){return new Promise((resolve,reject)=>{const url=new URL(route,base),method=body===undefined?'GET':'POST';const req=http.request(url,{method,agent:false,headers:body===undefined?{}:{'X-PS2-Files':'1','Content-Type':'application/octet-stream','Content-Length':body.length}},res=>{const chunks=[];let length=0;res.on('data',b=>{chunks.push(b);length+=b.length;if(length>4000000)req.destroy(new Error('Response too large'));});res.on('error',reject);res.on('end',()=>{const data=Buffer.concat(chunks);if(res.statusCode>=400)reject(Error(`HTTP ${res.statusCode}: ${data.toString('utf8').slice(0,200)}`));else resolve(data);});});const connecting=setTimeout(()=>req.destroy(new Error('connect timeout')),5000);req.on('socket',s=>{if(!s.connecting)clearTimeout(connecting);else s.once('connect',()=>clearTimeout(connecting))});req.on('close',()=>clearTimeout(connecting));req.setTimeout(15000,()=>req.destroy(new Error('HTTP inactivity timeout')));req.on('error',reject);req.end(body);});}
async function request(route,body){for(let i=0;;i++){try{return await requestOnce(route,body)}catch(e){if(body!==undefined||i>=3)throw e;await new Promise(r=>setTimeout(r,200))}}}
const post=(url,b=Buffer.alloc(0))=>request(url,b);
async function sizeOf(target){return new Promise((ok,no)=>{const q=http.request(new URL('/?download='+encodeURIComponent(target),base),{method:'HEAD',agent:false},r=>{r.resume();r.on('end',()=>r.statusCode===200?ok(Number(r.headers['content-length'])):no(Error('HEAD '+r.statusCode)))});q.setTimeout(15000,()=>q.destroy(Error('metadata timeout')));q.on('error',no);q.end()})}
const legacyAllowed=process.argv.includes('--replace-legacy');
const backupPaths=html=>[...new Set([...html.toString().matchAll(/href="\/\?download=([^"]+)"/g)].map(m=>decodeURIComponent(m[1])).filter(p=>/^mc0:\/Wifi\/WIFI\.ELF\.bak[0-9]$/i.test(p)))];
(async()=>{const info=JSON.parse(await request('/files/info')),target=decodeURIComponent(info.running);if(target.toLowerCase()!=='mc0:/wifi/wifi.elf')throw Error('Unexpected running path: '+target);if(info.upload)throw Error('Another upload is active; left untouched.');const status=JSON.parse(await request('/status'));if(status.mask||status.owner)throw Error('Robot must be stopped before updating.');
 const legacy=!info.pcBackupRequired;
 if(legacy)throw Error('Legacy server: use --replace-legacy only for a verified PC archive and immediate removal of the renamed old ELF.');
 const previousPaths=legacy?backupPaths(await request('/?ls='+encodeURIComponent('mc0:/Wifi'))):[];
 let old;
 if(process.argv.includes('--reuse-installed-backup')){
  const previous=JSON.parse(fs.readFileSync(path.join(root,'reports/remote-update.json'),'utf8')),cached=path.join(root,'Backups/PS2-Wifi',previous.sha256+'.elf');
  if(!previous.physicalMc0Updated||!previous.rangeReadbackVerified&&!previous.remoteReadBackSha256&&!previous.sizeOnlyVerification)throw Error('Previous activation/readback evidence required for cached backup');
  old=fs.readFileSync(cached);if(sha(old)!==previous.sha256||old.length!==previous.bytes)throw Error('Cached installed ELF archive mismatch');
  const display=JSON.parse(await request('/display'));if(!/^0\.4\.[0-9]+$/.test(display.version))throw Error('Unknown running server');
  if(await sizeOf(target)!==old.length)throw Error('Installed ELF size differs from PC archive');console.log('Installed ELF version/size matches PC archive; server checks old CRC before replacement.');
 }else throw Error('Use --reuse-installed-backup; installed ELF content must not be downloaded.');
 const backupPath=archive(root,old);
 let oldCrc=0xffffffff;for(const b of old){oldCrc^=b;for(let i=0;i<8;i++)oldCrc=(oldCrc>>>1)^((oldCrc&1)?0xedb88320:0);}oldCrc=(oldCrc^0xffffffff)>>>0;
 console.log('Old ELF archived and read-back verified on PC:',backupPath);
 console.log(`Uploading ${version}, ${bytes.length} bytes to ${target}`);
 await post('/files/upload/begin?path='+encodeURIComponent(target)+'&size='+bytes.length);
 for(let offset=0;offset<bytes.length;offset+=2048){const data=bytes.subarray(offset,offset+2048);let done=false;for(let attempt=0;attempt<3;attempt++){try{await post('/files/upload/chunk?offset='+offset,data);done=true;break;}catch(e){if(attempt===2)throw e;}}if(!done)throw Error('Chunk failed');if(offset%65536===0)console.log(`Sent ${offset+data.length}/${bytes.length}`);}
 const result=(await post('/files/upload/commit?crc32='+crc.toString(16).padStart(8,'0')+'&pcbackup='+oldCrc.toString(16).padStart(8,'0'))).toString('utf8');console.log(result);
 const remoteBytes=await sizeOf(target);if(remoteBytes!==bytes.length)throw Error('Installed byte count differs: '+remoteBytes+' vs '+bytes.length);
 archive(root,bytes);const report={utc:new Date().toISOString(),version,target,bytes:bytes.length,sha256:sha(bytes),remoteReadBackSha256:null,sizeOnlyVerification:true,remoteByteCount:remoteBytes,physicalMc0Updated:true,activated:false,backupLocation:backupPath,noRetainedCardBackup:!legacy,verification:'User requested metadata-only byte count comparison; installed file content not downloaded'};
 fs.writeFileSync(path.join(root,'reports/remote-update.json'),JSON.stringify(report,null,2)+'\n');console.log('PASS installed byte count:',remoteBytes,'No installed-file download performed.');
})().catch(e=>{console.error('UPDATE FAILED:',e.message);process.exitCode=1});
