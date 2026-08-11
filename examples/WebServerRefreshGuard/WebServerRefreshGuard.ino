#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <DMDESPHub12.h>
#include <fonts/ElektronMart6x8.h>

DMDESPHub12 Disp(3, 1);
ESP8266WebServer server(80);

bool filesystemReady = false;

void handleRoot() {
  server.send(200, "text/plain",
              "DMDESPHub12 is running. Open /save to test a protected SPIFFS write.");
}

void handleSave() {
  if (!filesystemReady) {
    server.send(503, "text/plain", "SPIFFS is not available");
    return;
  }

  bool ok = false;
  {
    // Keep the guard alive for the complete filesystem write window.
    DMDESPHub12RefreshGuard guard(Disp);

    File file = SPIFFS.open("/guard-test.txt", "w");
    if (file) {
      ok = (file.println(F("Saved safely with DMDESPHub12RefreshGuard")) > 0);
      file.close();
    }
  }

  server.send(ok ? 200 : 500, "text/plain",
              ok ? "Saved with DMDESPHub12RefreshGuard" : "Save failed");
}

void setup() {
  Serial.begin(115200);
  delay(50);

  filesystemReady = SPIFFS.begin();
  if (!filesystemReady) {
    Serial.println(F("WARNING: SPIFFS.begin() failed"));
  }

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP("DMDESPHub12", "12345678")) {
    Serial.println(F("ERROR: WiFi access point failed"));
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_GET, handleSave);
  server.begin();

  Disp.setDoubleBuffer(true);
  if (!Disp.doubleBuffer()) {
    Serial.println(F("ERROR: double-buffer allocation failed"));
    return;
  }

  Disp.setBrightness(150);
  Disp.setRefreshIntervalUs(Disp.recommendedRefreshIntervalUs());
  Disp.setFont(ElektronMart6x8);
  Disp.clear();
  Disp.drawText(0, 0, "WEB GUARD");
  Disp.drawText(0, 8, "192.168.4.1");
  Disp.swapBuffersAndCopy();
  Disp.start();

  if (!Disp.refreshRunning()) {
    Serial.println(F("ERROR: DMDESPHub12 refresh did not start"));
  }
}

void loop() {
  server.handleClient();
  yield();
}
