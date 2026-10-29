// Restart only a verified installed release; require a new session on same-version tests.
const http=require('http'),fs=require('fs'),crypto=require('crypto');
const version=fs.readFileSync('VERSION','utf8').trim();
const report=JSON.parse(fs.readFileSync('reports/remote-update.json','utf8'));
const sha=crypto.createHash('sha256').update(fs.readFileSync('Release/Wifi.elf')).digest('hex');
let crc=0xffffffff;for(const b of fs.readFileSync('Release/Wifi.elf')){crc^=b;for(let i=0;i<8;i++)crc=(crc>>>1)^((crc&1)?0xedb88320:0);}crc=((crc^0xffffffff)>>>0).toString(16).padStart(8,'0');
const verified=(report.sizeOnlyVerification&&report.remoteByteCount===fs.statSync('Release/Wifi.elf').size)||report.remoteReadBackSha256===sha||(report.serverVerifiedCrc32===crc&&report.serverElfValidationPassed&&report.rangeReadbackVerified&&report.noRetainedCardBackup);
if(report.version!==version||!report.physicalMc0Updated||report.sha256!==sha||!verified)throw Error('Verified current physical release required');
function req(path,method='GET'){return new Promise((ok,no)=>{let q=http.request({host:'192.168.1.43',path,method,agent:false},r=>{let b=[],n=0;r.on('data',x=>{n+=x.length;if(n>60000)q.destroy(Error('large response'));else b.push(x)});r.on('error',no);r.on('end',()=>ok({status:r.statusCode,text:Buffer.concat(b).toString()}))});q.setTimeout(4000,()=>q.destroy(Error('timeout')));const connecting=setTimeout(()=>q.destroy(Error('connect timeout')),4000);q.on('socket',s=>{if(!s.connecting)clearTimeout(connecting);else s.once('connect',()=>clearTimeout(connecting))});q.on('close',()=>clearTimeout(connecting));q.on('error',no);q.end()})}
(async()=>{
 const s=await req('/status');if(s.status!==200)throw Error('Cannot verify robot idle');const state=JSON.parse(s.text);if(state.mask||state.owner)throw Error('Robot must be idle');
 const beforeResponse=await req('/display');if(beforeResponse.status!==200)throw Error('Cannot read pre-restart counter');const before=JSON.parse(beforeResponse.text);
 const reload=await req('/?shutdown=-r');if(reload.status!==200||JSON.parse(reload.text).systemAction!=='restart')throw Error('Restart refused: '+reload.status);console.log('PS2 restart accepted; waiting for',version);
 const start=Date.now();let last='';while(Date.now()-start<360000){
  try{let r=await req('/display');if(r.status===423){await req('/','HEAD');continue;}
   if(r.status===200){let d=JSON.parse(r.text);if(d.version===version&&(before.version!==version||d.padTicks<before.padTicks)){report.lastRestart={beforeVersion:before.version,beforePadTicks:before.padTicks,afterVersion:d.version,afterPadTicks:d.padTicks,counterReset:d.padTicks<before.padTicks,elapsedMs:Date.now()-start};report.restartHistory=[...(report.restartHistory||[]),{utc:new Date().toISOString(),...report.lastRestart}].slice(-8);report.activated=true;report.activationVerifiedUtc=new Date().toISOString();fs.writeFileSync('reports/remote-update.json',JSON.stringify(report,null,2)+'\n');fs.writeFileSync('reports/physical-display-'+version.replaceAll('.','')+'.json',JSON.stringify(d,null,2)+'\n');console.log('PASS activated physical release:',JSON.stringify(d));return;}}
   if(last!==String(r.status)){console.log('Waiting, HTTP',r.status);last=String(r.status);}
  }catch(e){if(last!==(e.code||e.message)){console.log('Waiting:',e.code||e.message);last=e.code||e.message;}}
  await new Promise(r=>setTimeout(r,750));
 }throw Error('Copy verified; activation not confirmed within 360 seconds');
})().catch(e=>{console.error(e.message);process.exitCode=1});
