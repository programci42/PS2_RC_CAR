const fs=require('node:fs'),path=require('node:path');
const dir=path.resolve(__dirname,'../docs/screenshots');
const pages=['Ana ekran','Ayarlar şifresi','Yanlış şifre uyarısı','Motor grupları ve dil ayarları','VID düzenleme','PID düzenleme','Program kaydı','Program yürütme','Röle komut ayrıntısı'];
let rows='',md='# PS2 Robot Araç — Emülatör ekranları\n\n';
md+='Play! 0.70 üzerinden alınan 18 görüntü: 9 ekran × 2 dil. `UI TOUR` etiketli görüntüler, üretim kodundaki çizim işlevlerini kullanan ayrı ekran inceleme derlemesidir. Görünen USB sonuçları örnek veridir; fiziksel röle, motor ve hafıza kartı doğrulaması değildir.\n\n';
md+='[Görsel galeriyi aç](index.html) · [Dağıtım ELF’sinin gerçek açılış görüntüsü](en-main-live.jpg)\n\n';
md+='| Ekran | Türkçe | English |\n|---|---|---|\n';
for(let i=0;i<pages.length;i++) {
 const n=String(i+1).padStart(2,'0');
 for(const lang of ['tr','en'])if(!fs.existsSync(path.join(dir,`${lang}-${n}-tour.jpg`)))throw Error('Missing screenshot '+lang+n);
 rows+=`<section><h2>${n} — ${pages[i]}</h2><div class="pair">`;
 for(const [lang,label]of[['tr','Türkçe'],['en','English']])rows+=`<figure><a href="${lang}-${n}-tour.jpg"><img loading="lazy" src="${lang}-${n}-tour.jpg" alt="${pages[i]} — ${label}"></a><figcaption>${label}</figcaption></figure>`;
 rows+='</div></section>';
 md+=`| ${pages[i]} | [Görüntü](tr-${n}-tour.jpg) | [Image](en-${n}-tour.jpg) |\n`;
}
md+='\nYeniden üretim: `node tests/build-ui-tour.cjs --build-all`. Çıktılar `tests/ui-tour/TOUR_EN_01.ELF` … `TOUR_TR_09.ELF` olur. Her ELF, bir ekranı sabit gösterir. Play! komut satırı: `Play.exe --elf <dosya>`. Görüntüler Windows pencere yakalamasıyla alınmıştır; sonradan çizilmemiş veya değiştirilmemiştir.\n';
fs.writeFileSync(path.join(dir,'README.md'),md);
fs.writeFileSync(path.join(dir,'index.html'),`<!doctype html><html lang="tr"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>PS2 Robot Araç — Ekran Galerisi</title><style>body{margin:0;background:#111820;color:#eef3f7;font:16px/1.6 system-ui,sans-serif}main{max-width:1440px;margin:auto;padding:36px 24px}h1{font-size:clamp(26px,4vw,42px);line-height:1.2;margin:0 0 16px}h2{font-size:22px;margin:36px 0 14px}p{max-width:1000px;color:#c1ced9}.note{border-left:4px solid #edbc55;padding:12px 18px;background:#1e2833;border-radius:4px}.pair{display:grid;grid-template-columns:1fr 1fr;gap:18px}figure{margin:0;border:1px solid #344553;background:#080b10;border-radius:12px;overflow:hidden}img{width:100%;display:block}figcaption{padding:10px 16px;color:#c3d1dd}a{color:#86d4ff}section{margin-bottom:34px}@media(max-width:760px){.pair{grid-template-columns:1fr}main{padding:24px 14px}}</style><main><h1>PS2 Robot Araç</h1><p>Türkçe ve İngilizce arayüz • Play! 0.70 • 26 Ekim 2026</p><p class="note">18 gerçek emülatör pencere görüntüsü. UI TOUR, üretim kodunun çizim işlevlerini kullanan ekran inceleme derlemesidir. USB ve röle verileri örnektir; bu görüntüler fiziksel donanım testi değildir.</p><p><a href="en-main-live.jpg">Dağıtım ELF’sinin ana ekranda açıldığı gerçek görüntü</a></p>${rows}</main></html>`);
console.log('Gallery complete: 18 emulator screenshots');
