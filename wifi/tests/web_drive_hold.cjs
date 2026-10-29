// Run the actual page handlers: held pointer, heartbeat, takeover and TV controls.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const scripts=[...fs.readFileSync('web/controls.html','utf8').matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m=>m[1]);
const nodes={},intervals=new Map(),events={},moves=[];let id=0,rejectMove=false;
const element=()=>({style:{},hidden:false,classList:{add(){},remove(){}},setPointerCapture(){}});
const buttons=['forward','back'].map(m=>Object.assign(element(),{dataset:{m}}));
const $=s=>nodes[s]||(nodes[s]=element());
const context={AbortController,window:{addEventListener:(e,f)=>events[e]=f},document:{hidden:false,querySelector:$,querySelectorAll:()=>buttons,addEventListener:(e,f)=>events[e]=f},setInterval:(f,ms)=>{intervals.set(++id,{f,ms});return id;},clearInterval:i=>intervals.delete(i),setTimeout:()=>1,clearTimeout(){},fetch:async(url)=>{moves.push(url);return url==='/status'?{json:async()=>({ready:1,left:2,right:1,owner:0})}:{ok:!rejectMove,status:rejectMove?409:200,text:async()=>rejectMove?'Gamepad priority':'OK'};}};
const flush=()=>new Promise(r=>setImmediate(r)),event={preventDefault(){},pointerId:1};
(async()=>{
 vm.runInNewContext(scripts.find(s=>s.includes('async function send(move)')),context);await flush();assert(!buttons[0].disabled);moves.length=0;
 buttons[0].onpointerdown(event);await flush();for(let i=0;i<5;i++){[...intervals.values()].find(t=>t.ms===300).f();await flush();}
 assert.strictEqual(moves.length,6);assert(moves.every(m=>m.startsWith('/drive/forward?')));
 buttons[1].onpointerdown(event);await flush();buttons[0].onpointerup(event);buttons[0].onlostpointercapture();await flush();assert(!moves.some(m=>m.startsWith('/drive/stop?')));
 [...intervals.values()].find(t=>t.ms===300).f();await flush();assert(moves.at(-1).startsWith('/drive/back?'));
 buttons[1].onpointerup(event);await flush();assert(moves.at(-1).startsWith('/drive/stop?'));let count=moves.length;events.blur();await flush();assert.strictEqual(moves.length,count);
 rejectMove=true;buttons[0].onpointerdown(event);await flush();assert(moves.at(-1).startsWith('/drive/forward?'));assert(![...intervals.values()].some(t=>t.ms===300));
 rejectMove=false;vm.runInNewContext(scripts.find(s=>s.includes('function tvDisplay(')),context);await $('#tv-on').onclick();await $('#tv-off').onclick();assert.deepStrictEqual(moves.slice(-2),['/?display=1','/?display=0']);
 console.log('WEB_DRIVE_HOLD_TEST PASS actual JS: sustained heartbeats, pointer-switch/release, idle blur, gamepad takeover, settings TV on/off');
})().catch(e=>{console.error(e);process.exitCode=1});
