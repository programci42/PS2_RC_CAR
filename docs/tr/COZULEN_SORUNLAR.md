# Çözülen Sorunlar ve Öğrenilenler

Bu dosya çalışan PS2 robot aracını ortaya çıkaran sorunları ve çözümlerini, aynı hatalar tekrar edilmesin diye kalıcı olarak kaydeder.

## Güç tesisatı: ince kablo eridi ve konsol reset attı

Motorlar bağlı değilken PS2 Slim ve röle kartı ince kabloyla çalışıyor görünüyordu. RS-540 motorlar bağlanınca ilk kalkış akımı büyük gerilim düşümüne yol açtı; kablonun dış plastiği erimeye başladı ve PS2 anında reset attı. Çalışan çözüm daha kalın, akıma uygun kablo ve sağlam ekler oldu. Son tasarım ayrıca hat sigortası ve kısa motor akım yolları gerektiriyor. Boş röle yükü ile motorun kalkış yükü aynı değildir.

11,1 V 3S LiPo iki hatta ayrılır. 1. hat XL4016 buck dönüştürücüden geçip 8,50 V olarak harici adaptör girişli PS2 Slim SCPH-70004 modeline gider. 2. hat batarya geriliminde kalır: NC1–NC4 batarya eksiye, NO1–NO4 batarya artıya bağlanır. COM1 ve COM2 paralel sol motorları, COM3 ve COM4 paralel sağ motorları besler. Röle kartının 5 V güç ve HID veri kabloları PS2’nin iki USB portunu kullanır.

## Zıt yön komutları ve röle kontakları

Aynı anda basılan zıt yönler çelişkili motor durumu isteyebiliyordu. Yazılım artık karşıt yönleri denetleyip çakışmada bütün hareket rölelerini KAPATIR. Yön değiştirirken önce KAPAT, sonra varsayılan 100 ms ölü zamanı bekle, ardından AÇ sırası uygulanır. Kol kopması, SELECT acil durdurma ve programdan çıkış yolları her röleye ayrı KAPAT komutu yollar ve sonucu denetler.

## Belgesiz USB HID davranışı

Seda röle kartının güvenilir açık protokol belgesi yoktu; saklanan Windows test uygulaması referans oldu. Özel IOP sürücüsü `VID 0x178E / PID 0x07D9` aygıtını bulur, HID arayüzünü ve interrupt OUT endpoint’ini keşfeder, 12 aktarım çeşidini destekler. AUTO önce çalışan sekiz baytlık paketi interrupt OUT üzerinden dener; tanı modları rapor uzunluğu, türü, alıcı ve komut baytı konumlarını değiştirir.

Örnekler: röle AÇ komutları `1`, `2`, `3`, `4`; karşılık gelen KAPAT komutları `17`, `18`, `19`, `20`’dir. Normal paket `01 00 00 KOMUT 00 00 00 00` biçimindedir. EE uygulaması komutu SIF RPC ile yollar; IOP modülü USB aktarımını yapıp result, status, control ve interrupt tanı değerlerini döndürür.

## ELF 1,44 MB’tan 248 KB’a nasıl düştü?

`relay.elf` 1.509.884 baytlık geliştirici sürümüdür. Fazla alanın çoğunu DWARF hata ayıklama verisi oluşturur: kaynak satırı eşlemeleri, sembol adları, tür açıklamaları ve değişken bilgileri. PS2 bu haritayı çalıştırmaz; ELF içindeki LOAD bölümlerini belleğe alır. `build.ps1`, geliştirici ELF’ini `role.elf` adına kopyalayıp `mips64r5900el-ps2-elf-strip --strip-debug` çalıştırır. Dağıtım dosyası 254.224 bayt, Windows hesabıyla 248,3 KiB olur; gömülü `usbd.irx`, röle IOP modülü, SIF RPC, ayarlar, hafıza kartı desteği ve çalıştırılabilir veri korunur.

## Gerçek PS2’de otomatik açılış

Hedef konsol harici 8,5 V adaptör girişli PS2 Slim SCPH-70004 modelidir. `role.elf` dosyasını uLaunchELF ile Free McBoot hafıza kartında ayrı bir yola kopyala. Free McBoot Configurator içinde **Configure E1 launch keys** menüsünü aç, **AUTO** satırını bu ELF yoluna bağla, CNF dosyasını MC0’a kaydet ve yeniden başlat. Böylece araç yazılımı konsol açılır açılmaz çalışır. AUTO’yu etkinleştirmeden önce çalışan bir kaçış tuşu veya alternatif başlatıcı girişi bırak.

`C:\ps2dev\projeler\PS2OS_MLM\COZULEN_SORUNLAR.md` dosyasında kaydedilen bir ders burada da geçerlidir: uLaunchELF üzerinden açılan ELF hazır bir IOP ortamını miras alabilir, Free McBoot AUTO ise daha temiz bir ortamdan başlatabilir. Bu nedenle kendi kendine yeten ELF, gömülü IRX modüllerini yüklemeden önce IOP resetini, `SifLoadFileInit`, IOP heap kurulumunu ve SBV yamalarını kendisi yapmalıdır. Araç uygulamasının `main.c` dosyası bu disiplini uygular.

## Hareket programları ve tanı

Uygulama 128 adede kadar bir saniyelik hareket veya bekleme adımı kaydeder. L1 `mc0:SYS-CONF/ROLEPRG.TXT` dosyasını kaydeder, L2 yükler, START oynatır. R1 ekranında VID, PID, USB gönderim modu, otomatik USB yükleme ve ölü zaman ayarlanır. PCSX2 menü ve kol mantığını sınayabilir; gerçek röle USB yolunu güvenilir biçimde taklit edemediği için ekrandaki result/status/control/interrupt değerleri saha tanısında önemlidir.

## Yol testi videoları

- [PS2 robot araç kısa video 1](https://www.youtube.com/shorts/x-vaeMoXdgk)
- [PS2 robot araç kısa video 2](https://www.youtube.com/shorts/5uW2evH4y8U)
