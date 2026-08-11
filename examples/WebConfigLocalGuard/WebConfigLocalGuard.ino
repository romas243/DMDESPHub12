/*
  DMDESPHub12 - WebConfigLocalGuard

  Demonstrates the recommended LOCAL/scoped RefreshGuard pattern for
  short flash/filesystem writes from an ESP8266WebServer handler.

  Key rule:
    Keep DMDESPHub12RefreshGuard alive ONLY for the SPIFFS write window.

  This LOCAL/scoped pattern is intended for short synchronous operations
  such as configuration writes. For multi-callback operations such as OTA,
  keep a guard alive across the complete START/WRITE/END lifetime instead.

  The request is parsed and validated before the guard is created.
  The HTTP response is sent after the guard has gone out of scope, so the
  display refresh has already resumed.

  Hardware/reference configuration:
    ESP8266 + 3x1 HUB12/P10 panels

  WiFi AP:
    SSID     : DMDHub12-Guard
    Password : 12345678
    Open     : http://192.168.4.1/
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <DMDESPHub12.h>
#include <fonts/ElektronMart6x8.h>

DMDESPHub12 Disp(3, 1);
ESP8266WebServer server(80);

static const char *CONFIG_PATH = "/webconfig.txt";

struct WebConfig {
  uint16_t brightness;
  String message;
};

WebConfig config = {150, "LOCAL GUARD"};
bool filesystemReady = false;

// -----------------------------------------------------------------------------
// Small helpers
// -----------------------------------------------------------------------------

bool parseBrightnessStrict(const String &input, uint16_t &valueOut) {
  if (input.length() == 0) {
    return false;
  }

  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input.charAt(i);
    if (c < '0' || c > '9') {
      return false;
    }
  }

  const long value = input.toInt();
  if (value < 0 || value > 1023) {
    return false;
  }

  valueOut = (uint16_t)value;
  return true;
}

bool validMessage(const String &message) {
  if (message.length() == 0 || message.length() > 24) {
    return false;
  }

  // The example stores one key/value pair per line. Reject line breaks so a
  // submitted message cannot create extra configuration records.
  return message.indexOf('\n') < 0 && message.indexOf('\r') < 0;
}

String htmlEscape(const String &input) {
  String out;
  out.reserve(input.length() + 16);

  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input.charAt(i);
    switch (c) {
      case '&': out += F("&amp;");  break;
      case '<': out += F("&lt;");   break;
      case '>': out += F("&gt;");   break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;");  break;
      default:  out += c;           break;
    }
  }

  return out;
}

void drawCurrentConfig() {
  Disp.clear();
  Disp.setFont(ElektronMart6x8);
  Disp.drawText(0, 0, "WEB CONFIG");
  Disp.drawText(0, 8, config.message);
  Disp.swapBuffersAndCopy();
}

// Reads are performed during setup BEFORE Disp.start(), so no RefreshGuard is
// required here.
void loadConfig() {
  if (!filesystemReady || !SPIFFS.exists(CONFIG_PATH)) {
    return;
  }

  File file = SPIFFS.open(CONFIG_PATH, "r");
  if (!file) {
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.startsWith("brightness=")) {
      const long value = line.substring(11).toInt();
      if (value >= 0 && value <= 1023) {
        config.brightness = (uint16_t)value;
      }
    } else if (line.startsWith("message=")) {
      String value = line.substring(8);
      value.trim();
      if (value.length() > 0 && value.length() <= 24) {
        config.message = value;
      }
    }
  }

  file.close();
}

// This helper demonstrates the important pattern:
//   - payload already exists before entering the guard
//   - only SPIFFS open/write/close is protected
//   - any return inside the scope is safe because the RAII destructor runs
bool writeConfigSafely(const String &payload) {
  if (!filesystemReady) {
    return false;
  }

  {
    DMDESPHub12RefreshGuard guard(Disp);

    File file = SPIFFS.open(CONFIG_PATH, "w");
    if (!file) {
      // Safe early return: guard destructor automatically restores refresh.
      return false;
    }

    const size_t written = file.print(payload);
    file.flush();
    file.close();

    if (written != payload.length()) {
      // Also safe: refresh is restored when leaving this scope.
      return false;
    }
  }

  // Refresh is already running again here.
  return true;
}

// -----------------------------------------------------------------------------
// Web handlers
// -----------------------------------------------------------------------------

void handleRoot() {
  String page;
  page.reserve(1400);

  page += F("<!doctype html><html><head>");
  page += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  page += F("<title>DMDESPHub12 Local Guard</title>");
  page += F("<style>body{font-family:Arial,sans-serif;max-width:560px;margin:32px auto;padding:0 16px;}input{width:100%;box-sizing:border-box;padding:8px;margin:5px 0 14px;}button{padding:10px 18px;}code{background:#eee;padding:2px 4px;}</style>");
  page += F("</head><body>");
  page += F("<h2>DMDESPHub12 WebConfigLocalGuard</h2>");
  page += F("<p>This example protects only the SPIFFS write window with <code>DMDESPHub12RefreshGuard</code>.</p>");
  page += F("<form method='POST' action='/config'>");
  page += F("<label>Display message (1-24 characters)</label>");
  page += F("<input name='message' maxlength='24' value='");
  page += htmlEscape(config.message);
  page += F("'>");
  page += F("<label>Brightness PWM value (0-1023)</label>");
  page += F("<input name='brightness' type='number' min='0' max='1023' value='");
  page += String(config.brightness);
  page += F("'>");
  page += F("<button type='submit'>Save configuration</button>");
  page += F("</form>");
  page += F("<p><a href='/status'>View status</a></p>");
  page += F("</body></html>");

  server.send(200, "text/html", page);
}

void handleConfigUpdate() {
  // 1) Parse and validate request OUTSIDE the guard.
  if (!server.hasArg("message") || !server.hasArg("brightness")) {
    server.send(400, "text/plain", "Missing message or brightness");
    return;
  }

  String newMessage = server.arg("message");
  newMessage.trim();

  if (!validMessage(newMessage)) {
    server.send(400, "text/plain", "Message must contain 1-24 characters and no line breaks");
    return;
  }

  uint16_t newBrightness = 0;
  if (!parseBrightnessStrict(server.arg("brightness"), newBrightness)) {
    server.send(400, "text/plain", "Brightness must be an integer from 0 to 1023");
    return;
  }

  // 2) Build the complete payload BEFORE pausing display refresh.
  String payload;
  payload.reserve(newMessage.length() + 48);
  payload += F("brightness=");
  payload += String((unsigned int)newBrightness);
  payload += '\n';
  payload += F("message=");
  payload += newMessage;
  payload += '\n';

  // 3) The RefreshGuard lives only inside writeConfigSafely().
  if (!writeConfigSafely(payload)) {
    server.send(500, "text/plain", "SPIFFS write failed");
    return;
  }

  // 4) The guard has already been destroyed. Refresh is active again.
  config.brightness = newBrightness;
  config.message = newMessage;
  Disp.setBrightness(config.brightness);
  drawCurrentConfig();

  // 5) Send the HTTP response AFTER refresh has resumed.
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "Saved");
}

void handleStatus() {
  String text;
  text.reserve(256);
  text += F("DMDESPHub12 WebConfigLocalGuard\n");
  text += F("refreshRunning=");
  text += Disp.refreshRunning() ? F("true") : F("false");
  text += F("\nrefreshIntervalUs=");
  text += String(Disp.refreshIntervalUs());
  text += F("\nrefreshOverruns=");
  text += String(Disp.refreshOverruns());
  text += F("\nswapTimeouts=");
  text += String(Disp.swapTimeouts());
  text += F("\nmessage=");
  text += config.message;
  text += F("\nbrightness=");
  text += String(config.brightness);
  text += '\n';

  server.send(200, "text/plain", text);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// -----------------------------------------------------------------------------
// Arduino setup/loop
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(50);

  // Mount/read filesystem before display refresh starts.
  filesystemReady = SPIFFS.begin();
  if (!filesystemReady) {
    Serial.println(F("WARNING: SPIFFS.begin() failed; saving is disabled"));
  } else {
    loadConfig();
  }

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP("DMDHub12-Guard", "12345678")) {
    Serial.println(F("ERROR: WiFi access point failed"));
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/config", HTTP_POST, handleConfigUpdate);
  server.on("/status", HTTP_GET, handleStatus);
  server.onNotFound(handleNotFound);
  server.begin();

  Disp.setDoubleBuffer(true);
  if (!Disp.doubleBuffer()) {
    Serial.println(F("ERROR: double-buffer allocation failed"));
    return;
  }

  Disp.setBrightness(config.brightness);
  Disp.setRefreshIntervalUs(Disp.recommendedRefreshIntervalUs());
  Disp.setFont(ElektronMart6x8);
  Disp.clear();
  Disp.drawText(0, 0, "WEB CONFIG");
  Disp.drawText(0, 8, config.message);
  Disp.swapBuffersAndCopy();
  Disp.start();

  if (!Disp.refreshRunning()) {
    Serial.println(F("ERROR: DMDESPHub12 refresh did not start"));
  }

  Serial.println();
  Serial.println(F("DMDESPHub12 WebConfigLocalGuard ready"));
  Serial.print(F("Open http://"));
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
  yield();
}
