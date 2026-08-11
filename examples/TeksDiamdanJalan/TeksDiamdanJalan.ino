/*
 * Contoh teks diam dan teks berjalan dengan DMDESPHub12.
 *
 * DMDESPHub12 menggunakan Timer0 untuk refresh panel. Jangan memanggil
 * Disp.refresh() secara manual. Disp.loop() adalah compatibility no-op dan
 * tidak diperlukan. Setiap frame selesai digambar harus di-commit dengan
 * swapBuffers() atau swapBuffersAndCopy() ketika double buffer aktif.
 */

#include <DMDESPHub12.h>
#include <fonts/ElektronMart6x8.h>

#define DISPLAYS_WIDE 2
#define DISPLAYS_HIGH 1

DMDESPHub12 Disp(DISPLAYS_WIDE, DISPLAYS_HIGH);

const char teksBerjalan[] = "TEKS BERJALAN DENGAN DMDESPHub12 - https://saweria.co/romaslutfi";
bool displayReady = false;
int16_t posisiX = 0;
uint32_t langkahBerikutnyaMs = 0;

void gambarFrame() {
  // Gambar ulang satu frame lengkap pada back buffer.
  Disp.clear();
  Disp.drawText(0, 0, "TEKS DIAM");
  Disp.drawText(posisiX, 8, teksBerjalan);

  // Commit tepat pada batas frame, lalu salin frame yang tampil ke back buffer.
  Disp.swapBuffersAndCopy();
}

void setup() {
  Serial.begin(115200);
  delay(50);

  Disp.setDoubleBuffer(true);
  if (!Disp.doubleBuffer()) {
    Serial.println(F("ERROR: double-buffer allocation failed"));
    return;
  }

  Disp.setBrightness(150);
  Disp.setRefreshIntervalUs(Disp.recommendedRefreshIntervalUs());
  Disp.setFont(ElektronMart6x8);

  posisiX = Disp.width();
  gambarFrame();
  Disp.start();

  if (!Disp.refreshRunning()) {
    Serial.println(F("ERROR: DMDESPHub12 refresh did not start"));
    return;
  }

  displayReady = true;
  langkahBerikutnyaMs = millis() + 50UL;
}

void loop() {
  if (!displayReady) {
    delay(1000);
    return;
  }

  const uint32_t sekarang = millis();
  if ((int32_t)(sekarang - langkahBerikutnyaMs) >= 0) {
    langkahBerikutnyaMs += 50UL;
    if ((int32_t)(sekarang - langkahBerikutnyaMs) > 100) {
      langkahBerikutnyaMs = sekarang + 50UL;
    }

    --posisiX;
    if (posisiX < -Disp.textWidth(teksBerjalan)) {
      posisiX = Disp.width();
    }

    gambarFrame();
  }

  yield();
}
