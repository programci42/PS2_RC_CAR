const fs=require('fs');
for(const [file,header,macro,limit] of [['controls','robot_web_asset.h','ROBOT_WEB_ASSET',27500],['tools','ps2_tools_asset.h','PS2_TOOLS_ASSET',8000]]){
 const html=fs.readFileSync('web/'+file+'.html','utf8');
 for(const m of html.matchAll(/<script>([\s\S]*?)<\/script>/g))new Function(m[1]);
 if(Buffer.byteLength(html)>limit)throw Error(file+' exceeds embedded page budget');
 fs.writeFileSync('include/'+header,'/* Generated from web/'+file+'.html. */\n#define '+macro+' '+JSON.stringify(html)+'\n');
 console.log(file,Buffer.byteLength(html),'bytes');
}
