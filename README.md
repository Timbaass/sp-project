# Mini Dosya Sistemi Simulatoru

Bu proje, gercek disk yerine tek bir binary dosya uzerinde calisan kucuk bir
dosya sistemi simulatorudur. C ile yazilmistir ve komut satirindan kullanilir.

## Ozellikler

- Tek disk dosyasi uzerinde calisir.
- Dosya olusturma, silme, yazma, okuma ve listeleme desteklenir.
- Superblock, inode tablosu, dizin girdileri ve FAT benzeri blok tablosu kullanir.
- Bos blok yonetimi yapar.
- Dosya boyutu, ilk blok ve kullanilan blok sayisini izler.
- `status` komutu ile dosya sistemi durumu goruntulenir.
- Hatali erisimlerde acik hata mesaji verir.

## Derleme

GCC veya MinGW:

```sh
gcc -std=c11 -Wall -Wextra -O2 main.c commands.c mfs.c -o minifs
```

Make:

```sh
make
```

MSVC:

```bat
cl /std:c11 /W4 main.c commands.c mfs.c /Fe:minifs.exe
```

## Kullanim

```sh
minifs init <disk> [blok_sayisi]
minifs create <disk> <dosya_adi>
minifs delete <disk> <dosya_adi>
minifs write <disk> <dosya_adi> <metin...>
minifs read <disk> <dosya_adi>
minifs list <disk>
minifs status <disk>
```

Ornek:

```sh
./minifs init disk.bin 128
./minifs create disk.bin not.txt
./minifs write disk.bin not.txt Merhaba mini dosya sistemi
./minifs read disk.bin not.txt
./minifs list disk.bin
./minifs status disk.bin
./minifs delete disk.bin not.txt
```

## Hizli Test

Windows PowerShell:

```powershell
.\test.ps1
```

Bu script once `gcc`, sonra `cl` derleyicisini arar. Derleyici bulursa projeyi
derler; ardindan disk olusturma, dosya olusturma, yazma, okuma, listeleme,
durum gosterme ve silme komutlarini sirayla test eder.

## Disk Tasarimi

Disk dosyasi iki ana bolumden olusur:

1. Metadata bolumu
2. Sabit boyutlu veri bloklari

Metadata bolumunde su yapilar tutulur:

- `SuperBlock`: sihirli imza, blok boyutu, toplam blok sayisi, bos blok sayisi,
  dosya sayisi.
- `Inode`: dosya boyutu, ilk veri blogu ve blok sayisi.
- `DirectoryEntry`: dosya adi ve inode indeksini esleyen dizin kaydi.
- `fat`: her veri blogu icin bir sonraki blok indeksini tutan blok tablosu.

FAT tablosunda `-2` bos blogu, `-1` dosya sonunu, sifir veya pozitif degerler
ise zincirdeki sonraki veri blogunu ifade eder.

Varsayilan blok boyutu `512` bayttir. En fazla `64` dosya ve `1024` veri blogu
desteklenir.

## Kod Yapisi

- `main.c`: Komut satirindan gelen komutu ilgili fonksiyona yonlendirir.
- `commands.c` / `commands.h`: `init`, `create`, `delete`, `write`, `read`,
  `list` ve `status` komutlarini uygular.
- `mfs.c` / `mfs.h`: Metadata okuma/yazma, blok ayirma, blok serbest birakma,
  dosya adi kontrolu ve dizin/inode arama gibi cekirdek dosya sistemi
  islemlerini icerir.
- `Makefile`: GCC/MinGW ile kolay derleme icin eklenmistir.
