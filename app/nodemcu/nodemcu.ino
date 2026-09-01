/*
 * ============================================================
 *  SMART CAT FEEDER -- STEP 6: Server Polling + Dynamic Features
 * ============================================================
 *  Fitur:
 *  1. Polling & Heartbeat Non-blocking
 *  2. Pengaturan Servo Dinamis (Sudut Buka, Sudut Tutup, Durasi Delay)
 *  3. Jadwal Dinamis (Mendukung hingga 6 slot jadwal otomatis)
 *  4. Sinkronisasi RTC DS3231
 *  5. Pengaturan WiFi & Auto Reconnect
 *  6. Offline Feed Queue & Sync
 * ============================================================
 */

#include <Wire.h>
#include <RTClib.h>
#include <Servo.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// ----- PIN -----
#define SDA_PIN    4
#define SCL_PIN    5
#define SERVO_PIN  14

// ----- SERVO PARAMS (DINAMIS DARI SERVER / LITTLEFS) -----
int servoCloseAngle = 0;     // Sudut tutup default (0 - 180)
int servoOpenAngle  = 90;    // Sudut buka default (0 - 180)
int feedDurationMs  = 2000;  // Durasi buka default (ms)
#define SERVO_SETTLE 400

// ----- FILE PATHS -----
#define WIFI_CONFIG_FILE   "/wifi_config.json"
#define SCHEDULE_FILE      "/schedule.json"
#define SERVO_CONFIG_FILE  "/servo_config.json"
#define FEEDLOG_FILE       "/feedlog.json"

// ============================================================
//  SERVER CONFIG (PRODUCTION: catfeeder.tamamici.my.id)
// ============================================================
#define SERVER_HOST      "catfeeder.tamamici.my.id"   // Domain VPS / aaPanel
#define SERVER_PORT      80                           // Port 80 (HTTP)
#define SERVER_PATH      "/api"                       // Endpoint API
#define DEVICE_ID        "CAT_FEEDER_01"
#define DEVICE_TOKEN     "catfeeder_secret_token_2026"

// ----- INTERVAL -----
#define HEARTBEAT_INTERVAL   10000   // ms (10 detik)
#define POLL_INTERVAL         5000   // ms (5 detik)
#define SYNC_LOG_INTERVAL   120000   // ms (2 menit)

// ----- JADWAL (Mendukung hingga 6 Slot Jadwal) -----
struct Schedule {
  bool enabled;
  int  hour;
  int  minute;
};
#define MAX_SCHEDULES 6
Schedule schedules[MAX_SCHEDULES];
int totalSchedules = 2;

// ----- ANTI-DUPLICATE -----
char lastFeedDate[11] = "";
int  lastFeedSlot     = -1;

// ----- OBJECTS -----
RTC_DS3231 rtc;
Servo feederServo;
ESP8266WebServer apServer(80);

// ----- STATE -----
bool rtcReady = false;
bool fsReady  = false;

enum AppWiFiState { WIFI_STATE_CONNECTING, WIFI_STATE_CONNECTED, WIFI_STATE_AP, WIFI_STATE_RETRY };
AppWiFiState wifiMode = WIFI_STATE_CONNECTING;
unsigned long wifiConnectStart = 0;
unsigned long wifiRetryStart   = 0;
String wifiSsid     = "";
String wifiPassword = "";
bool   wifiConfigExists = false;

// AP Mode Setup
#define AP_SSID      "CatFeeder-Setup"
#define AP_PASSWORD  ""   // Open AP (tanpa password agar langsung terdeteksi & mudah disambungkan)
#define AP_IP_STR    "192.168.4.1"
#define WIFI_CONNECT_TIMEOUT 15000   // 15 detik timeout sebelum fallback ke AP Mode
#define WIFI_RETRY_INTERVAL  30000
bool apModeActive = false;
bool apSetupDone  = false;

// Servo state machine
enum FeedState { FEED_IDLE, FEED_OPENING, FEED_OPEN_WAIT, FEED_CLOSING, FEED_DONE };
FeedState feedState = FEED_IDLE;
unsigned long feedTimer = 0;
bool feedingInProgress  = false;
int  feedTriggerSlot    = -1;
int  lastCommandId      = -1;

// Serial & Timing
String serialBuffer = "";
unsigned long lastCheckMs     = 0;
unsigned long lastHeartbeatMs = 0;
unsigned long lastPollMs      = 0;
unsigned long lastSyncLogMs   = 0;
const unsigned long CHECK_INTERVAL = 1000;

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
//  HTTP HELPER
// ============================================================
String buildUrl(const String& endpoint) {
  return "http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT) + String(SERVER_PATH) + endpoint;
}

int httpPost(const String& endpoint, const String& jsonBody, String& responseBody) {
  if (wifiMode != WIFI_STATE_CONNECTED) return -1;

  String url = buildUrl(endpoint);
  WiFiClient client;
  HTTPClient httpClient;

  if (!httpClient.begin(client, url)) return -1;

  httpClient.addHeader("Content-Type", "application/json");
  httpClient.addHeader("X-Device-ID",    DEVICE_ID);
  httpClient.addHeader("X-Device-Token", DEVICE_TOKEN);
  httpClient.setTimeout(5000);

  int code = httpClient.POST(jsonBody);
  if (code > 0) {
    responseBody = httpClient.getString();
  } else {
    responseBody = "";
    logError("HTTP POST gagal (" + String(code) + "): " + httpClient.errorToString(code) + " -> " + url);
  }
  httpClient.end();
  client.stop();
  return code;
}

int httpGet(const String& endpoint, String& responseBody) {
  if (wifiMode != WIFI_STATE_CONNECTED) return -1;

  String url = buildUrl(endpoint);
  WiFiClient client;
  HTTPClient httpClient;

  if (!httpClient.begin(client, url)) return -1;

  httpClient.addHeader("X-Device-ID",    DEVICE_ID);
  httpClient.addHeader("X-Device-Token", DEVICE_TOKEN);
  httpClient.setTimeout(5000);

  int code = httpClient.GET();
  if (code > 0) {
    responseBody = httpClient.getString();
  } else {
    responseBody = "";
    logError("HTTP GET gagal (" + String(code) + "): " + httpClient.errorToString(code) + " -> " + url);
  }
  httpClient.end();
  client.stop();
  return code;
}

// ============================================================
//  PENGATURAN SERVO (LITTLEFS)
// ============================================================
bool loadServoConfig() {
  if (!LittleFS.exists(SERVO_CONFIG_FILE)) {
    servoCloseAngle = 0;
    servoOpenAngle  = 90;
    feedDurationMs  = 2000;
    return true;
  }
  File f = LittleFS.open(SERVO_CONFIG_FILE, "r");
  if (!f) return false;
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, f)) { f.close(); return false; }
  f.close();

  servoCloseAngle = doc["close_angle"] | 0;
  servoOpenAngle  = doc["open_angle"]  | 90;
  feedDurationMs  = doc["duration_ms"] | 2000;

  char buf[80];
  snprintf(buf, sizeof(buf), "Servo config dimuat: Close=%d deg, Open=%d deg, Duration=%d ms",
    servoCloseAngle, servoOpenAngle, feedDurationMs);
  logInfo(buf);
  return true;
}

bool saveServoConfig() {
  StaticJsonDocument<128> doc;
  doc["close_angle"] = servoCloseAngle;
  doc["open_angle"]  = servoOpenAngle;
  doc["duration_ms"] = feedDurationMs;
  File f = LittleFS.open(SERVO_CONFIG_FILE, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

// ============================================================
//  HEARTBEAT & LAPOR STATUS
// ============================================================
void sendHeartbeat() {
  String body = "{}";
  if (rtcReady) {
    DateTime now = rtc.now();
    char buf[25];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
      now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    body = "{\"rtc\":\"" + String(buf) + "\"}";
  }

  String resp;
  int code = httpPost("/heartbeat.php", body, resp);
  if (code == 200) {
    logInfo("Heartbeat OK (200)");
  } else {
    logWarn("Heartbeat gagal: HTTP " + String(code));
  }
}

void reportCommandResult(int commandId, const String& status, const String& message = "") {
  if (commandId < 0) return;

  StaticJsonDocument<128> doc;
  doc["command_id"] = commandId;
  doc["status"]     = status;
  if (!message.isEmpty()) doc["message"] = message;

  String body;
  serializeJson(doc, body);

  String resp;
  httpPost("/command_result.php", body, resp);
}

// ============================================================
//  JADWAL (LITTLEFS)
// ============================================================
bool saveSchedule() {
  StaticJsonDocument<512> doc;
  JsonArray arr = doc.createNestedArray("schedules");
  for (int i = 0; i < totalSchedules; i++) {
    JsonObject s = arr.createNestedObject();
    s["slot"]    = i + 1;
    s["enabled"] = schedules[i].enabled;
    s["hour"]    = schedules[i].hour;
    s["minute"]  = schedules[i].minute;
  }
  File f = LittleFS.open(SCHEDULE_FILE, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

bool loadSchedule() {
  for (int i = 0; i < MAX_SCHEDULES; i++) { schedules[i] = {false, 0, 0}; }
  totalSchedules = 2;
  schedules[0] = {true, 7, 0};
  schedules[1] = {true, 18, 0};

  if (!LittleFS.exists(SCHEDULE_FILE)) return true;

  File f = LittleFS.open(SCHEDULE_FILE, "r");
  if (!f) return false;

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, f)) { f.close(); return false; }
  f.close();

  if (doc.containsKey("schedules")) {
    JsonArray arr = doc["schedules"].as<JsonArray>();
    totalSchedules = min((int)arr.size(), MAX_SCHEDULES);
    for (int i = 0; i < totalSchedules; i++) {
      schedules[i].enabled = arr[i]["enabled"] | true;
      schedules[i].hour    = arr[i]["hour"]    | 0;
      schedules[i].minute  = arr[i]["minute"]  | 0;
    }
  }
  return true;
}

// ============================================================
//  OFFLINE FEED LOG & SYNC
// ============================================================
void appendOfflineFeedLog(const String& type, int slot, const String& status) {
  if (!fsReady || !rtcReady) return;
  DateTime now = rtc.now();
  char timestamp[20];
  snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
    now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());

  StaticJsonDocument<2048> doc;
  JsonArray logs;
  if (LittleFS.exists(FEEDLOG_FILE)) {
    File rf = LittleFS.open(FEEDLOG_FILE, "r");
    if (rf) {
      if (deserializeJson(doc, rf) == DeserializationError::Ok && doc.containsKey("logs")) {
        logs = doc["logs"].as<JsonArray>();
      }
      rf.close();
    }
  }
  if (logs.isNull()) logs = doc.createNestedArray("logs");

  JsonObject entry = logs.createNestedObject();
  entry["timestamp"] = timestamp;
  entry["type"]      = type;
  entry["slot"]      = slot;
  entry["status"]    = status;
  entry["synced"]    = false;

  while (logs.size() > 50) logs.remove(0);

  File wf = LittleFS.open(FEEDLOG_FILE, "w");
  if (wf) { serializeJson(doc, wf); wf.close(); }
}

void syncOfflineLog() {
  if (!fsReady || wifiMode != WIFI_STATE_CONNECTED) return;
  if (!LittleFS.exists(FEEDLOG_FILE)) return;

  File f = LittleFS.open(FEEDLOG_FILE, "r");
  if (!f) return;

  StaticJsonDocument<2048> doc;
  if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return; }
  f.close();

  JsonArray logs = doc["logs"].as<JsonArray>();
  if (logs.isNull() || logs.size() == 0) return;

  StaticJsonDocument<2048> uploadDoc;
  JsonArray uploadLogs = uploadDoc.createNestedArray("logs");
  bool hasPending = false;

  for (JsonObject log : logs) {
    if (!log["synced"].as<bool>()) {
      uploadLogs.add(log);
      hasPending = true;
    }
  }
  if (!hasPending) return;

  String body;
  serializeJson(uploadDoc, body);
  String resp;
  int code = httpPost("/device_status.php?action=upload-log", body, resp);

  if (code == 200) {
    logInfo("Offline log sync OK: " + String(uploadLogs.size()) + " entries");
    for (JsonObject log : logs) { log["synced"] = true; }
    File wf = LittleFS.open(FEEDLOG_FILE, "w");
    if (wf) { serializeJson(doc, wf); wf.close(); }
  }
}

// ============================================================
//  SERVO ACTION
// ============================================================
void servoMoveTo(int angle) { feederServo.write(constrain(angle, 0, 180)); }
void servoOpen()  { logInfo("Servo OPEN  --> " + String(servoOpenAngle)  + " deg"); servoMoveTo(servoOpenAngle); }
void servoClose() { logInfo("Servo CLOSE --> " + String(servoCloseAngle) + " deg"); servoMoveTo(servoCloseAngle); }

void startFeeding(int slot) {
  if (feedingInProgress) { logWarn("Feeding sedang berjalan!"); return; }
  if (!rtcReady) { logError("RTC tidak siap!"); return; }

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
      logInfo("Pakan keluar... tunggu " + String(feedDurationMs) + "ms");
      break;
    case FEED_OPEN_WAIT:
      if (now - feedTimer >= (unsigned long)feedDurationMs) feedState = FEED_CLOSING;
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

        String type = (feedTriggerSlot == -1 || feedTriggerSlot == -2) ? "manual" : "schedule";
        int slot = (feedTriggerSlot >= 0) ? feedTriggerSlot + 1 : 0;
        appendOfflineFeedLog(type, slot, "success");

        if (feedTriggerSlot == -2 && lastCommandId > 0) {
          reportCommandResult(lastCommandId, "success");
          lastCommandId = -1;
        }

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
//  CEK JADWAL OTOMATIS (DS3231)
// ============================================================
void checkSchedule() {
  if (!rtcReady || feedingInProgress) return;
  DateTime now = rtc.now();
  char today[11];
  snprintf(today, sizeof(today), "%04d-%02d-%02d", now.year(), now.month(), now.day());

  for (int i = 0; i < totalSchedules; i++) {
    if (!schedules[i].enabled) continue;
    if (now.hour()   != schedules[i].hour)   continue;
    if (now.minute() != schedules[i].minute) continue;
    if (now.second() > 4) continue;
    if (strcmp(lastFeedDate, today) == 0 && lastFeedSlot == i) continue;

    char buf[60];
    snprintf(buf, sizeof(buf), "Jadwal %d terpicu: %02d:%02d", i+1, schedules[i].hour, schedules[i].minute);
    logInfo(buf);

    strcpy(lastFeedDate, today);
    lastFeedSlot = i;
    startFeeding(i);
    break;
  }
}

// ============================================================
//  EKSEKUSI COMMAND DARI SERVER
// ============================================================
void executeCommand(int commandId, const String& command, const JsonObject& payload) {
  logInfo("Eksekusi command: " + command + " (id=" + String(commandId) + ")");

  // ---- 1. FEED ----
  if (command == "FEED") {
    if (feedingInProgress) {
      reportCommandResult(commandId, "failed", "feeding_in_progress");
    } else {
      lastCommandId = commandId;
      startFeeding(-2);
    }
    return;
  }

  // ---- 2. SET_SERVO_CONFIG (Pengaturan Sudut & Delay) ----
  if (command == "SET_SERVO_CONFIG") {
    if (payload.containsKey("close_angle")) servoCloseAngle = payload["close_angle"].as<int>();
    if (payload.containsKey("open_angle"))  servoOpenAngle  = payload["open_angle"].as<int>();
    if (payload.containsKey("duration_ms")) feedDurationMs  = payload["duration_ms"].as<int>();

    servoCloseAngle = constrain(servoCloseAngle, 0, 180);
    servoOpenAngle  = constrain(servoOpenAngle, 0, 180);
    feedDurationMs  = constrain(feedDurationMs, 500, 10000);

    saveServoConfig();
    servoClose(); // Pindahkan ke posisi tutup baru

    char buf[90];
    snprintf(buf, sizeof(buf), "Servo diatur: Close=%d deg, Open=%d deg, Delay=%d ms",
      servoCloseAngle, servoOpenAngle, feedDurationMs);
    logInfo(buf);

    reportCommandResult(commandId, "success");
    return;
  }

  // ---- 3. SET_SCHEDULE (Jadwal Dinamis) ----
  if (command == "SET_SCHEDULE") {
    if (payload.containsKey("schedules")) {
      JsonArray arr = payload["schedules"].as<JsonArray>();
      totalSchedules = min((int)arr.size(), MAX_SCHEDULES);
      for (int i = 0; i < totalSchedules; i++) {
        schedules[i].enabled = arr[i]["enabled"] | true;
        schedules[i].hour    = arr[i]["hour"]    | 0;
        schedules[i].minute  = arr[i]["minute"]  | 0;
      }
      saveSchedule();
      logInfo("Jadwal dinamis (" + String(totalSchedules) + " slot) berhasil disimpan ke LittleFS");
      reportCommandResult(commandId, "success");
      return;
    }

    // Fallback: ambil via GET jika payload tidak ada
    String resp;
    int code = httpGet("/schedule.php", resp);
    if (code == 200) {
      StaticJsonDocument<1024> doc;
      if (!deserializeJson(doc, resp) && doc.containsKey("data")) {
        JsonObject schedObj = doc["data"]["schedules"];
        int idx = 0;
        for (JsonPair kv : schedObj) {
          if (idx >= MAX_SCHEDULES) break;
          JsonObject s = kv.value().as<JsonObject>();
          schedules[idx].enabled = s["enabled"] | true;
          schedules[idx].hour    = s["hour"]    | 0;
          schedules[idx].minute  = s["minute"]  | 0;
          idx++;
        }
        totalSchedules = idx;
        saveSchedule();
        reportCommandResult(commandId, "success");
        return;
      }
    }
    reportCommandResult(commandId, "failed", "fetch_schedule_failed");
    return;
  }

  // ---- 4. SET_RTC ----
  if (command == "SET_RTC") {
    if (!rtcReady) { reportCommandResult(commandId, "failed", "rtc_not_ready"); return; }
    String dt = payload["datetime"] | "";
    if (dt.length() >= 19) {
      int yr = dt.substring(0,4).toInt();
      int mo = dt.substring(5,7).toInt();
      int dy = dt.substring(8,10).toInt();
      int hr = dt.substring(11,13).toInt();
      int mn = dt.substring(14,16).toInt();
      int sc = dt.substring(17,19).toInt();
      if (yr >= 2020 && mo >= 1 && mo <= 12) {
        rtc.adjust(DateTime(yr, mo, dy, hr, mn, sc));
        logInfo("RTC diselaraskan: " + dt);
        reportCommandResult(commandId, "success");
        return;
      }
    }
    reportCommandResult(commandId, "failed", "invalid_datetime");
    return;
  }

  // ---- 5. SET_WIFI ----
  if (command == "SET_WIFI") {
    String newSsid = payload["ssid"] | "";
    String newPass = payload["password"] | "";
    if (newSsid.length() > 0) {
      StaticJsonDocument<128> cfg;
      cfg["ssid"] = newSsid; cfg["password"] = newPass;
      File f = LittleFS.open(WIFI_CONFIG_FILE, "w");
      if (f) { serializeJson(cfg, f); f.close(); }
      reportCommandResult(commandId, "success");
      delay(1000);
      ESP.restart();
      return;
    }
  }

  reportCommandResult(commandId, "failed", "unknown_command");
}

void pollCommand() {
  if (wifiMode != WIFI_STATE_CONNECTED || feedingInProgress) return;

  String resp;
  int code = httpGet("/command_poll.php", resp);
  if (code != 200) return;

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, resp) || !doc["success"].as<bool>()) return;

  JsonObject data = doc["data"];
  if (data["command"].isNull()) return;

  int    commandId = data["command_id"] | -1;
  String command   = data["command"]   | "";
  JsonObject payload = data["payload"].as<JsonObject>();

  if (commandId > 0 && !command.isEmpty()) {
    executeCommand(commandId, command, payload);
  }
}

// ============================================================
//  WIFI CONFIG & AP MODE
// ============================================================
bool loadWifiConfig() {
  if (!LittleFS.exists(WIFI_CONFIG_FILE)) return false;
  File f = LittleFS.open(WIFI_CONFIG_FILE, "r");
  if (!f) return false;
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, f)) { f.close(); return false; }
  f.close();
  wifiSsid     = doc["ssid"].as<String>();
  wifiPassword = doc["password"].as<String>();
  return wifiSsid.length() > 0;
}

void deleteWifiConfig() {
  if (LittleFS.exists(WIFI_CONFIG_FILE)) LittleFS.remove(WIFI_CONFIG_FILE);
  wifiSsid = ""; wifiPassword = "";
}

void handleApRoot() {
  String rtcStr = "--";
  if (rtcReady) {
    DateTime now = rtc.now();
    char buf[25];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
      now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    rtcStr = String(buf);
  }

  String staStatus = (WiFi.status() == WL_CONNECTED) 
    ? "<span style='color:#10B981;'>Terhubung (" + WiFi.localIP().toString() + ")</span>" 
    : "<span style='color:#EF4444;'>Menghubungkan / Belum Terhubung</span>";

  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Smart Cat Feeder Control</title>";
  html += "<style>";
  html += "body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;background:#0F172A;color:#F8FAFC;margin:0;padding:16px}";
  html += ".card{max-width:420px;margin:auto;background:#1E293B;padding:22px;border-radius:14px;box-shadow:0 8px 24px rgba(0,0,0,0.3)}";
  html += "h2{margin-top:0;color:#38BDF8;text-align:center;font-size:22px}";
  html += ".stat{background:#0F172A;padding:12px;border-radius:10px;margin-bottom:16px;font-size:13px;line-height:1.6}";
  html += "label{font-size:12px;color:#94A3B8;display:block;margin-top:10px}";
  html += "input{width:100%;padding:10px;margin:6px 0;background:#0F172A;border:1px solid #334155;border-radius:8px;color:#fff;box-sizing:border-box}";
  html += "button{width:100%;padding:12px;background:#2563EB;color:#fff;border:none;border-radius:8px;font-weight:bold;cursor:pointer;margin-top:14px}";
  html += ".btn-feed{background:#059669;margin-top:10px}";
  html += "</style></head><body><div class='card'>";
  html += "<h2>🐱 Smart Cat Feeder Pro</h2>";
  html += "<div class='stat'>";
  html += "<b>Hotspot AP:</b> CatFeeder-Setup (192.168.4.1)<br>";
  html += "<b>Status WiFi:</b> " + staStatus + "<br>";
  html += "<b>Waktu RTC:</b> " + rtcStr + "<br>";
  html += "<b>Servo:</b> Tutup " + String(servoCloseAngle) + "&deg; | Buka " + String(servoOpenAngle) + "&deg;";
  html += "</div>";

  html += "<form method='POST' action='/feed'><button type='submit' class='btn-feed'>🍖 BERI PAKAN SEKARANG</button></form>";

  html += "<h3 style='color:#E2E8F0;font-size:15px;margin-top:20px;'>⚙️ Pengaturan Wi-Fi Rumah</h3>";
  html += "<form method='POST' action='/wifi'>";
  html += "<label>Nama Wi-Fi (SSID):</label><input name='ssid' value='" + wifiSsid + "' required placeholder='Contoh: WiFi-Rumah'>";
  html += "<label>Password Wi-Fi:</label><input type='password' name='password' placeholder='••••••••'>";
  html += "<button type='submit'>💾 Simpan & Hubungkan</button>";
  html += "</form></div></body></html>";
  apServer.send(200, "text/html", html);
}

void handleApFeed() {
  startFeeding(-1);
  apServer.send(200, "text/html", "<meta http-equiv='refresh' content='2;url=/'><body style='background:#0F172A;color:#10B981;font-family:sans-serif;text-align:center;padding:40px;'><h2>✅ Pakan berhasil dikeluarkan!</h2><p>Kembali ke halaman utama...</p></body>");
}

void handleApWifi() {
  if (apServer.method() != HTTP_POST) { apServer.send(405, "text/plain", "Method Not Allowed"); return; }
  String ssid = apServer.arg("ssid"); ssid.trim();
  String pass = apServer.arg("password"); pass.trim();
  if (ssid.length() == 0) { apServer.send(400, "text/html", "<h2>SSID tidak boleh kosong</h2>"); return; }

  StaticJsonDocument<128> doc;
  doc["ssid"] = ssid; doc["password"] = pass;
  File f = LittleFS.open(WIFI_CONFIG_FILE, "w");
  if (f) { serializeJson(doc, f); f.close(); }

  wifiSsid = ssid;
  wifiPassword = pass;

  apServer.send(200, "text/html", "<meta http-equiv='refresh' content='3;url=/'><body style='background:#0F172A;color:#38BDF8;font-family:sans-serif;text-align:center;padding:40px;'><h2>💾 WiFi Tersimpan!</h2><p>NodeMCU sedang menghubungkan ke " + ssid + "...</p></body>");
  startWifiConnect();
}

void initDualWifi() {
  logInfo("Mengaktifkan Mode DUAL (WIFI_AP_STA)...");
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  apServer.on("/",       HTTP_GET,  handleApRoot);
  apServer.on("/wifi",   HTTP_GET,  handleApRoot);
  apServer.on("/wifi",   HTTP_POST, handleApWifi);
  apServer.on("/feed",   HTTP_POST, handleApFeed);
  apServer.begin();
  apModeActive = true;

  logInfo("Hotspot Aktif: " + String(AP_SSID) + " (IP: 192.168.4.1)");
}

void startAPMode() {
  initDualWifi();
}

void startWifiConnect() {
  if (wifiSsid.length() == 0) return;
  logInfo("Menghubungkan Station ke WiFi: " + wifiSsid);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  wifiConnectStart = millis();
  wifiMode         = WIFI_STATE_CONNECTING;
}

void updateWifi() {
  unsigned long now = millis();

  // Web Portal AP selalu aktif melayani klien 24/7
  apServer.handleClient();

  switch (wifiMode) {
    case WIFI_STATE_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        wifiMode = WIFI_STATE_CONNECTED;
        logInfo("WiFi Station Connected! IP: " + WiFi.localIP().toString());
        delay(200);
        syncOfflineLog();
      } else if (now - wifiConnectStart > WIFI_CONNECT_TIMEOUT) {
        logWarn("Koneksi ke WiFi '" + wifiSsid + "' belum berhasil. Hotspot tetap aktif, retry dalam 30 detik...");
        wifiRetryStart = now;
        wifiMode = WIFI_STATE_RETRY;
      }
      break;
    case WIFI_STATE_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        logWarn("WiFi terputus! Reconnecting...");
        wifiMode = WIFI_STATE_CONNECTING;
        WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
        wifiConnectStart = now;
      }
      break;
    case WIFI_STATE_RETRY:
      if (now - wifiRetryStart >= WIFI_RETRY_INTERVAL) {
        logInfo("Retry koneksi ke WiFi...");
        startWifiConnect();
      }
      break;
    case WIFI_STATE_AP:
      break;
  }
}

// ============================================================
//  STATUS & SERIAL
// ============================================================
void printStatus() {
  Serial.println("\n=== STATUS STEP 6 ===");
  Serial.println("  WiFi    : " + String(wifiMode == WIFI_STATE_CONNECTED ? "CONNECTED " + WiFi.localIP().toString() : (wifiMode == WIFI_STATE_AP ? "HOTSPOT AP (CatFeeder-Setup)" : "DISCONNECTED")));
  Serial.println("  Server  : " + String(SERVER_HOST) + ":" + String(SERVER_PORT));
  if (rtcReady) {
    DateTime now = rtc.now();
    char tbuf[25];
    snprintf(tbuf, sizeof(tbuf), "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    Serial.println("  RTC     : " + String(tbuf));
  }
  Serial.println("  Servo   : Close=" + String(servoCloseAngle) + " deg, Open=" + String(servoOpenAngle) + " deg, Delay=" + String(feedDurationMs) + "ms");
  Serial.println("  Jadwal  : (" + String(totalSchedules) + " slot)");
}

void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      serialBuffer.trim();
      if (serialBuffer.length() == 0) { serialBuffer = ""; return; }
      if      (serialBuffer == "d") printStatus();
      else if (serialBuffer == "f") startFeeding(-1);
      else if (serialBuffer == "h") sendHeartbeat();
      else if (serialBuffer == "p") pollCommand();
      else if (serialBuffer == "a" || serialBuffer == "w") { initDualWifi(); }
      else if (serialBuffer == "R") { deleteWifiConfig(); delay(500); ESP.restart(); }
      serialBuffer = "";
    } else { if (c != '\r') serialBuffer += c; }
  }
}

// ============================================================
//  SETUP & LOOP
// ============================================================
void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println("\n=== SMART CAT FEEDER -- PRO ===");

  Wire.begin(SDA_PIN, SCL_PIN);
  if (rtc.begin()) {
    rtcReady = true;
    logInfo("DS3231 OK");
  } else {
    logError("DS3231 tidak terdeteksi!");
  }

  if (LittleFS.begin()) {
    fsReady = true;
    logInfo("LittleFS OK");
    loadServoConfig();
    loadSchedule();
  }

  // Tentukan pulse width eksplisit agar sudut tidak terpotong.
  // Default ESP8266 (1000-2000µs) menyebabkan 180° hanya menggerakkan
  // servo ke 90° fisik. SG90 & MG996R butuh range 500-2500µs.
  feederServo.attach(SERVO_PIN, 500, 2500);
  servoClose();
  delay(300);

  // Inisialisasi Mode Dual (Hotspot AP 192.168.4.1 SELALU AKTIF 24/7)
  initDualWifi();

  // Jika ada konfigurasi WiFi router, hubungkan di latar belakang
  wifiConfigExists = fsReady ? loadWifiConfig() : false;
  if (wifiConfigExists && wifiSsid.length() > 0) {
    startWifiConnect();
  }

  printStatus();
}

void loop() {
  unsigned long nowMs = millis();
  updateWifi();

  if (nowMs - lastCheckMs >= CHECK_INTERVAL) {
    lastCheckMs = nowMs;
    checkSchedule();
  }

  if (wifiMode == WIFI_STATE_CONNECTED && nowMs - lastHeartbeatMs >= HEARTBEAT_INTERVAL) {
    lastHeartbeatMs = nowMs;
    sendHeartbeat();
  }

  if (wifiMode == WIFI_STATE_CONNECTED && nowMs - lastPollMs >= POLL_INTERVAL) {
    lastPollMs = nowMs;
    pollCommand();
  }

  if (wifiMode == WIFI_STATE_CONNECTED && nowMs - lastSyncLogMs >= SYNC_LOG_INTERVAL) {
    lastSyncLogMs = nowMs;
    syncOfflineLog();
  }

  updateFeedStateMachine();
  handleSerial();
  yield();
}
