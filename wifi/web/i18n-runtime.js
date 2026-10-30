(()=>{'use strict';let language=window.ps2Language||'tr';const originals=new WeakMap();
const keys=Object.keys(PS2_TRANSLATIONS).filter(k=>k&&!k.includes('%')).sort((a,b)=>b.length-a.length);
const escape=s=>s.replace(/[.*+?^${}()|[\]\\]/g,'\\$&');
const matcher=new RegExp('(?<![A-Za-z])(?:'+keys.map(escape).join('|')+')(?![A-Za-z])','g');
function tr(s){return language==='en'?s:s.replace(matcher,k=>PS2_TRANSLATIONS[k])}
function text(n){if(!n.parentElement||n.parentElement.closest('script,style,textarea,pre,code,#ui-language'))return;let value=n.nodeValue,old=originals.get(n);if(!old||value!==old.last)old={en:value,last:value};let next=tr(old.en);if(value!==next)n.nodeValue=next;old.last=next;originals.set(n,old)}
function walk(root){if(root.nodeType===3){text(root);return}if(root.nodeType!==1&&root.nodeType!==9)return;const w=document.createTreeWalker(root,NodeFilter.SHOW_TEXT);while(w.nextNode())text(w.currentNode)}
function apply(){document.documentElement.lang=language;walk(document);let select=document.querySelector('#ui-language');if(select)select.value=language}
document.addEventListener('DOMContentLoaded',()=>{apply();new MutationObserver(records=>{for(const r of records){if(r.type==='characterData')text(r.target);else for(const n of r.addedNodes)walk(n)}}).observe(document.body,{subtree:true,childList:true,characterData:true});
 let save=document.querySelector('#language-save');if(save)save.onclick=async()=>{let result=document.querySelector('#language-result');save.disabled=true;try{let selected=document.querySelector('#ui-language').value,r=await fetch('/language',{method:'POST',headers:{'X-PS2-Control':'1','Content-Type':'text/plain'},body:selected});if(!r.ok)throw Error(await r.text()||String(r.status));language=selected;apply();result.textContent=tr('Language saved.')}catch(e){result.textContent=e.message}finally{save.disabled=false}};
});window.ps2Translate=tr;
})();
