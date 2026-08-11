#include <DMDESPHub12.h>
#include <fonts/ElektronMart6x8.h>

DMDESPHub12 Disp(3, 1);

const char runningText[] = "FRAME-SYNCHRONIZED RUNNING TEXT - https://saweria.co/romaslutfi";
bool displayReady = false;
int16_t scrollX = 0;
uint32_t nextStepMs = 0;

void drawFrame() {
  Disp.clear();
  Disp.drawText(0, 0, "SMOOTH TEXT");
  Disp.drawText(scrollX, 8, runningText);
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

  scrollX = Disp.width();
  drawFrame();
  Disp.start();

  if (!Disp.refreshRunning()) {
    Serial.println(F("ERROR: DMDESPHub12 refresh did not start"));
    return;
  }

  displayReady = true;
  nextStepMs = millis() + 50UL;
}

void loop() {
  if (!displayReady) {
    delay(1000);
    return;
  }

  const uint32_t now = millis();
  if ((int32_t)(now - nextStepMs) >= 0) {
    nextStepMs += 50UL;
    if ((int32_t)(now - nextStepMs) > 100) {
      nextStepMs = now + 50UL;
    }

    --scrollX;
    if (scrollX < -Disp.textWidth(runningText)) {
      scrollX = Disp.width();
    }

    drawFrame();
  }

  yield();
}
