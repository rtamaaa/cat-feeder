/*
 * ============================================================
 *  SMART CAT FEEDER -- STEP 2: ESP8266 + DS3231 + SERVO
 * ============================================================
 *  Tujuan  : Uji servo motor GPIO14 + integrasi DS3231
 *  Board   : ESP8266 NodeMCU
 *  Pin     : SDA=GPIO4, SCL=GPIO5, SERVO=GPIO14
 *  Library : Wire.h, RTClib.h, Servo.h (ESP8266)
 *
 *  TEST-04 : Servo bergerak OPEN (0 -> 90 derajat)
 *  TEST-05 : Servo bergerak CLOSE (90 -> 0 derajat)
 *  TEST-06 : Feeding sequence lengkap (non-blocking via state machine)
 *
 *  Perintah Serial Monitor (9600 baud):
 *    'f'   --> Feeding sequence (open -> tunggu -> close)
 *    'o'   --> Servo OPEN (90 derajat)
 *    'c'   --> Servo CLOSE (0 derajat)
 *    'p'   --> Set posisi custom (muncul prompt)
 *    't'   --> Tampilkan waktu RTC
 *    's'   --> I2C Scanner
 *    'm'   --> Menu
 * ============================================================
 */

#include <Wire.h>
#include <RTClib.h>
#include <Servo.h>

// ----- PIN DEFINITIONS -----
#define SDA_PIN     4    // GPIO4
#define SCL_PIN     5    // GPIO5
#define SERVO_PIN   14   // GPIO14

// ----- SERVO PARAMETERS -----
// Ubah nilai-nilai ini untuk menyesuaikan mekanik dispenser
#define SERVO_CLOSE     0     // posisi tutup (derajat)
#define SERVO_OPEN      90    // posisi buka (derajat)
#define FEED_DURATION   2000  // durasi buka servo (ms)
#define SERVO_MOVE_DELAY 300  // jeda setelah servo bergerak sebelum lanjut (ms)

// ----- OBJECTS -----
RTC_DS3231 rtc;
Servo feederServo;

// ----- SERVO STATE MACHINE -----
// State feeding menggunakan millis() agar tidak blocking
enum FeedState {
  FEED_IDLE,
  FEED_OPENING,
  FEED_OPEN_WAIT,
  FEED_CLOSING,
  FEED_DONE
};

FeedState feedState     = FEED_IDLE;
unsigned long feedTimer = 0;
bool feedingInProgress  = false;

// ----- RTC STATE -----
bool rtcReady = false;

// ----- SERIAL -----
String serialBuffer = "";
bool waitingForAngle = false;

// ============================================================
//  LOGGER
// ============================================================
void logInfo(const String& msg) {
  if (rtcReady) {
    DateTime now = rtc.now();
    char buf[12];
    snprintf(buf, sizeof(buf), "[%02d:%02d:%02d]", now.hour(), now.minute(), now.second());
    Serial.print(buf);
  } else {
    Serial.print("[--:--:--]");
  }
  Serial.print(" [INFO] ");
  Serial.println(msg);
}

void logWarn(const String& msg) {
  if (rtcReady) {
    DateTime now = rtc.now();
    char buf[12];
    snprintf(buf, sizeof(buf), "[%02d:%02d:%02d]", now.hour(), now.minute(), now.second());
    Serial.print(buf);
  } else {
    Serial.print("[--:--:--]");
  }
  Serial.print(" [WARN] ");
  Serial.println(msg);
}

void logError(const String& msg) {
  if (rtcReady) {
    DateTime now = rtc.now();
    char buf[12];
    snprintf(buf, sizeof(buf), "[%02d:%02d:%02d]", now.hour(), now.minute(), now.second());
    Serial.print(buf);
  } else {
    Serial.print("[--:--:--]");
  }
  Serial.print(" [ERROR] ");
  Serial.println(msg);
}

// ============================================================
//  I2C SCANNER
// ============================================================
void scanI2C() {
  Serial.println();
  Serial.println("=== I2C SCANNER ===");
  Serial.print("SDA=GPIO"); Serial.print(SDA_PIN);
  Serial.print(" SCL=GPIO"); Serial.println(SCL_PIN);

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
  Serial.println("===================");
  Serial.println();
}

// ============================================================
//  INISIALISASI DS3231
// ============================================================
bool initRTC() {
  Wire.begin(SDA_PIN, SCL_PIN);
  logInfo("I2C init SDA=GPIO4 SCL=GPIO5");

  if (!rtc.begin()) {
    logError("DS3231 tidak terdeteksi! Ketik 's' untuk scan I2C.");
    return false;
  }

  logInfo("DS3231 OK -- Alamat 0x68");

  if (rtc.lostPower()) {
    logWarn("RTC lostPower! Set ke waktu compile.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  rtc.disable32K();
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  rtc.disableAlarm(1);
  rtc.disableAlarm(2);

  return true;
}

// ============================================================
//  TAMPILKAN WAKTU RTC
// ============================================================
void printRTC() {
  if (!rtcReady) { logError("RTC belum siap"); return; }
  DateTime now = rtc.now();
  char buf[64];
  snprintf(buf, sizeof(buf),
    "RTC: %04d-%02d-%02d %02d:%02d:%02d | Suhu: %.1fC",
    now.year(), now.month(), now.day(),
    now.hour(), now.minute(), now.second(),
    rtc.getTemperature()
  );
  logInfo(buf);
}

// ============================================================
//  SERVO KONTROL LANGSUNG
// ============================================================
void servoMoveTo(int angle) {
  angle = constrain(angle, 0, 180);
  feederServo.write(angle);
  char buf[40];
  snprintf(buf, sizeof(buf), "Servo --> %d derajat", angle);
  logInfo(buf);
}

void servoOpen() {
  logInfo("Servo OPEN (" + String(SERVO_OPEN) + " deg)");
  servoMoveTo(SERVO_OPEN);
}

void servoClose() {
  logInfo("Servo CLOSE (" + String(SERVO_CLOSE) + " deg)");
  servoMoveTo(SERVO_CLOSE);
}

// ============================================================
//  FEEDING STATE MACHINE (non-blocking)
//
//  Alur:
//    IDLE
//     --> OPENING  : servo bergerak ke posisi OPEN
//     --> OPEN_WAIT: tunggu FEED_DURATION ms (pakan keluar)
//     --> CLOSING  : servo bergerak ke posisi CLOSE
//     --> DONE     : log selesai, kembali ke IDLE
//
//  Tidak menggunakan delay() agar loop() tetap responsif
// ============================================================
void startFeeding() {
  if (feedingInProgress) {
    logWarn("FEEDING sedang berjalan! Perintah diabaikan.");
    return;
  }

  logInfo("=== FEEDING DIMULAI ===");
  feedingInProgress = true;
  feedState = FEED_OPENING;
  feedTimer = millis();
}

void updateFeedStateMachine() {
  if (feedState == FEED_IDLE) return;

  unsigned long now = millis();

  switch (feedState) {

    case FEED_OPENING:
      servoOpen();
      feedTimer = now;
      feedState = FEED_OPEN_WAIT;
      logInfo("Menunggu " + String(FEED_DURATION) + "ms...");
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
      if (now - feedTimer >= (unsigned long)SERVO_MOVE_DELAY) {
        logInfo("=== FEEDING SELESAI ===");
        feedingInProgress = false;
        feedState = FEED_IDLE;
      }
      break;

    default:
      feedState = FEED_IDLE;
      feedingInProgress = false;
      break;
  }
}

// ============================================================
//  TAMPILKAN STATUS SERVO
// ============================================================
void printServoStatus() {
  Serial.println();
  Serial.println("=== STATUS SERVO ===");
  Serial.print("  Pin         : GPIO"); Serial.println(SERVO_PIN);
  Serial.print("  CLOSE pos   : "); Serial.print(SERVO_CLOSE); Serial.println(" deg");
  Serial.print("  OPEN pos    : "); Serial.print(SERVO_OPEN); Serial.println(" deg");
  Serial.print("  Feed dur    : "); Serial.print(FEED_DURATION); Serial.println(" ms");
  Serial.print("  State       : ");
  switch (feedState) {
    case FEED_IDLE:      Serial.println("IDLE"); break;
    case FEED_OPENING:   Serial.println("OPENING"); break;
    case FEED_OPEN_WAIT: Serial.println("OPEN_WAIT"); break;
    case FEED_CLOSING:   Serial.println("CLOSING"); break;
    case FEED_DONE:      Serial.println("DONE"); break;
  }
  Serial.print("  In progress : "); Serial.println(feedingInProgress ? "YA" : "TIDAK");
  Serial.println("====================");
  Serial.println();
}

// ============================================================
//  MENU
// ============================================================
void printMenu() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("  SMART CAT FEEDER -- STEP 2 MENU");
  Serial.println("========================================");
  Serial.println("  f   --> Feeding sequence lengkap");
  Serial.println("  o   --> Servo OPEN saja");
  Serial.println("  c   --> Servo CLOSE saja");
  Serial.println("  p   --> Servo ke posisi custom (0-180)");
  Serial.println("  v   --> Status servo");
  Serial.println("  t   --> Waktu RTC");
  Serial.println("  s   --> I2C Scanner");
  Serial.println("  m   --> Menu ini");
  Serial.println("========================================");
  Serial.println();
}

// ============================================================
//  HANDLE SERIAL INPUT
// ============================================================
void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();

    if (c == '\n' || c == '\r') {
      serialBuffer.trim();

      if (serialBuffer.length() == 0) {
        serialBuffer = "";
        return;
      }

      // Mode input sudut custom
      if (waitingForAngle) {
        int angle = serialBuffer.toInt();
        if (angle >= 0 && angle <= 180) {
          servoMoveTo(angle);
        } else {
          logError("Sudut harus 0-180. Input: " + serialBuffer);
        }
        waitingForAngle = false;
        serialBuffer = "";
        return;
      }

      // Perintah utama
      if (serialBuffer == "f") {
        startFeeding();
      }
      else if (serialBuffer == "o") {
        if (feedingInProgress) {
          logWarn("Feeding sedang berjalan, perintah diabaikan");
        } else {
          servoOpen();
        }
      }
      else if (serialBuffer == "c") {
        if (feedingInProgress) {
          logWarn("Feeding sedang berjalan, perintah diabaikan");
        } else {
          servoClose();
        }
      }
      else if (serialBuffer == "p") {
        Serial.println("Masukkan sudut (0-180) lalu Enter:");
        waitingForAngle = true;
      }
      else if (serialBuffer == "v") {
        printServoStatus();
      }
      else if (serialBuffer == "t") {
        printRTC();
      }
      else if (serialBuffer == "s") {
        scanI2C();
      }
      else if (serialBuffer == "m") {
        printMenu();
      }
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
  Serial.println("  STEP 2: ESP8266 + DS3231 + SERVO");
  Serial.println("  Build: " __DATE__ " " __TIME__);
  Serial.println("========================================");

  // Inisialisasi RTC
  rtcReady = initRTC();
  if (!rtcReady) {
    logWarn("RTC gagal -- lanjut tanpa RTC (servo tetap bisa diuji)");
  } else {
    printRTC();
  }

  // Inisialisasi Servo
  feederServo.attach(SERVO_PIN, 500, 2500); // 500-2500µs agar 0-180° akurat
  logInfo("Servo attached di GPIO" + String(SERVO_PIN));

  // Pastikan servo mulai di posisi CLOSE
  servoClose();
  delay(500);
  logInfo("Servo siap di posisi CLOSE (" + String(SERVO_CLOSE) + " deg)");

  printServoStatus();
  printMenu();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  // Update state machine feeding (non-blocking)
  updateFeedStateMachine();

  // Proses input Serial
  handleSerial();

  // Beri sedikit jeda agar ESP8266 tidak watchdog reset
  yield();
}

/*
 * ============================================================
 *  LIBRARY YANG DIBUTUHKAN
 * ============================================================
 *  1. RTClib by Adafruit        --> Manage Libraries
 *  2. Servo.h --> sudah built-in di ESP8266 Arduino core
 *
 *  CATATAN: Gunakan "Servo.h" dari ESP8266 core, BUKAN
 *  "ESP8266Servo" yang terpisah. Jika ragu, cek:
 *  Tools --> Manage Libraries --> cari "ESP8266Servo"
 *  dan install jika Servo.h tidak dikenali.
 * ============================================================
 *
 *  WIRING DIAGRAM
 * ============================================================
 *  DS3231 Pin    ESP8266 NodeMCU
 *  ----------    ---------------
 *  VCC      -->  3.3V
 *  GND      -->  GND
 *  SDA      -->  GPIO4
 *  SCL      -->  GPIO5
 *
 *  Servo Pin     ESP8266 NodeMCU
 *  ---------     ---------------
 *  Signal   -->  GPIO14
 *  VCC      -->  5V (dari VIN/USB, BUKAN 3.3V)
 *  GND      -->  GND (sama dengan ESP8266)
 *
 *  PENTING SERVO:
 *  - Servo membutuhkan 5V, ambil dari pin VIN NodeMCU
 *    (ketika NodeMCU powered via USB)
 *  - Signal servo dari GPIO14 aman di 3.3V untuk kebanyakan servo
 *  - Jangan ambil power servo dari pin 3.3V,
 *    bisa menyebabkan NodeMCU reset saat servo bergerak
 *  - Jika servo bergetar/tidak mau diam, coba kapasitor
 *    100uF antara VCC dan GND servo
 * ============================================================
 *
 *  KALIBRASI SERVO
 * ============================================================
 *  Setiap servo berbeda. Gunakan perintah 'p' untuk
 *  mencari sudut yang tepat untuk posisi BUKA dan TUTUP
 *  dispenser pakan kucing kamu.
 *
 *  Setelah menemukan nilai yang tepat, ubah di kode:
 *    #define SERVO_CLOSE   <nilai_tutup>
 *    #define SERVO_OPEN    <nilai_buka>
 *    #define FEED_DURATION <durasi_ms>
 *
 *  CHECKLIST TEST STEP 2
 * ============================================================
 *  [TEST-04] Ketik 'o' --> Servo bergerak ke OPEN (90 deg)
 *  [TEST-05] Ketik 'c' --> Servo bergerak ke CLOSE (0 deg)
 *  [TEST-06] Ketik 'f' --> Feeding sequence berjalan:
 *            - Servo OPEN
 *            - Tunggu 2 detik
 *            - Servo CLOSE
 *            - Tampilkan "FEEDING SELESAI"
 *  [TEST-X]  Ketik 'f' dua kali cepat --> feeding kedua
 *            harus ditolak dengan pesan WARNING
 *
 *  Setelah semua TEST PASS --> Lanjut STEP 3: Jadwal + LittleFS
 * ============================================================
 */
