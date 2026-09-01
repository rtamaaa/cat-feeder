/*
 * ============================================================
 *  SMART CAT FEEDER -- STEP 4: WiFi + AP Mode Provisioning
 * ============================================================
 *  Tujuan  : ESP8266 konek ke WiFi Rumah B
 *            Jika belum ada konfigurasi --> masuk AP Mode
 *            AP Mode: HTTP server di 192.168.4.1 untuk setup WiFi
 *            Auto-reconnect non-blocking jika WiFi terputus
 *  Board   : ESP8266 NodeMCU
 *  Pin     : SDA=GPIO4, SCL=GPIO5, SERVO=GPIO14
 *  Library : Wire.h, RTClib.h, Servo.h, LittleFS.h,
 *            ArduinoJson.h, ESP8266WiFi.h, ESP8266WebServer.h
 *
 *  File LittleFS:
 *    /wifi_config.json  --> SSID + password WiFi rumah
 *    /schedule.json     --> jadwal feeding
 *
 *  MODE AP (Setup Awal):
 *    SSID    : CatFeeder-Setup
 *    Password: catfeeder123
 *    IP      : 192.168.4.1
 *    Endpoint: POST http://192.168.4.1/wifi  {ssid, password}
 *              GET  http://192.168.4.1/status
 *              GET  http://192.168.4.1/reset  (reset WiFi config)
 *
 *  TEST-11 : AP Mode aktif saat tidak ada WiFi config
 *  TEST-12 : Setup WiFi via HTTP di AP Mode
 *  TEST-13 : ESP8266 konek ke WiFi Rumah B
 *  TEST-14 : Auto-reconnect saat WiFi terputus
 *  TEST-15 : Jadwal tetap berjalan saat WiFi terputus
 *
 *  Perintah Serial Monitor (9600 baud):
 *    'd'  --> Status (WiFi, RTC, jadwal)
 *    'r'  --> Tampilkan jadwal
 *    'w'  --> Set jadwal (wizard)
 *    'f'  --> Force feeding
 *    'R'  --> Reset konfigurasi WiFi (masuk AP Mode)
 *    'W'  --> Tampilkan info WiFi
 *    'm'  --> Menu
 * ============================================================
 */

#include <Wire.h>
#include <RTClib.h>
#include <Servo.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ----- PIN -----
#define SDA_PIN    4
#define SCL_PIN    5
#define SERVO_PIN  14

// ----- SERVO PARAMS -----
#define SERVO_CLOSE     0
#define SERVO_OPEN      90
#define FEED_DURATION   2000
#define SERVO_SETTLE    400

// ----- FILE PATHS -----
#define WIFI_CONFIG_FILE  "/wifi_config.json"
#define SCHEDULE_FILE     "/schedule.json"

// ----- AP MODE CONFIG -----
#define AP_SSID      "CatFeeder-Setup"
#define AP_PASSWORD  "catfeeder123"
#define AP_IP_STR    "192.168.4.1"

// ----- WIFI RECONNECT -----
#define WIFI_CONNECT_TIMEOUT   15000   // ms tunggu konek
#define WIFI_RETRY_INTERVAL    30000   // ms antar retry reconnect

// ----- JADWAL -----
struct Schedule {
  bool enabled;
  int  hour;
  int  minute;
};
#define MAX_SCHEDULES 2
Schedule schedules[MAX_SCHEDULES];

// ----- ANTI-DUPLICATE -----
char lastFeedDate[11] = "";
int  lastFeedSlot     = -1;

// ----- OBJECTS -----
RTC_DS3231 rtc;
Servo feederServo;
ESP8266WebServer apServer(80);  // HTTP server di AP Mode

// ----- STATE -----
bool rtcReady   = false;
bool fsReady    = false;

enum AppWiFiState { WIFI_STATE_CONNECTING, WIFI_STATE_CONNECTED, WIFI_STATE_AP, WIFI_STATE_RETRY };
AppWiFiState wifiMode = WIFI_STATE_CONNECTING;

unsigned long wifiConnectStart  = 0;
unsigned long wifiRetryStart    = 0;
String        wifiSsid          = "";
String        wifiPassword      = "";
bool          wifiConfigExists  = false;

// AP Mode state
bool apModeActive    = false;
bool apSetupDone     = false;  // WiFi baru sudah di-set, perlu restart

// ----- SERVO STATE MACHINE -----
enum FeedState { FEED_IDLE, FEED_OPENING, FEED_OPEN_WAIT, FEED_CLOSING, FEED_DONE };
FeedState     feedState         = FEED_IDLE;
unsigned long feedTimer         = 0;
bool          feedingInProgress = false;
int           feedTriggerSlot   = -1;

// ----- SERIAL -----
String serialBuffer = "";
int    wizardStep   = 0;
int    wizardSlot   = 0;

// ----- TIMING -----
unsigned long lastCheckMs   = 0;
unsigned long lastStatusMs  = 0;
const unsigned long CHECK_INTERVAL  = 1000;
const unsigned long STATUS_INTERVAL = 10000;

// ============================================================
//  LOGGER
// ============================================================
String getTimestamp() {
  if (!rtcReady) return "[--:--:--]";
  DateTime now = rtc.now();
  char buf[12];
  snprintf(buf, sizeof(buf), "[%02d:%02d:%02d]", now.hour(), now.minute(), now.second());
  return String(buf);
}
void logInfo(const String& m)  { Serial.println(getTimestamp() + " [INFO]  " + m); }
void logWarn(const String& m)  { Serial.println(getTimestamp() + " [WARN]  " + m); }
void logError(const String& m) { Serial.println(getTimestamp() + " [ERROR] " + m); }

// ============================================================
//  LITTLEFS
// ============================================================
bool initFS() {
  if (!LittleFS.begin()) {
    logError("LittleFS mount GAGAL!");
    return false;
  }
  logInfo("LittleFS OK");
  return true;
}

// ============================================================
//  WIFI CONFIG -- LOAD/SAVE
// ============================================================
bool loadWifiConfig() {
  if (!LittleFS.exists(WIFI_CONFIG_FILE)) {
    logWarn("Tidak ada wifi_config.json");
    return false;
  }

  File f = LittleFS.open(WIFI_CONFIG_FILE, "r");
  if (!f) return false;

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    logError("wifi_config.json parse error: " + String(err.c_str()));
    return false;
  }

  wifiSsid     = doc["ssid"].as<String>();
  wifiPassword = doc["password"].as<String>();

  if (wifiSsid.length() == 0) {
    logWarn("SSID kosong di wifi_config.json");
    return false;
  }

  logInfo("WiFi config dimuat: SSID=" + wifiSsid);
  return true;
}

bool saveWifiConfig(const String& ssid, const String& password) {
  StaticJsonDocument<256> doc;
  doc["ssid"]     = ssid;
  doc["password"] = password;

  File f = LittleFS.open(WIFI_CONFIG_FILE, "w");
  if (!f) {
    logError("Gagal simpan wifi_config.json");
    return false;
  }
  serializeJson(doc, f);
  f.close();
  logInfo("WiFi config tersimpan: SSID=" + ssid);
  return true;
}

void deleteWifiConfig() {
  if (LittleFS.exists(WIFI_CONFIG_FILE)) {
    LittleFS.remove(WIFI_CONFIG_FILE);
    logInfo("wifi_config.json dihapus");
  }
  wifiSsid     = "";
  wifiPassword = "";
  wifiConfigExists = false;
}

// ============================================================
//  AP MODE -- HTTP SERVER
// ============================================================
void handleApRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Cat Feeder Setup</title>";
  html += "<style>body{font-family:Arial;max-width:400px;margin:auto;padding:20px;background:#f0f0f0}";
  html += "h2{color:#333}input{width:100%;padding:8px;margin:8px 0;box-sizing:border-box}";
  html += "button{width:100%;padding:10px;background:#4CAF50;color:white;border:none;cursor:pointer;font-size:16px}";
  html += "button:hover{background:#45a049}.info{background:#fff;padding:10px;border-radius:5px;margin:10px 0}</style>";
  html += "</head><body>";
  html += "<h2>Cat Feeder WiFi Setup</h2>";
  html += "<div class='info'>Masukkan SSID dan password WiFi Rumah B</div>";
  html += "<form method='POST' action='/wifi'>";
  html += "<label>SSID WiFi:</label><input name='ssid' placeholder='Nama WiFi' required>";
  html += "<label>Password:</label><input type='password' name='password' placeholder='Password WiFi'>";
  html += "<button type='submit'>Simpan & Konek</button>";
  html += "</form>";
  html += "<br><a href='/status'>Cek Status Koneksi</a>";
  html += "</body></html>";
  apServer.send(200, "text/html", html);
}

void handleApWifi() {
  if (apServer.method() != HTTP_POST) {
    apServer.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  String ssid     = apServer.arg("ssid");
  String password = apServer.arg("password");

  ssid.trim();
  password.trim();

  if (ssid.length() == 0) {
    apServer.send(400, "text/html",
      "<h2>Error: SSID tidak boleh kosong</h2><a href='/'>Kembali</a>");
    return;
  }

  logInfo("AP Mode: Menerima konfigurasi WiFi baru");
  logInfo("  SSID: " + ssid);

  // Simpan ke LittleFS
  saveWifiConfig(ssid, password);

  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'></head><body>";
  html += "<h2>Konfigurasi WiFi Tersimpan!</h2>";
  html += "<p>SSID: <strong>" + ssid + "</strong></p>";
  html += "<p>ESP8266 akan restart dan mencoba konek ke WiFi tersebut.</p>";
  html += "<p><strong>Sambungkan kembali HP ke WiFi biasa kamu.</strong></p>";
  html += "<p>Jika berhasil, LED akan berhenti berkedip dan ESP8266 akan muncul online di aplikasi.</p>";
  html += "</body></html>";

  apServer.send(200, "text/html", html);

  logInfo("AP Mode: Config tersimpan, restart dalam 2 detik...");
  apSetupDone = true;
}

void handleApStatus() {
  StaticJsonDocument<128> doc;
  doc["mode"]     = apModeActive ? "AP" : "STA";
  doc["ap_ssid"]  = AP_SSID;
  doc["connected"]= (WiFi.status() == WL_CONNECTED);
  doc["ip"]       = WiFi.localIP().toString();

  String json;
  serializeJson(doc, json);
  apServer.send(200, "application/json", json);
}

void handleApReset() {
  deleteWifiConfig();
  apServer.send(200, "text/html",
    "<h2>Konfigurasi WiFi direset!</h2><p>Refresh dan setup ulang.</p><a href='/'>Setup WiFi</a>");
  logInfo("AP Mode: WiFi config direset via HTTP");
}

void handleApNotFound() {
  apServer.send(404, "text/plain", "Not Found");
}

// ============================================================
//  START AP MODE
// ============================================================
void startAPMode() {
  logInfo("Masuk AP Mode...");
  logInfo("SSID    : " + String(AP_SSID));
  logInfo("Password: " + String(AP_PASSWORD));
  logInfo("IP      : " + String(AP_IP_STR));

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  IPAddress apIP = WiFi.softAPIP();
  char ipBuf[20];
  snprintf(ipBuf, sizeof(ipBuf), "%d.%d.%d.%d", apIP[0], apIP[1], apIP[2], apIP[3]);
  logInfo("AP IP   : " + String(ipBuf));

  // Daftarkan routes
  apServer.on("/",       HTTP_GET,  handleApRoot);
  apServer.on("/wifi",   HTTP_GET,  handleApRoot);
  apServer.on("/wifi",   HTTP_POST, handleApWifi);
  apServer.on("/status", HTTP_GET,  handleApStatus);
  apServer.on("/reset",  HTTP_GET,  handleApReset);
  apServer.onNotFound(handleApNotFound);
  apServer.begin();

  apModeActive = true;
  wifiMode     = WIFI_STATE_AP;

  logInfo("HTTP Server AP Mode siap");
  logInfo("Buka browser HP: http://" + String(AP_IP_STR));
}

// ============================================================
//  STOP AP MODE
// ============================================================
void stopAPMode() {
  apServer.stop();
  WiFi.softAPdisconnect(true);
  apModeActive = false;
  logInfo("AP Mode dinonaktifkan");
}

// ============================================================
//  START WIFI CONNECT (STA Mode)
// ============================================================
void startWifiConnect() {
  logInfo("Menghubungkan ke WiFi: " + wifiSsid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  wifiConnectStart = millis();
  wifiMode         = WIFI_STATE_CONNECTING;
}

// ============================================================
//  UPDATE WIFI STATE MACHINE (non-blocking)
// ============================================================
void updateWifi() {
  unsigned long now = millis();

  switch (wifiMode) {

    case WIFI_STATE_CONNECTING: {
      if (WiFi.status() == WL_CONNECTED) {
        wifiMode = WIFI_STATE_CONNECTED;
        logInfo("WiFi connected!");
        logInfo("IP Address: " + WiFi.localIP().toString());
        logInfo("SSID      : " + WiFi.SSID());
        logInfo("RSSI      : " + String(WiFi.RSSI()) + " dBm");
      } else if (now - wifiConnectStart > WIFI_CONNECT_TIMEOUT) {
        logWarn("WiFi timeout! SSID=" + wifiSsid);
        logWarn("Retry dalam " + String(WIFI_RETRY_INTERVAL / 1000) + " detik...");
        WiFi.disconnect();
        wifiRetryStart = now;
        wifiMode       = WIFI_STATE_RETRY;
      }
      break;
    }

    case WIFI_STATE_CONNECTED: {
      if (WiFi.status() != WL_CONNECTED) {
        logWarn("WiFi terputus! Mencoba reconnect...");
        wifiMode = WIFI_STATE_CONNECTING;
        WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
        wifiConnectStart = now;
      }
      break;
    }

    case WIFI_STATE_RETRY: {
      if (now - wifiRetryStart >= WIFI_RETRY_INTERVAL) {
        logInfo("Retry WiFi connect...");
        startWifiConnect();
      }
      break;
    }

    case WIFI_STATE_AP: {
      // AP Mode: handle HTTP requests
      apServer.handleClient();

      // Jika setup sudah selesai, restart
      if (apSetupDone) {
        delay(1000);
        logInfo("Restart ESP8266...");
        delay(500);
        ESP.restart();
      }
      break;
    }
  }
}

// ============================================================
//  INISIALISASI RTC
// ============================================================
bool initRTC() {
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!rtc.begin()) {
    logError("DS3231 tidak terdeteksi!");
    return false;
  }
  logInfo("DS3231 OK -- 0x68");
  if (rtc.lostPower()) {
    logWarn("RTC lostPower! Set ke waktu compile.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  rtc.disable32K();
  rtc.clearAlarm(1); rtc.clearAlarm(2);
  rtc.disableAlarm(1); rtc.disableAlarm(2);
  return true;
}

// ============================================================
//  JADWAL
// ============================================================
void setDefaultSchedule() {
  schedules[0] = {true, 7, 0};
  schedules[1] = {true, 18, 0};
}

bool loadSchedule() {
  if (!LittleFS.exists(SCHEDULE_FILE)) {
    setDefaultSchedule();
    return true;
  }
  File f = LittleFS.open(SCHEDULE_FILE, "r");
  if (!f) { setDefaultSchedule(); return false; }

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, f) || !doc.containsKey("schedule1")) {
    f.close(); setDefaultSchedule(); return false;
  }
  f.close();

  schedules[0] = {doc["schedule1"]["enabled"] | true,
                  doc["schedule1"]["hour"]    | 7,
                  doc["schedule1"]["minute"]  | 0};
  schedules[1] = {doc["schedule2"]["enabled"] | true,
                  doc["schedule2"]["hour"]    | 18,
                  doc["schedule2"]["minute"]  | 0};
  return true;
}

bool saveSchedule() {
  StaticJsonDocument<256> doc;
  JsonObject s1 = doc.createNestedObject("schedule1");
  s1["enabled"] = schedules[0].enabled;
  s1["hour"]    = schedules[0].hour;
  s1["minute"]  = schedules[0].minute;
  JsonObject s2 = doc.createNestedObject("schedule2");
  s2["enabled"] = schedules[1].enabled;
  s2["hour"]    = schedules[1].hour;
  s2["minute"]  = schedules[1].minute;

  File f = LittleFS.open(SCHEDULE_FILE, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

// ============================================================
//  SERVO
// ============================================================
void servoMoveTo(int angle) { feederServo.write(constrain(angle, 0, 180)); }
void servoOpen()  { logInfo("Servo OPEN  --> " + String(SERVO_OPEN)  + " deg"); servoMoveTo(SERVO_OPEN); }
void servoClose() { logInfo("Servo CLOSE --> " + String(SERVO_CLOSE) + " deg"); servoMoveTo(SERVO_CLOSE); }

// ============================================================
//  FEEDING STATE MACHINE
// ============================================================
void startFeeding(int slot) {
  if (feedingInProgress) { logWarn("Feeding sedang berjalan!"); return; }
  if (!rtcReady) { logError("RTC tidak siap! Feeding dibatalkan."); return; }
  logInfo("=== FEEDING DIMULAI === slot=" + String(slot < 0 ? 0 : slot + 1));
  feedingInProgress = true;
  feedTriggerSlot   = slot;
  feedState         = FEED_OPENING;
  feedTimer         = millis();
}

void updateFeedStateMachine() {
  if (feedState == FEED_IDLE) return;
  unsigned long now = millis();
  switch (feedState) {
    case FEED_OPENING:
      servoOpen();
      feedTimer = now;
      feedState = FEED_OPEN_WAIT;
      break;
    case FEED_OPEN_WAIT:
      if (now - feedTimer >= (unsigned long)FEED_DURATION) feedState = FEED_CLOSING;
      break;
    case FEED_CLOSING:
      servoClose();
      feedTimer = now;
      feedState = FEED_DONE;
      break;
    case FEED_DONE:
      if (now - feedTimer >= (unsigned long)SERVO_SETTLE) {
        logInfo("=== FEEDING SELESAI ===");
        feedingInProgress = false;
        feedState = FEED_IDLE;
        feedTriggerSlot = -1;
      }
      break;
    default:
      feedState = FEED_IDLE;
      feedingInProgress = false;
  }
}

// ============================================================
//  CEK JADWAL
// ============================================================
void checkSchedule() {
  if (!rtcReady || feedingInProgress) return;
  DateTime now = rtc.now();
  char today[11];
  snprintf(today, sizeof(today), "%04d-%02d-%02d", now.year(), now.month(), now.day());

  for (int i = 0; i < MAX_SCHEDULES; i++) {
    if (!schedules[i].enabled) continue;
    if (now.hour()   != schedules[i].hour)   continue;
    if (now.minute() != schedules[i].minute) continue;
    if (now.second() > 4) continue;
    if (strcmp(lastFeedDate, today) == 0 && lastFeedSlot == i) continue;

    char buf[60];
    snprintf(buf, sizeof(buf), "Jadwal %d terpicu: %02d:%02d",
      i + 1, schedules[i].hour, schedules[i].minute);
    logInfo(buf);

    strcpy(lastFeedDate, today);
    lastFeedSlot = i;
    startFeeding(i);
    break;
  }
}

// ============================================================
//  TAMPILKAN STATUS
// ============================================================
void printStatus() {
  Serial.println();
  Serial.println("=== STATUS ===");

  // WiFi
  Serial.print("  WiFi Mode : ");
  switch (wifiMode) {
    case WIFI_STATE_CONNECTING: Serial.println("CONNECTING..."); break;
    case WIFI_STATE_CONNECTED:  Serial.println("CONNECTED"); break;
    case WIFI_STATE_AP:         Serial.println("AP MODE (setup)"); break;
    case WIFI_STATE_RETRY:      Serial.println("RETRY (waiting)"); break;
  }

  if (wifiMode == WIFI_STATE_CONNECTED) {
    Serial.println("  SSID      : " + WiFi.SSID());
    Serial.println("  IP        : " + WiFi.localIP().toString());
    Serial.println("  RSSI      : " + String(WiFi.RSSI()) + " dBm");
  } else if (wifiMode == WIFI_STATE_AP) {
    Serial.println("  AP SSID   : " + String(AP_SSID));
    Serial.println("  AP IP     : " + String(AP_IP_STR));
    Serial.println("  Setup URL : http://" + String(AP_IP_STR));
  }

  // RTC
  if (rtcReady) {
    DateTime now = rtc.now();
    char tbuf[40];
    snprintf(tbuf, sizeof(tbuf), "%04d-%02d-%02d %02d:%02d:%02d",
      now.year(), now.month(), now.day(),
      now.hour(), now.minute(), now.second());
    Serial.println("  RTC       : " + String(tbuf));
  } else {
    Serial.println("  RTC       : ERROR");
  }

  // Jadwal
  for (int i = 0; i < MAX_SCHEDULES; i++) {
    char buf[50];
    snprintf(buf, sizeof(buf), "  Jadwal %d  : %s %02d:%02d",
      i + 1,
      schedules[i].enabled ? "[ON] " : "[OFF]",
      schedules[i].hour, schedules[i].minute);
    Serial.println(buf);
  }

  Serial.print("  Feeding   : ");
  Serial.println(feedingInProgress ? "RUNNING" : "IDLE");
  Serial.println("==============");
  Serial.println();
}

// ============================================================
//  WIZARD SET JADWAL
// ============================================================
void handleWizard(const String& input) {
  switch (wizardStep) {
    case 1:
      wizardSlot = input.toInt() - 1;
      if (wizardSlot < 0 || wizardSlot >= MAX_SCHEDULES) {
        logError("Slot tidak valid"); wizardStep = 0;
      } else { Serial.println("Jam (0-23):"); wizardStep = 2; }
      break;
    case 2: {
      int h = input.toInt();
      if (h < 0 || h > 23) { logError("Jam tidak valid"); wizardStep = 0; }
      else { schedules[wizardSlot].hour = h; Serial.println("Menit (0-59):"); wizardStep = 3; }
      break;
    }
    case 3: {
      int m = input.toInt();
      if (m < 0 || m > 59) { logError("Menit tidak valid"); wizardStep = 0; }
      else { schedules[wizardSlot].minute = m; Serial.println("Aktif? (1=ON, 0=OFF):"); wizardStep = 4; }
      break;
    }
    case 4: {
      schedules[wizardSlot].enabled = (input.toInt() == 1);
      char buf[60];
      snprintf(buf, sizeof(buf), "Jadwal %d: %s %02d:%02d",
        wizardSlot + 1,
        schedules[wizardSlot].enabled ? "[ON]" : "[OFF]",
        schedules[wizardSlot].hour, schedules[wizardSlot].minute);
      logInfo(buf);
      saveSchedule();
      wizardStep = 0;
      break;
    }
  }
}

// ============================================================
//  MENU
// ============================================================
void printMenu() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("  SMART CAT FEEDER -- STEP 4 MENU");
  Serial.println("========================================");
  Serial.println("  d   --> Status (WiFi + RTC + jadwal)");
  Serial.println("  r   --> Tampilkan jadwal");
  Serial.println("  w   --> Set jadwal (wizard)");
  Serial.println("  f   --> Force feeding sekarang");
  Serial.println("  R   --> Reset WiFi config (masuk AP Mode)");
  Serial.println("  W   --> Info WiFi detail");
  Serial.println("  m   --> Menu ini");
  Serial.println("========================================");
  Serial.println();
}

// ============================================================
//  HANDLE SERIAL
// ============================================================
void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      serialBuffer.trim();
      if (serialBuffer.length() == 0) { serialBuffer = ""; return; }

      if (wizardStep > 0) { handleWizard(serialBuffer); serialBuffer = ""; return; }

      if      (serialBuffer == "d") { printStatus(); }
      else if (serialBuffer == "r") {
        Serial.println();
        for (int i = 0; i < MAX_SCHEDULES; i++) {
          char buf[50];
          snprintf(buf, sizeof(buf), "  Jadwal %d: %s %02d:%02d",
            i + 1, schedules[i].enabled ? "[ON] " : "[OFF]",
            schedules[i].hour, schedules[i].minute);
          Serial.println(buf);
        }
        Serial.println();
      }
      else if (serialBuffer == "w") { Serial.println("Set jadwal slot (1/2):"); wizardStep = 1; }
      else if (serialBuffer == "f") { logInfo("Force feeding manual"); startFeeding(-1); }
      else if (serialBuffer == "R") {
        logWarn("Reset WiFi config! ESP8266 akan masuk AP Mode...");
        deleteWifiConfig();
        delay(500);
        ESP.restart();
      }
      else if (serialBuffer == "W") {
        Serial.println();
        Serial.println("=== WIFI INFO ===");
        Serial.println("  Status : " + String(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED"));
        Serial.println("  SSID   : " + WiFi.SSID());
        Serial.println("  IP     : " + WiFi.localIP().toString());
        Serial.println("  RSSI   : " + String(WiFi.RSSI()) + " dBm");
        Serial.println("  MAC    : " + WiFi.macAddress());
        Serial.println("=================");
        Serial.println();
      }
      else if (serialBuffer == "m") { printMenu(); }
      else { logError("Perintah tidak dikenal: " + serialBuffer); printMenu(); }

      serialBuffer = "";
    } else {
      if (c != '\r') serialBuffer += c;
    }
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(9600);
  delay(500);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  SMART CAT FEEDER");
  Serial.println("  STEP 4: WiFi + AP Mode Provisioning");
  Serial.println("  Build: " __DATE__ " " __TIME__);
  Serial.println("========================================");

  // RTC
  rtcReady = initRTC();

  // LittleFS
  fsReady = initFS();

  // Load jadwal
  if (fsReady) loadSchedule();
  else setDefaultSchedule();
  logInfo("Jadwal dimuat: " + String(schedules[0].hour) + ":00 dan " + String(schedules[1].hour) + ":00");

  // Servo
  feederServo.attach(SERVO_PIN, 500, 2500); // 500-2500µs agar 0-180° akurat
  logInfo("Servo attached GPIO" + String(SERVO_PIN));
  servoClose();
  delay(300);

  // Load WiFi config
  if (fsReady) {
    wifiConfigExists = loadWifiConfig();
  }

  // Tentukan mode: STA atau AP
  if (wifiConfigExists && wifiSsid.length() > 0) {
    logInfo("Konfigurasi WiFi ditemukan --> mode STA");
    startWifiConnect();
  } else {
    logInfo("Tidak ada konfigurasi WiFi --> masuk AP Mode");
    startAPMode();
    logInfo("Buka http://" + String(AP_IP_STR) + " untuk setup WiFi");
  }

  printStatus();
  printMenu();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  // Update WiFi state machine (non-blocking)
  updateWifi();

  // Cek jadwal setiap 1 detik
  unsigned long nowMs = millis();
  if (nowMs - lastCheckMs >= CHECK_INTERVAL) {
    lastCheckMs = nowMs;
    checkSchedule();
  }

  // Print status periodik setiap 10 detik
  if (nowMs - lastStatusMs >= STATUS_INTERVAL) {
    lastStatusMs = nowMs;
    if (wifiMode == WIFI_STATE_CONNECTING) {
      Serial.print(".");  // progress indicator
    }
  }

  // Update feeding state machine
  updateFeedStateMachine();

  // Proses input Serial
  handleSerial();

  yield();
}

/*
 * ============================================================
 *  LIBRARY YANG DIBUTUHKAN
 * ============================================================
 *  1. RTClib by Adafruit
 *  2. ArduinoJson by Benoit Blanchon (v6.x)
 *  3. LittleFS    --> built-in ESP8266 core
 *  4. Servo       --> built-in ESP8266 core
 *  5. ESP8266WiFi --> built-in ESP8266 core
 *  6. ESP8266WebServer --> built-in ESP8266 core
 * ============================================================
 *
 *  ALUR KERJA STEP 4
 * ============================================================
 *  1. ESP8266 boot
 *  2. Cek LittleFS ada wifi_config.json?
 *     YA  --> koneksi ke WiFi Rumah B (mode STA)
 *     TIDAK --> masuk AP Mode "CatFeeder-Setup"
 *
 *  Setup Awal (AP Mode):
 *  3. Konek HP ke WiFi "CatFeeder-Setup" (password: catfeeder123)
 *  4. Buka browser: http://192.168.4.1
 *  5. Isi SSID dan password WiFi Rumah B --> Submit
 *  6. ESP8266 simpan ke LittleFS --> restart --> konek WiFi
 *  7. Konek HP kembali ke WiFi biasa
 *
 *  Ganti WiFi (sudah online):
 *  - Ketik 'R' di Serial Monitor --> reset config --> AP Mode
 *
 *  CHECKLIST TEST STEP 4
 * ============================================================
 *  [TEST-11] Boot tanpa wifi_config.json:
 *            Serial: "masuk AP Mode"
 *            WiFi HP: "CatFeeder-Setup" terlihat
 *
 *  [TEST-12] Setup via browser http://192.168.4.1:
 *            Isi SSID + password --> Submit
 *            Serial: "WiFi config tersimpan"
 *            ESP8266 restart otomatis
 *
 *  [TEST-13] Setelah setup:
 *            Serial: "WiFi connected!"
 *            Serial: "IP Address: 192.168.x.x"
 *
 *  [TEST-14] Cabut/matikan router sementara:
 *            Serial: "WiFi terputus! Mencoba reconnect..."
 *            Jadwal tetap berjalan
 *            Saat router nyala lagi: "WiFi connected!" kembali
 *
 *  [TEST-15] Ketik 'R' --> WiFi config dihapus --> AP Mode aktif
 *
 *  Setelah semua TEST PASS --> Lanjut STEP 5: PHP REST API
 * ============================================================
 */
