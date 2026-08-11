#include <DMDESPHub12.h>
#include <fonts/ElektronMart6x8.h>

#define PANELS_WIDE 6
#define PANELS_HIGH 1

DMDESPHub12 Disp(PANELS_WIDE, PANELS_HIGH);

const char runningText[] = "6X1 FRAME-SYNC FIFO STRESS TEST - https://saweria.co/romaslutfi";
bool displayReady = false;
int16_t scrollX = 0;
uint32_t nextFrameMs = 0;
uint32_t nextStatsMs = 0;

void drawFrame() {
  Disp.clear();
  Disp.drawText(0, 0, "DMDESPHub12 6X1");
  Disp.drawText(scrollX, 8, runningText);
  Disp.swapBuffersAndCopy();
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Disp.setDoubleBuffer(true);
  if (!Disp.doubleBuffer()) {
    Serial.println(F("ERROR: double-buffer allocation failed"));
    return;
  }

  Disp.setBrightness(150);
  Disp.setRefreshIntervalUs(Disp.recommendedRefreshIntervalUs());
  Disp.setFont(ElektronMart6x8);

  scrollX = Disp.width();
  drawFrame();
  Disp.start();

  if (!Disp.refreshRunning()) {
    Serial.println(F("ERROR: DMDESPHub12 refresh did not start"));
    return;
  }

  displayReady = true;
  const uint32_t now = millis();
  nextFrameMs = now + 50UL;
  nextStatsMs = now + 1000UL;

  Serial.printf("6x1 interval=%lu us, framebuffer=%u bytes x2\n",
                (unsigned long)Disp.refreshIntervalUs(),
                (unsigned)(Disp.stride() * Disp.height()));
}

void loop() {
  if (!displayReady) {
    delay(1000);
    return;
  }

  const uint32_t now = millis();

  if ((int32_t)(now - nextFrameMs) >= 0) {
    nextFrameMs += 50UL;
    if ((int32_t)(now - nextFrameMs) > 100) {
      nextFrameMs = now + 50UL;
    }

    --scrollX;
    if (scrollX < -Disp.textWidth(runningText)) {
      scrollX = Disp.width();
    }

    drawFrame();
  }

  if ((int32_t)(now - nextStatsMs) >= 0) {
    nextStatsMs += 1000UL;
    const uint32_t util = Disp.refreshUtilizationPermille();

    Serial.printf("ISR=%lu overrun=%lu max=%lu util=%lu.%01lu%% swap=%lu timeout=%lu heap=%u\n",
                  (unsigned long)Disp.refreshISRCount(),
                  (unsigned long)Disp.refreshOverruns(),
                  (unsigned long)Disp.maxRefreshCycles(),
                  (unsigned long)(util / 10UL),
                  (unsigned long)(util % 10UL),
                  (unsigned long)Disp.swapCount(),
                  (unsigned long)Disp.swapTimeouts(),
                  ESP.getFreeHeap());
  }

  yield();
}
