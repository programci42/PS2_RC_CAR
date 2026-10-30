(()=>{'use strict';
document.querySelector('#relay-test-buttons').innerHTML=Array.from({length:8},(_,i)=>'<button data-relay="'+(i+1)+'">R'+(i+1)+'</button>').join('');
const pane=document.querySelector('#relaytest-pane'),info=document.querySelector('#relay-test-result'),buttons=[...pane.querySelectorAll('[data-relay]')];
let wanted=0,actual=0,busy=false,ready=false,uncertain=true;
window.ps2Sequence=window.ps2Sequence||Date.now()%1000000000;
async function command(mask){const r=await fetch('/drive/'+(mask?'test'+mask:'stop')+'?s='+(++window.ps2Sequence),{method:'POST',headers:{'X-PS2-Control':'1'},signal:AbortSignal.timeout(1500)});const t=await r.text();if(!r.ok)throw Error(t);}
function render(){for(const b of buttons){const bit=1<<(+b.dataset.relay-1),on=!!(actual&bit);b.setAttribute('aria-pressed',uncertain?'mixed':String(on));b.style.background=uncertain?'#70551d':on?'#187b46':'#223451';b.disabled=busy||(!on&&(!ready||uncertain||!!(wanted&(((+b.dataset.relay-1)&1)?bit/2:bit*2))));}}
async function status(){const r=await fetch('/status',{cache:'no-store',signal:AbortSignal.timeout(1500)});if(!r.ok)throw Error('Cannot read relay status.');const s=await r.json();ready=!!s.ready&&!s.resetPending;uncertain=!!s.outputUncertain;actual=s.mask;if(!ready||uncertain||s.owner===1)wanted=0;info.textContent=uncertain?'Outputs uncertain. Disconnect motor power.':ready?'USB command acknowledged; contacts are not measured.':'Relay board not ready.';render();}
async function tick(){if(busy||pane.hidden||document.hidden||window.ps2Inactive||window.ps2Transfer)return;busy=true;render();try{if(wanted)await command(wanted);await status();}catch(e){wanted=0;ready=false;uncertain=true;info.textContent='Connection lost; relay OFF is not confirmed.';}finally{busy=false;render();}}
for(const b of buttons)b.onclick=async()=>{if(busy)return;const next=wanted^(1<<(+b.dataset.relay-1));busy=true;render();try{await command(next);wanted=next;await status();}catch(e){wanted=0;uncertain=true;info.textContent=e.message;}finally{busy=false;render();}};
async function stop(force=false){const active=wanted||actual;wanted=0;if(!active&&force!==true)return;try{await command(0);await status();}catch(e){uncertain=true;info.textContent='Connection lost; relay OFF is not confirmed.';}render();}
document.querySelector('#relay-test-stop').onclick=()=>stop(true);
document.addEventListener('ps2-tab-change',stop);window.addEventListener('blur',stop);document.addEventListener('visibilitychange',()=>{if(document.hidden)stop();});
setInterval(tick,300);render();
})();
