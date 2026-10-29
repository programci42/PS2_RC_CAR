# PS2 Robot Araç — Emülatör ekranları

Play! 0.70 üzerinden alınan 18 görüntü: 9 ekran × 2 dil. `UI TOUR` etiketli görüntüler, üretim kodundaki çizim işlevlerini kullanan ayrı ekran inceleme derlemesidir. Görünen USB sonuçları örnek veridir; fiziksel röle, motor ve hafıza kartı doğrulaması değildir.

[Görsel galeriyi aç](index.html) · [Dağıtım ELF’sinin gerçek açılış görüntüsü](en-main-live.jpg)

| Ekran | Türkçe | English |
|---|---|---|
| Ana ekran | [Görüntü](tr-01-tour.jpg) | [Image](en-01-tour.jpg) |
| Ayarlar şifresi | [Görüntü](tr-02-tour.jpg) | [Image](en-02-tour.jpg) |
| Yanlış şifre uyarısı | [Görüntü](tr-03-tour.jpg) | [Image](en-03-tour.jpg) |
| Motor grupları ve dil ayarları | [Görüntü](tr-04-tour.jpg) | [Image](en-04-tour.jpg) |
| VID düzenleme | [Görüntü](tr-05-tour.jpg) | [Image](en-05-tour.jpg) |
| PID düzenleme | [Görüntü](tr-06-tour.jpg) | [Image](en-06-tour.jpg) |
| Program kaydı | [Görüntü](tr-07-tour.jpg) | [Image](en-07-tour.jpg) |
| Program yürütme | [Görüntü](tr-08-tour.jpg) | [Image](en-08-tour.jpg) |
| Röle komut ayrıntısı | [Görüntü](tr-09-tour.jpg) | [Image](en-09-tour.jpg) |

Yeniden üretim: `node tests/build-ui-tour.cjs --build-all`. Çıktılar `tests/ui-tour/TOUR_EN_01.ELF` … `TOUR_TR_09.ELF` olur. Her ELF, bir ekranı sabit gösterir. Play! komut satırı: `Play.exe --elf <dosya>`. Görüntüler Windows pencere yakalamasıyla alınmıştır; sonradan çizilmemiş veya değiştirilmemiştir.
