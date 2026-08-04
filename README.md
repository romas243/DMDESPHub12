# DMDESPHub12 v1.1.0

DMDESPHub12 adalah library ESP8266 untuk panel LED monochrome P10/HUB12 **flicker free** dengan refresh deterministik berbasis Timer0, SPI1 FIFO, double buffering, dan swap framebuffer yang tersinkron pada batas frame.

Library ini merupakan pengembangan terbuka dari DMDESP. Notice, atribusi, README historis, dan file lisensi proyek asal tetap dipertahankan. Salinan dokumentasi asli tersedia pada:

- `README.md` — README historis tetap dipertahankan
- `LICENSE` dan `license.txt` — file lisensi asli.

## Identitas teknis

Seluruh identitas teknis telah dipisahkan dari DMDESP asli:

```cpp
#include <DMDESPHub12.h>

DMDESPHub12 Disp(3, 1);
```

- Nama library: `DMDESPHub12`
- Header publik: `DMDESPHub12.h`
- Class publik: `DMDESPHub12`
- Guard flash/OTA: `DMDESPHub12RefreshGuard`
- Base bitmap internal: `DMDESPHub12Bitmap`
- Prefix macro: `DMDESPHUB12_`
- 
## Fitur utama

- Flicker-Free engine display untuk panel P10 HUB12.
- ESP8266 Timer0 hard refresh; scan panel tidak bergantung pada kecepatan `loop()`.
- SPI1/HSPI FIFO untuk mengurangi durasi dan stack ISR.
- Swap framebuffer tersinkron tepat sebelum phase 0.
- Double buffering dengan API `swapBuffers()` dan `swapBuffersAndCopy()`.
- `DMDESPHub12RefreshGuard` untuk operasi flash, filesystem, EEPROM, dan OTA.
- Diagnostik ISR, overrun, utilization, swap, dan timeout.
- Target konservatif untuk panel 1x1 hingga 6x1 HUB12.

## Instalasi

1. Unduh atau clone repository `DMDESPHub12`.
2. Pastikan folder library bernama `DMDESPHub12`.
3. Tempatkan folder pada `Arduino/libraries/` atau instal ZIP melalui Arduino IDE.
4. Restart Arduino IDE.
5. Pilih board ESP8266.

## Contoh dasar

```cpp
#include <DMDESPHub12.h>
#include <fonts/ElektronMart6x8.h>

DMDESPHub12 Disp(3, 1);

void setup() {
  Disp.setDoubleBuffer(true);
  Disp.setBrightness(150);
  Disp.setRefreshIntervalUs(1000);
  Disp.setFont(ElektronMart6x8);

  Disp.clear();
  Disp.drawText(0, 0, "DMDESPHub12");
  Disp.swapBuffers();
  Disp.start();
}

void loop() {
  yield();
}
```

Jangan memanggil `Disp.refresh()` secara manual setelah `Disp.start()` aktif. Timer0 dan SPI1/HSPI harus dianggap sebagai resource eksklusif library.

## Example yang disertakan

1. `Basic3x1FrameSync` — pergantian dua frame dan diagnostik FrameSync.
2. `SmoothText3x1` — running text nonblocking dengan double buffer.
3. `TeksDiamdanJalan` — contoh Indonesia untuk teks diam dan berjalan; tidak memakai `Disp.loop()`.
4. `WebServerRefreshGuard` — penulisan SPIFFS yang dilindungi `DMDESPHub12RefreshGuard`.
5. `PanelStress6x1` — stress test 6x1 dengan interval rekomendasi dan diagnostik.

Pola umum example dinamis adalah:

```cpp
Disp.setDoubleBuffer(true);
Disp.clear();
// gambar seluruh frame pada back buffer
Disp.swapBuffersAndCopy();
Disp.start();
```

Setelah refresh aktif, setiap perubahan frame diselesaikan dengan `swapBuffers()` atau `swapBuffersAndCopy()` dari main context. `Disp.loop()` tidak diperlukan karena scan panel dijalankan Timer0.

## Frame-synchronized swap

Pada v1.1.0, `swapBuffers()` meminta pertukaran pointer dan menunggu hingga ISR mencapai batas frame berikutnya. Pertukaran dilakukan tepat sebelum phase 0 dibaca, sehingga phase 0–3 dalam satu full scan berasal dari framebuffer yang sama.

```cpp
Disp.clear();
Disp.drawText(0, 0, "FRAME A");
Disp.swapBuffersAndCopy();
```

Fungsi swap hanya boleh dipanggil dari main context/`loop()`, bukan dari ISR, `Ticker`, atau blok yang sedang menonaktifkan interrupt.

## DMDESPHub12RefreshGuard wajib untuk penulisan flash

Gunakan `DMDESPHub12RefreshGuard` pada handler WebServer yang menulis filesystem, EEPROM, atau flash:

```cpp
void handleSave() {
  bool ok = false;
  {
    DMDESPHub12RefreshGuard guard(Disp);

    File file = SPIFFS.open("/config.json", "w");
    if (file) {
      ok = serializeJson(config, file) > 0;
      file.close();
    }
  }

  server.send(ok ? 200 : 500, "text/plain",
              ok ? "Saved" : "Save failed");
}
```

Guard wajib untuk:

- `SPIFFS`, `LittleFS`, atau file konfigurasi JSON yang ditulis ke flash;
- `EEPROM.commit()`;
- proses `Update.write()` pada OTA;
- operasi erase/write flash lainnya.

Guard tidak diperlukan untuk:

- `server.handleClient()`;
- respons HTTP biasa tanpa penulisan flash;
- rendering ke framebuffer;
- pembacaan konfigurasi dari RAM.

Untuk OTA berbasis upload bertahap, guard harus tetap hidup sejak `UPLOAD_FILE_START` sampai `UPLOAD_FILE_END`, `UPLOAD_FILE_ABORTED`, atau error. Jangan membuat guard lokal yang langsung hancur setelah `Update.begin()`.

## Interval refresh yang direkomendasikan

| Konfigurasi | Minimum library | Titik awal yang direkomendasikan |
|---|---:|---:|
| 1x1–3x1 | 500 µs | 1000 µs |
| 4x1 | 650 µs | 1000 µs |
| 5x1 | 800 µs | 1100 µs |
| 6x1 | 950 µs | 1200 µs |

Untuk 5x1 dan 6x1, pantau:

```cpp
Disp.refreshOverruns();
Disp.refreshUtilizationPermille();
Disp.swapTimeouts();
```

Target operasi normal:

```text
refreshOverruns = 0
swapTimeouts    = 0
```

## Kepemilikan Timer0 dan HSPI

DMDESPHub12 dirancang untuk satu instance aktif. Jangan menggunakan Timer0 atau SPI1/HSPI untuk peripheral lain selama refresh aktif. Perangkat seperti SD card, TFT, LoRa, W5500, dan sensor SPI tidak boleh berbagi HSPI secara bersamaan dengan panel P10.


## Atribusi dan lisensi

DMDESPHub12 berasal dari DMDESP oleh Bonny Useful/ElektronMart, port HJS589/dmk007, serta DMD/Bitmap karya Southern Storm Software. Header notice asli tetap berada pada source dan tidak boleh dihapus. File lisensi asli disertakan tanpa perubahan.

Perubahan nama dan pengembangan DMDESPHub12 tidak menyiratkan dukungan atau tanggung jawab dari para pengembang asli atas bug pada fork ini.

## Repository

Nama repository harus tetap:

```text
DMDESPHub12
```
