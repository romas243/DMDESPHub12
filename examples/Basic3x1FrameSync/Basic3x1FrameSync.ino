#include <DMDESPHub12.h>
#include <fonts/ElektronMart6x8.h>

DMDESPHub12 Disp(3, 1);

bool displayReady = false;
bool alternateFrame = false;
uint32_t nextFrameMs = 0;
uint32_t nextStatsMs = 0;

void drawFrame(bool alternate) {
  Disp.clear();

  if (alternate) {
    Disp.drawText(0, 0, "FRAME B");
    Disp.drawText(0, 8, "SYNC OK");
  } else {
    Disp.drawText(0, 0, "FRAME A");
    Disp.drawText(0, 8, "DMDESPHub12");
  }

  // Before start(), this commits immediately. After start(), the swap is
  // synchronized by the Timer0 ISR immediately before scan phase 0.
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

  drawFrame(false);
  Disp.start();

  if (!Disp.refreshRunning()) {
    Serial.println(F("ERROR: DMDESPHub12 refresh did not start"));
    return;
  }

  displayReady = true;
  const uint32_t now = millis();
  nextFrameMs = now + 1000UL;
  nextStatsMs = now + 5000UL;

  Serial.printf("DMDESPHub12 %s, interval=%lu us\n",
                DMDESPHUB12_VERSION_STRING,
                (unsigned long)Disp.refreshIntervalUs());
}

void loop() {
  if (!displayReady) {
    delay(1000);
    return;
  }

  const uint32_t now = millis();

  if ((int32_t)(now - nextFrameMs) >= 0) {
    nextFrameMs += 1000UL;
    if ((int32_t)(now - nextFrameMs) > 1000) {
      nextFrameMs = now + 1000UL;
    }

    alternateFrame = !alternateFrame;
    drawFrame(alternateFrame);
  }

  if ((int32_t)(now - nextStatsMs) >= 0) {
    nextStatsMs += 5000UL;
    const uint32_t util = Disp.refreshUtilizationPermille();

    Serial.printf("ISR=%lu overrun=%lu util=%lu.%01lu%% swap=%lu timeout=%lu\n",
                  (unsigned long)Disp.refreshISRCount(),
                  (unsigned long)Disp.refreshOverruns(),
                  (unsigned long)(util / 10UL),
                  (unsigned long)(util % 10UL),
                  (unsigned long)Disp.swapCount(),
                  (unsigned long)Disp.swapTimeouts());
  }

  yield();
}
