/*
 * ============================================================
 *  SMART CAT FEEDER -- STEP 3: Jadwal Lokal + LittleFS
 * ============================================================
 *  Tujuan  : Jadwal feeding tersimpan permanen di LittleFS
 *            Feeding otomatis berdasarkan DS3231 (tanpa internet)
 *  Board   : ESP8266 NodeMCU
 *  Pin     : SDA=GPIO4, SCL=GPIO5, SERVO=GPIO14
 *  Library : Wire.h, RTClib.h, Servo.h, LittleFS.h, ArduinoJson.h
 *
 *  File di LittleFS:
 *    /schedule.json  --> konfigurasi jadwal
 *    /feedlog.json   --> log feeding offline (dipakai di step 13)
 *
 *  TEST-07 : Jadwal tersimpan & terbaca dari LittleFS
 *  TEST-08 : Feeding otomatis terpicu tepat di jam jadwal
 *  TEST-09 : Anti-duplicate feeding (1 feeding per jadwal per hari)
 *  TEST-10 : Jadwal tetap berjalan setelah ESP8266 restart
 *
 *  Perintah Serial Monitor (9600 baud):
 *    'r'   --> Tampilkan jadwal dari LittleFS
 *    'w'   --> Set jadwal (muncul wizard)
 *    'd'   --> Tampilkan waktu RTC + status jadwal
 *    'f'   --> Force feeding manual sekarang
 *    'l'   --> Tampilkan log feeding
 *    'x'   --> Reset anti-duplicate (untuk testing ulang)
 *    's'   --> I2C Scanner
 *    'm'   --> Menu
 * ============================================================
 */

#include <Wire.h>
#include <RTClib.h>
#include <Servo.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

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
#define SCHEDULE_FILE   "/schedule.json"
#define FEEDLOG_FILE    "/feedlog.json"

// ----- JADWAL STRUCT -----
struct Schedule {
  bool enabled;
  int  hour;
  int  minute;
};

// Sistem mendukung 2 jadwal
#define MAX_SCHEDULES 2
Schedule schedules[MAX_SCHEDULES];

// ----- ANTI-DUPLICATE -----
// Menyimpan jadwal terakhir yang sudah dieksekusi hari ini
// Format: "YYYY-MM-DD"
char lastFeedDate[11]        = "";
int  lastFeedSlot            = -1;  // slot 0 atau 1, -1 = belum ada

// ----- OBJECTS -----
RTC_DS3231 rtc;
Servo feederServo;

// ----- STATE -----
bool rtcReady   = false;
bool fsReady    = false;

// ----- SERVO STATE MACHINE -----
enum FeedState { FEED_IDLE, FEED_OPENING, FEED_OPEN_WAIT, FEED_CLOSING, FEED_DONE };
FeedState     feedState         = FEED_IDLE;
unsigned long feedTimer         = 0;
bool          feedingInProgress = false;
int           feedTriggerSlot   = -1;  // slot yang memicu feeding (-1 = manual)

// ----- SERIAL -----
String serialBuffer  = "";
int    wizardStep    = 0;   // untuk wizard set jadwal
int    wizardSlot    = 0;

// ----- TIMING -----
unsigned long lastCheckMs  = 0;
const unsigned long CHECK_INTERVAL = 1000; // cek jadwal tiap 1 detik

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

void logInfo(const String& msg)  { Serial.println(getTimestamp() + " [INFO]  " + msg); }
void logWarn(const String& msg)  { Serial.println(getTimestamp() + " [WARN]  " + msg); }
void logError(const String& msg) { Serial.println(getTimestamp() + " [ERROR] " + msg); }

// ============================================================
//  INISIALISASI RTC
// ============================================================
bool initRTC() {
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!rtc.begin()) {
    logError("DS3231 tidak terdeteksi! (SDA=GPIO4, SCL=GPIO5)");
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
//  LITTLEFS
// ============================================================
bool initFS() {
  if (!LittleFS.begin()) {
    logError("LittleFS mount GAGAL!");
    logError("Pastikan LittleFS sudah di-flash via Tools --> ESP8266 LittleFS Data Upload");
    return false;
  }
  logInfo("LittleFS OK");

  // Tampilkan info filesystem
  FSInfo fs_info;
  LittleFS.info(fs_info);
  char buf[80];
  snprintf(buf, sizeof(buf),
    "FS: total=%lu bytes, used=%lu bytes",
    fs_info.totalBytes, fs_info.usedBytes
  );
  logInfo(buf);
  return true;
}

// ============================================================
//  JADWAL -- DEFAULT
// ============================================================
void setDefaultSchedule() {
  schedules[0].enabled = true;
  schedules[0].hour    = 7;
  schedules[0].minute  = 0;

  schedules[1].enabled = true;
  schedules[1].hour    = 18;
  schedules[1].minute  = 0;

  logInfo("Jadwal default dimuat: 07:00 dan 18:00");
}

// ============================================================
//  JADWAL -- SIMPAN KE LITTLEFS
// ============================================================
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
  if (!f) {
    logError("Gagal membuka " + String(SCHEDULE_FILE) + " untuk write");
    return false;
  }

  serializeJson(doc, f);
  f.close();

  logInfo("Jadwal tersimpan ke " + String(SCHEDULE_FILE));
  return true;
}

// ============================================================
//  JADWAL -- LOAD DARI LITTLEFS
// ============================================================
bool loadSchedule() {
  if (!LittleFS.exists(SCHEDULE_FILE)) {
    logWarn("File jadwal tidak ada, gunakan default");
    setDefaultSchedule();
    saveSchedule();
    return true;
  }

  File f = LittleFS.open(SCHEDULE_FILE, "r");
  if (!f) {
    logError("Gagal membuka " + String(SCHEDULE_FILE) + " untuk read");
    setDefaultSchedule();
    return false;
  }

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    logError("JSON parse error: " + String(err.c_str()));
    logWarn("Reset ke jadwal default");
    setDefaultSchedule();
    saveSchedule();
    return false;
  }

  schedules[0].enabled = doc["schedule1"]["enabled"] | true;
  schedules[0].hour    = doc["schedule1"]["hour"]    | 7;
  schedules[0].minute  = doc["schedule1"]["minute"]  | 0;

  schedules[1].enabled = doc["schedule2"]["enabled"] | true;
  schedules[1].hour    = doc["schedule2"]["hour"]    | 18;
  schedules[1].minute  = doc["schedule2"]["minute"]  | 0;

  logInfo("Jadwal dimuat dari LittleFS:");
  for (int i = 0; i < MAX_SCHEDULES; i++) {
    char buf[60];
    snprintf(buf, sizeof(buf), "  Slot %d: %s  %02d:%02d",
      i + 1,
      schedules[i].enabled ? "[ON] " : "[OFF]",
      schedules[i].hour,
      schedules[i].minute
    );
    logInfo(buf);
  }
  return true;
}

// ============================================================
//  TAMPILKAN JADWAL
// ============================================================
void printSchedule() {
  Serial.println();
  Serial.println("=== JADWAL FEEDING ===");
  for (int i = 0; i < MAX_SCHEDULES; i++) {
    char buf[60];
    snprintf(buf, sizeof(buf), "  Slot %d: %s  %02d:%02d",
      i + 1,
      schedules[i].enabled ? "[ON ] " : "[OFF]",
      schedules[i].hour,
      schedules[i].minute
    );
    Serial.println(buf);
  }
  Serial.println();
  Serial.print("  Anti-dup date : "); Serial.println(lastFeedDate[0] ? lastFeedDate : "(belum ada)");
  Serial.print("  Anti-dup slot : "); Serial.println(lastFeedSlot >= 0 ? String(lastFeedSlot + 1) : "(belum ada)");
  Serial.println("======================");
  Serial.println();
}

// ============================================================
//  STATUS REAL-TIME
// ============================================================
void printStatus() {
  if (!rtcReady) { logError("RTC tidak siap"); return; }
  DateTime now = rtc.now();

  Serial.println();
  Serial.println("=== STATUS ===");
  char tbuf[40];
  snprintf(tbuf, sizeof(tbuf), "  Waktu  : %04d-%02d-%02d %02d:%02d:%02d",
    now.year(), now.month(), now.day(),
    now.hour(), now.minute(), now.second()
  );
  Serial.println(tbuf);
  Serial.print("  Suhu   : "); Serial.print(rtc.getTemperature()); Serial.println(" C");
  Serial.println();

  for (int i = 0; i < MAX_SCHEDULES; i++) {
    char buf[80];

    // Cek apakah jadwal ini sudah dieksekusi hari ini
    char today[11];
    snprintf(today, sizeof(today), "%04d-%02d-%02d", now.year(), now.month(), now.day());
    bool doneToday = (strcmp(lastFeedDate, today) == 0 && lastFeedSlot == i);

    snprintf(buf, sizeof(buf), "  Jadwal %d: %s  %02d:%02d  %s",
      i + 1,
      schedules[i].enabled ? "[ON ] " : "[OFF]",
      schedules[i].hour,
      schedules[i].minute,
      doneToday ? "<-- sudah dieksekusi hari ini" : ""
    );
    Serial.println(buf);
  }

  Serial.print("  Feeding: "); Serial.println(feedingInProgress ? "SEDANG BERJALAN" : "IDLE");
  Serial.println("==============");
  Serial.println();
}

// ============================================================
//  LOG FEEDING KE LITTLEFS
//  Digunakan untuk offline queue (step 13)
//  Di step ini kita sudah siapkan strukturnya
// ============================================================
void appendFeedLog(const String& type, int slot, const String& status) {
  if (!fsReady) return;

  DateTime now = rtc.now();
  char timestamp[20];
  snprintf(timestamp, sizeof(timestamp),
    "%04d-%02d-%02d %02d:%02d:%02d",
    now.year(), now.month(), now.day(),
    now.hour(), now.minute(), now.second()
  );

  // Baca log yang ada
  StaticJsonDocument<2048> doc;
  JsonArray logs;

  if (LittleFS.exists(FEEDLOG_FILE)) {
    File rf = LittleFS.open(FEEDLOG_FILE, "r");
    if (rf) {
      DeserializationError err = deserializeJson(doc, rf);
      rf.close();
      if (!err && doc.containsKey("logs")) {
        logs = doc["logs"].as<JsonArray>();
      }
    }
  }

  if (logs.isNull()) {
    logs = doc.createNestedArray("logs");
  }

  // Tambah entry baru
  JsonObject entry = logs.createNestedObject();
  entry["timestamp"]   = timestamp;
  entry["type"]        = type;
  entry["slot"]        = slot;
  entry["status"]      = status;
  entry["synced"]      = false;  // belum di-upload ke server

  // Simpan kembali (max 50 entry agar tidak overflow)
  while (logs.size() > 50) {
    logs.remove(0);
  }

  File wf = LittleFS.open(FEEDLOG_FILE, "w");
  if (wf) {
    serializeJson(doc, wf);
    wf.close();
    logInfo("Log feeding disimpan: " + type + " slot=" + String(slot) + " " + status);
  } else {
    logError("Gagal menyimpan feed log");
  }
}

// ============================================================
//  TAMPILKAN LOG FEEDING
// ============================================================
void printFeedLog() {
  if (!LittleFS.exists(FEEDLOG_FILE)) {
    logInfo("Belum ada log feeding");
    return;
  }

  File f = LittleFS.open(FEEDLOG_FILE, "r");
  if (!f) { logError("Gagal buka feedlog"); return; }

  StaticJsonDocument<2048> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err || !doc.containsKey("logs")) {
    logError("feedlog.json rusak atau kosong");
    return;
  }

  JsonArray logs = doc["logs"].as<JsonArray>();
  Serial.println();
  Serial.println("=== FEED LOG ===");
  Serial.print("  Total: "); Serial.println(logs.size());
  Serial.println("  ----------------------------------------");

  for (JsonObject entry : logs) {
    char buf[100];
    snprintf(buf, sizeof(buf), "  %s | %s | slot=%d | %s | synced=%s",
      entry["timestamp"].as<const char*>(),
      entry["type"].as<const char*>(),
      entry["slot"].as<int>(),
      entry["status"].as<const char*>(),
      entry["synced"].as<bool>() ? "yes" : "no"
    );
    Serial.println(buf);
  }
  Serial.println("================");
  Serial.println();
}

// ============================================================
//  SERVO KONTROL
// ============================================================
void servoMoveTo(int angle) {
  feederServo.write(constrain(angle, 0, 180));
}

void servoOpen()  {
  logInfo("Servo OPEN  --> " + String(SERVO_OPEN)  + " deg");
  servoMoveTo(SERVO_OPEN);
}

void servoClose() {
  logInfo("Servo CLOSE --> " + String(SERVO_CLOSE) + " deg");
  servoMoveTo(SERVO_CLOSE);
}

// ============================================================
//  FEEDING STATE MACHINE
// ============================================================
void startFeeding(int slot) {
  if (feedingInProgress) {
    logWarn("Feeding sedang berjalan! Perintah diabaikan.");
    return;
  }

  if (!rtcReady) {
    logError("RTC tidak siap! Feeding dibatalkan (waktu tidak valid).");
    return;
  }

  logInfo("=== FEEDING DIMULAI === (slot=" + String(slot < 0 ? 0 : slot + 1) + ", " +
          String(slot < 0 ? "manual" : "jadwal") + ")");

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
      logInfo("Pakan keluar... tunggu " + String(FEED_DURATION) + "ms");
      break;

    case FEED_OPEN_WAIT:
      if (now - feedTimer >= (unsigned long)FEED_DURATION) {
        feedState = FEED_CLOSING;
      }
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

        // Simpan log ke LittleFS
        if (rtcReady) {
          String type = (feedTriggerSlot < 0) ? "manual" : "schedule";
          appendFeedLog(type, feedTriggerSlot < 0 ? 0 : feedTriggerSlot + 1, "success");
        }

        feedState = FEED_IDLE;
        feedTriggerSlot = -1;
      }
      break;

    default:
      feedState = FEED_IDLE;
      feedingInProgress = false;
      break;
  }
}

// ============================================================
//  CEK JADWAL
//  Dipanggil setiap 1 detik
//  Hanya memicu feeding jika:
//    1. Jadwal enabled
//    2. Jam & menit cocok dengan RTC
//    3. Belum feeding di jadwal + hari yang sama
//    4. Detik = 0 (tepat di menit pertama, bukan berulang)
// ============================================================
void checkSchedule() {
  if (!rtcReady)          return;
  if (feedingInProgress)  return;

  DateTime now = rtc.now();

  // Format hari ini sebagai string
  char today[11];
  snprintf(today, sizeof(today), "%04d-%02d-%02d", now.year(), now.month(), now.day());

  for (int i = 0; i < MAX_SCHEDULES; i++) {
    if (!schedules[i].enabled) continue;
    if (now.hour()   != schedules[i].hour)   continue;
    if (now.minute() != schedules[i].minute) continue;

    // Hanya eksekusi di detik 0-4 (toleransi 5 detik pertama)
    // Ini penting agar saat restart tepat di menit jadwal,
    // feeding tetap bisa terpicu
    if (now.second() > 4) continue;

    // Anti-duplicate: cek apakah sudah feeding di slot ini hari ini
    bool alreadyFed = (strcmp(lastFeedDate, today) == 0 && lastFeedSlot == i);
    if (alreadyFed) {
      // Sudah feeding -- tidak perlu log lagi (sudah di-log sebelumnya)
      continue;
    }

    // === FEEDING TERPICU ===
    char buf[60];
    snprintf(buf, sizeof(buf), "Jadwal %d terpicu: %02d:%02d", i + 1, schedules[i].hour, schedules[i].minute);
    logInfo(buf);

    // Update anti-duplicate SEBELUM feeding (agar tidak double trigger)
    strcpy(lastFeedDate, today);
    lastFeedSlot = i;

    startFeeding(i);
    break; // hanya satu feeding per pengecekan
  }
}

// ============================================================
//  I2C SCANNER
// ============================================================
void scanI2C() {
  Serial.println("\n=== I2C SCANNER (SDA=GPIO4, SCL=GPIO5) ===");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  0x");
      if (addr < 16) Serial.print("0");
      Serial.print(addr, HEX);
      if (addr == 0x68) Serial.print("  <-- DS3231");
      if (addr == 0x57) Serial.print("  <-- AT24C32");
      Serial.println();
      found++;
    }
  }
  if (found == 0) Serial.println("  Tidak ada device ditemukan!");
  Serial.println("==========================================\n");
}

// ============================================================
//  WIZARD SET JADWAL VIA SERIAL
//  Step: slot --> jam --> menit --> on/off
// ============================================================
void handleWizard(const String& input) {
  switch (wizardStep) {
    case 1:
      wizardSlot = input.toInt() - 1;
      if (wizardSlot < 0 || wizardSlot >= MAX_SCHEDULES) {
        logError("Slot tidak valid. Harus 1 atau 2.");
        wizardStep = 0;
      } else {
        Serial.println("Jam (0-23):");
        wizardStep = 2;
      }
      break;

    case 2: {
      int h = input.toInt();
      if (h < 0 || h > 23) {
        logError("Jam tidak valid (0-23)");
        wizardStep = 0;
      } else {
        schedules[wizardSlot].hour = h;
        Serial.println("Menit (0-59):");
        wizardStep = 3;
      }
      break;
    }

    case 3: {
      int m = input.toInt();
      if (m < 0 || m > 59) {
        logError("Menit tidak valid (0-59)");
        wizardStep = 0;
      } else {
        schedules[wizardSlot].minute = m;
        Serial.println("Aktif? (1=ON, 0=OFF):");
        wizardStep = 4;
      }
      break;
    }

    case 4: {
      schedules[wizardSlot].enabled = (input.toInt() == 1);
      char buf[80];
      snprintf(buf, sizeof(buf), "Jadwal %d diset: %s  %02d:%02d",
        wizardSlot + 1,
        schedules[wizardSlot].enabled ? "[ON]" : "[OFF]",
        schedules[wizardSlot].hour,
        schedules[wizardSlot].minute
      );
      logInfo(buf);
      saveSchedule();
      wizardStep = 0;
      printSchedule();
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
  Serial.println("  SMART CAT FEEDER -- STEP 3 MENU");
  Serial.println("========================================");
  Serial.println("  d   --> Status real-time (RTC + jadwal)");
  Serial.println("  r   --> Tampilkan jadwal");
  Serial.println("  w   --> Set jadwal (wizard)");
  Serial.println("  f   --> Force feeding manual sekarang");
  Serial.println("  l   --> Log feeding");
  Serial.println("  x   --> Reset anti-duplicate (utk testing)");
  Serial.println("  s   --> I2C Scanner");
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

      // Mode wizard
      if (wizardStep > 0) {
        handleWizard(serialBuffer);
        serialBuffer = "";
        return;
      }

      // Perintah utama
      if      (serialBuffer == "d") { printStatus(); }
      else if (serialBuffer == "r") { printSchedule(); }
      else if (serialBuffer == "w") {
        Serial.println("Set jadwal slot (1 atau 2):");
        wizardStep = 1;
      }
      else if (serialBuffer == "f") {
        logInfo("Force feeding manual dari Serial");
        startFeeding(-1);  // -1 = manual
      }
      else if (serialBuffer == "l") { printFeedLog(); }
      else if (serialBuffer == "x") {
        lastFeedDate[0] = '\0';
        lastFeedSlot = -1;
        logInfo("Anti-duplicate direset. Jadwal bisa terpicu lagi.");
      }
      else if (serialBuffer == "s") { scanI2C(); }
      else if (serialBuffer == "m") { printMenu(); }
      else {
        logError("Perintah tidak dikenal: " + serialBuffer);
        printMenu();
      }

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
  Serial.println("  STEP 3: Jadwal + LittleFS");
  Serial.println("  Build: " __DATE__ " " __TIME__);
  Serial.println("========================================");

  // RTC
  rtcReady = initRTC();
  if (!rtcReady) {
    logError("RTC gagal! Jadwal otomatis tidak akan berjalan.");
    logError("Perbaiki RTC lalu restart.");
  }

  // LittleFS
  fsReady = initFS();

  // Load jadwal
  if (fsReady) {
    loadSchedule();
  } else {
    logWarn("LittleFS gagal, pakai jadwal default di RAM saja");
    setDefaultSchedule();
  }

  // Servo
  feederServo.attach(SERVO_PIN, 500, 2500); // 500-2500µs agar 0-180° akurat
  logInfo("Servo attached GPIO" + String(SERVO_PIN));
  servoClose();
  delay(300);

  // Tampilkan status awal
  printStatus();
  printMenu();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  // Cek jadwal setiap 1 detik
  unsigned long nowMs = millis();
  if (nowMs - lastCheckMs >= CHECK_INTERVAL) {
    lastCheckMs = nowMs;
    checkSchedule();
  }

  // Update state machine feeding (non-blocking)
  updateFeedStateMachine();

  // Proses input Serial
  handleSerial();

  // Beri CPU time ke background tasks ESP8266
  yield();
}

/*
 * ============================================================
 *  LIBRARY YANG DIBUTUHKAN
 * ============================================================
 *  Tools --> Manage Libraries:
 *  1. RTClib by Adafruit
 *  2. ArduinoJson by Benoit Blanchon  (versi 6.x)
 *  3. LittleFS --> sudah built-in di ESP8266 Arduino core >= 2.6.0
 *  4. Servo    --> sudah built-in
 *
 *  PENTING - LittleFS Data Upload:
 *  Untuk menggunakan LittleFS, tidak perlu upload file manual
 *  di step ini. File akan dibuat otomatis oleh firmware.
 *  Pastikan Tools --> Flash Size --> "4MB (FS:2MB OTA:~1019KB)"
 *  atau sesuaikan dengan board kamu.
 * ============================================================
 *
 *  WIRING (sama dengan STEP 2)
 * ============================================================
 *  DS3231  VCC --> 3.3V
 *  DS3231  GND --> GND
 *  DS3231  SDA --> GPIO4
 *  DS3231  SCL --> GPIO5
 *  Servo Signal --> GPIO14
 *  Servo   VCC --> VIN (5V)
 *  Servo   GND --> GND
 * ============================================================
 *
 *  CARA TEST ANTI-DUPLICATE
 * ============================================================
 *  1. Set jadwal ke 1-2 menit ke depan dengan perintah 'w'
 *  2. Tunggu jadwal terpicu --> servo bergerak
 *  3. Di menit yang sama, ketik 'x' lalu Enter untuk reset
 *  4. Pastikan feeding TIDAK terpicu lagi di menit yang sama
 *     (karena second() sudah > 4)
 *  5. Tunggu menit berikutnya --> feeding terpicu lagi (normal)
 *
 *  CHECKLIST TEST STEP 3
 * ============================================================
 *  [TEST-07] Ketik 'r' --> jadwal 07:00 dan 18:00 terbaca
 *  [TEST-07] Ketik 'w' --> set jadwal baru --> restart ESP8266
 *            --> jadwal masih tersimpan (LittleFS persistent)
 *  [TEST-08] Set jadwal ke 1 menit ke depan --> servo terpicu otomatis
 *  [TEST-09] Feeding tidak terpicu 2x di menit yang sama
 *  [TEST-10] Restart ESP8266 --> jadwal masih ada di 'r'
 *
 *  Setelah semua TEST PASS --> Lanjut STEP 4: WiFi ESP8266
 * ============================================================
 */
