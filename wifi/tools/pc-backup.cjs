const fs=require('fs'),path=require('path'),crypto=require('crypto');
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
function archive(root,bytes){const dir=path.join(root,'Backups','PS2-Wifi');fs.mkdirSync(dir,{recursive:true});const file=path.join(dir,sha(bytes)+'.elf');if(!fs.existsSync(file)){const fd=fs.openSync(file,'wx');try{fs.writeFileSync(fd,bytes);fs.fsyncSync(fd);}finally{fs.closeSync(fd);}}if(!fs.readFileSync(file).equals(bytes))throw Error('PC backup read-back failed');return file;}
module.exports={archive,sha};
