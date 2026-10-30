// Documentation / UI test server only. Never connects to a PS2 or USB hardware.
const fs=require('fs'),path=require('path'),http=require('http'),assert=require('assert');
const root=path.resolve(__dirname,'..'),version=fs.readFileSync(path.join(root,'VERSION'),'utf8').trim();
const controls=fs.readFileSync(path.join(root,'web/controls.html'),'utf8');
let language='tr';const moves=[];
const c=fs.readFileSync(path.join(root,'src/rtl_http.c'),'utf8');
const shell=JSON.parse(c.match(/"<!doctype html><html lang=.*?<\/html>"/)[0]);
const values=['PS2 Slim Wi-Fi',192,0,2,25,version,0,controls];let next=0;
const preview='<aside style="padding:12px;background:#583b0a;color:white;font:14px system-ui">UI PREVIEW — simulated network and relay data; no hardware connection</aside>';
const page=shell.replace(/%lu|%u|%s/g,()=>String(values[next++])).replace('<body>','<body>'+preview);
const config={version:1,left:2,right:1,bindings:[{button:'CROSS',actions:['on5','on3']},{button:'TRIANGLE',actions:['on6','on4']}]};
const telemetry={displayEnabled:0,padConnected:0,padButtons:0,batterySet:1,batteryEstimated:85,powerMw:20000,motor100msUwh:666,signalValid:0,rxKbps:0,txKbps:0,lastRequest:'UI preview',ramBytes:33554432,heapBytes:262144,logBytes:0,logQueued:0,logDropped:0,clockValid:0};
const server=http.createServer((req,res)=>{const u=new URL(req.url,'http://localhost');let body,type='application/json';
 if(u.pathname==='/'){body=page;type='text/html';}
 else if(u.pathname==='/i18n.js'){let header=fs.readFileSync(path.join(root,'include/wifi_i18n_asset.h'),'utf8');body='window.ps2Language='+JSON.stringify(language)+';'+JSON.parse(header.match(/#define WIFI_I18N_ASSET (.*)/)[1]);type='application/javascript';}
 else if(u.pathname==='/language'){if(req.method==='POST'){req.setEncoding('utf8');req.on('data',b=>{if(b==='tr'||b==='en')language=b})}body=JSON.stringify({language});}
 else if(u.pathname==='/tools'){body=fs.readFileSync(path.join(root,'web/tools.html'),'utf8').replace('<h1>',preview+'<h1>');type='text/html';}
 else if(u.pathname==='/status')body=JSON.stringify({ready:1,owner:0,mask:0,left:2,right:1});
 else if(u.pathname==='/settings')body=JSON.stringify(config);
 else if(u.pathname==='/telemetry')body=JSON.stringify(telemetry);
 else if(u.pathname==='/battery')body=JSON.stringify({percent:85,capacityMah:3300,motors:4,baseMw:18000,motorMa:2000,efficiency:90});
 else if(u.pathname==='/files/info')body=JSON.stringify({running:'mc0:/Wifi/WIFI.ELF',pcBackupRequired:true,upload:0,download:0});
 else if(u.pathname==='/exec/status')body=JSON.stringify({state:0,output:'UI preview. No code executed.',compilerLocation:'PS2',sourceLanguage:0});
 else if(u.pathname.startsWith('/devices'))body=JSON.stringify({scanning:0,count:0,devices:[],note:'UI preview. No hardware scan performed.'});
 else if(u.pathname==='/clock')body='{}';
 else if(u.pathname.startsWith('/drive/')){moves.push(u.pathname.slice(7));body='UI preview; no movement command sent.';type='text/plain';}
 else {body='UI preview; operation not executed.';type='text/plain';}
 res.writeHead(200,{'Content-Type':type+'; charset=utf-8','Cache-Control':'no-store'});res.end(body);
});
server.listen(0,'127.0.0.1',async()=>{const url='http://127.0.0.1:'+server.address().port;console.log('UI preview:',url);
 if(!process.argv.includes('--capture')&&!process.argv.includes('--test'))return;
 const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');let browser;
 try{browser=await chromium.launch({channel:'chrome',headless:true});const p=await browser.newPage({viewport:{width:1280,height:900},deviceScaleFactor:1});const errors=[];p.on('pageerror',e=>errors.push(e.message));
  const out=process.env.SCREENSHOT_DIR||path.join(root,'reports/web-screenshots');fs.mkdirSync(out,{recursive:true});
  await p.goto(url);await p.waitForTimeout(200);assert.equal(await p.locator('[data-tab=settings]').textContent(),'AYARLAR');
  await p.waitForFunction(()=>!document.querySelector('[data-m=forward]').disabled);let forward=p.locator('[data-m=forward]'),box=await forward.boundingBox();await p.mouse.move(box.x+box.width/2,box.y+box.height/2);await p.mouse.down();await p.waitForTimeout(1000);assert(moves.filter(m=>m==='forward').length>=3);await p.mouse.up();await p.waitForTimeout(400);assert.equal(moves.at(-1),'stop');let count=moves.length;await p.waitForTimeout(400);assert.equal(moves.length,count);
  await p.locator('[data-tab=settings]').click();await p.waitForFunction(()=>document.querySelector('#cfg-left').value==='2');await p.locator('#cfg-left').selectOption('3');await p.locator('#cfg-button').selectOption('CROSS');assert((await p.locator('#cfg-steps').textContent()).includes('7'));
  await p.locator('#ui-language').selectOption('en');await p.locator('#language-save').click();await p.waitForFunction(()=>document.documentElement.lang==='en');assert.equal(await p.locator('[data-tab=settings]').textContent(),'SETTINGS');await p.reload();assert.equal(await p.locator('[data-tab=settings]').textContent(),'SETTINGS');
  for(const tab of ['robot','files','commands','settings']){await p.locator('[data-tab='+tab+']').click();await p.waitForTimeout(250);if(process.argv.includes('--capture'))await p.screenshot({path:path.join(out,'web-'+tab+'-preview.png'),fullPage:true});}
  await p.goto(url+'/tools');await p.waitForTimeout(250);assert.equal(await p.locator('h1').textContent(),'PS2 devices and background code');if(process.argv.includes('--capture'))await p.screenshot({path:path.join(out,'web-tools-preview.png'),fullPage:true});
  assert.deepEqual(errors,[]);console.log('WEB_PREVIEW_TEST PASS Turkish-default English-switch-persistence group-remap hold-heartbeats release-stop 5 pages; page errors=0; fixture only');
 }catch(e){console.error(e);process.exitCode=1;}finally{if(browser)await browser.close();server.close();}
});
