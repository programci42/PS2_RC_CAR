# PS2 Robot Araç — 26.10.2026

## Kullanım

R1 ile ayarlara girilir. Ekranda gösterilen tuş sırasını tamamlayıp START ile
onaylayın. Sol ve sağ motor satırlarında SOL/SAG ile grubu seçin:
R1 & R2, R3 & R4, R5 & R6, R7 & R8. Diğer tarafın seçtiği grup otomatik atlanır.
Grup değişikliği fiziksel kabloları değiştirmez; motor uçları seçilen grubun COM
uçlarına bağlanmalıdır. Varsayılan sol R1/R2, sağ R3/R4 olarak kalır.

Dil satırında Türkçe/İngilizce seçilir ve tüm menü metinleri anında değişir.
START ayarları `mc0:SYS-CONF/ROLECFG.TXT` dosyasına kaydeder; SELECT çıkar.
PS2'nin yerleşik yazı tipinde Türkçe harfler olmadığı için Türkçe metinler
`Turkce`, `Ayarlar`, `Sag` gibi okunabilir ASCII harflerle yazılır.

## Açılış testi

Uygulama USB sürücüsünü otomatik yükler; kart hazır olduğunda sekiz röle için
önce OFF, ardından ON komutları gönderilir. Son ON komutundan sonra 100 ms
beklenir, ardından sekiz röle ayrı ayrı kapatılır. Başarılı test aynı uygulama
oturumunda tekrarlanmaz. Eski `autoload=0` ayarı bu kontrolü engellemez.

Bir komut başarısız olursa test başarısız sayılır ve sürüş engellenir. Sorun
giderildikten sonra L1 ile yeniden denenebilir. Testten ve acil durdurmadan sonra
hareket tuşlarını bırakıp yeniden basmak gerekir.

Komutlar USB'den sırayla gönderildiği için her rölenin enerjili kalma süresi
100 ms'ye aktarım süresini de ekler. Gerçek kart üzerinde süre ölçümü yapılmadı.
Belgelenmiş NC=eksi, NO=artı bağlantısında aynı grubun iki rölesinin birlikte
açılması motorun iki ucunu artıya bağlar; motoru frenler. Sıralı anahtarlamada
kısa geçiş hareketleri oluşabilir. İlk fiziksel denemeyi tekerler yerden kesikken yapın.

## Kurulum ve çıktılar

- Kaynak: `C:\ps2dev\projeler\role`
- MSYS2: `C:\msys64`; make, xorriso, Git ve gerekli 32/64 bit derleyici çalışma kitaplıkları kuruldu.
- PS2DEV / PS2SDK: `C:\ps2dev` ve `C:\ps2dev\ps2sdk`; GCC 15.2.0.
- MSYS2 ortamı: `/etc/profile.d/ps2dev.sh`.
- `build.ps1`: ELF oluşturur; başka klasörden çağrılabilir.
- `build-disc.ps1`: ELF + `role-disc.iso` oluşturur.
- `role.elf`: gerçek PS2'ye aktarılacak uygulama.
- PCSX2: `C:\ps2dev\emulators\pcsx2`, sürüm 2.8.2.
- Play!: `C:\ps2dev\emulators\play`, sürüm 0.70; BIOS dosyası gerektirmez.
- `run-play.ps1`: dağıtım ELF'sini Play! ile açar. `-ScreenTour -Language TR -Page 4`
  seçenekleri Türkçe ayar ekranını inceleme derlemesinde açar.
- Fiziksel araçta güncel dönüştürücü **XL4016**; eski sunum/görseller geçmiş sürümü anlatır.

## Doğrulama ve sınırlar

`node tests/verify.cjs`: 430 kontrol senaryosu ve 126 çeviri biçimi eşleşmesi.
Gerçek `main.c` işlevleri ana bilgisayarda derlenip sınanır: grup çakışmaları,
yön değiştirme sırası, 100 ms yazılım beklemesi, tüm açılış komutlarında hata
enjeksiyonu, tek seferlik test, eski kayıt uyumluluğu ve yeni ayarların kaydı.

Play! üzerinde dağıtım ELF'sinin ana ekranı açıldı. OpenPS2BIOS 0.4 ile PCSX2
diskten ELF'yi yükledi fakat SIF başlangıcında ilerlemedi. Sony BIOS'u indirilmedi.
Tüm ekranların görüntüleri ayrı UI turunda aynı üretim çizim işlevleri kullanılarak
emülatörden alındı; turda USB, kontrolcü akışı ve hafıza kartı doğrulanmaz.
`tests/ui-tour/UI.ELF` yalnız ekran inceleme aracıdır, araca yüklenmemelidir.
Fiziksel Seda kartı, gerçek motorlar ve gerçek PS2'de hafıza kartı testi yapılmadı.

[Ekran görüntüleri](../screenshots/README.md)
