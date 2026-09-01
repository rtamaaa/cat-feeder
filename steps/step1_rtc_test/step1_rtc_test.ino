/*
 * ============================================================
 *  SMART CAT FEEDER — STEP 1: ESP8266 + DS3231
 * ============================================================
 *  Tujuan  : Uji koneksi DS3231 via I2C
 *  Board   : ESP8266 NodeMCU
 *  Pin     : SDA = GPIO4, SCL = GPIO5
 *  Library : Wire.h, RTClib.h
 *
 *  TEST-01 : DS3231 detected di alamat 0x68
 *  TEST-02 : RTC dapat membaca waktu
 *  TEST-03 : RTC dapat di-set via Serial Monitor
 *
 *  Perintah Serial Monitor (9600 baud):
 *    's'              --> Scan I2C bus
 *    't'              --> Tampilkan waktu RTC sekarang
 *    'S'              --> Set RTC ke waktu saat ini (dari compile time)
 *    'YYYYMMDDHHMMSS' --> Set RTC manual, contoh: 20260901183000
 *    'm'              --> Tampilkan menu
 * ============================================================
 */

#include <Wire.h>
#include <RTClib.h>

// ----- PIN DEFINITIONS -----
#define SDA_PIN   4   // GPIO4
#define SCL_PIN   5   // GPIO5

// ----- OBJECTS -----
RTC_DS3231 rtc;

// ----- STATE -----
bool rtcFound   = false;
bool rtcReady   = false;
unsigned long lastPrintMs = 0;
const unsigned long PRINT_INTERVAL = 1000; // cetak waktu tiap 1 detik

// ============================================================
//  SERIAL LOGGER
//  Format: [HH:MM:SS] [LEVEL] pesan
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
//  Scan seluruh alamat 0x01-0x7F, cetak yang ditemukan
// ============================================================
void scanI2C() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("  I2C SCANNER");
  Serial.println("========================================");
  Serial.print("  SDA = GPIO"); Serial.println(SDA_PIN);
  Serial.print("  SCL = GPIO"); Serial.println(SCL_PIN);
  Serial.println("----------------------------------------");

  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.print("  Device ditemukan di alamat 0x");
      if (addr < 16) Serial.print("0");
      Serial.print(addr, HEX);

      // Identifikasi device umum
      if (addr == 0x68) Serial.print("  <-- DS3231 RTC");
      if (addr == 0x57) Serial.print("  <-- AT24C32 EEPROM (modul DS3231)");
      if (addr == 0x3C || addr == 0x3D) Serial.print("  <-- OLED Display");

      Serial.println();
      found++;
    }
  }

  if (found == 0) {
    Serial.println("  TIDAK ADA device I2C yang terdeteksi!");
    Serial.println();
    Serial.println("  Periksa:");
    Serial.println("   - Kabel SDA ke GPIO4");
    Serial.println("   - Kabel SCL ke GPIO5");
    Serial.println("   - Power VCC DS3231 (3.3V atau 5V)");
    Serial.println("   - Kabel GND terhubung");
    Serial.println("   - Pull-up 4.7k ohm jika diperlukan");
  } else {
    Serial.print("  Total: ");
    Serial.print(found);
    Serial.println(" device ditemukan");
  }
  Serial.println("========================================");
  Serial.println();
}

// ============================================================
//  INISIALISASI DS3231
// ============================================================
bool initRTC() {
  Serial.println();
  Serial.println("----------------------------------------");
  Serial.println("  Inisialisasi DS3231...");

  if (!rtc.begin()) {
    logError("DS3231 tidak terdeteksi!");
    Serial.println();
    Serial.println("  Kemungkinan penyebab:");
    Serial.println("   1. Kabel SDA/SCL terbalik atau longgar");
    Serial.println("   2. Modul DS3231 tidak mendapat power");
    Serial.println("   3. Alamat I2C konflik dengan device lain");
    Serial.println("   4. Library RTClib belum terinstall");
    Serial.println();
    Serial.println("  --> Jalankan I2C Scanner: ketik 's' lalu Enter");
    return false;
  }

  logInfo("DS3231 terdeteksi!");
  logInfo("Alamat I2C = 0x68");

  // Cek apakah RTC kehilangan power (baterai habis / pertama kali)
  if (rtc.lostPower()) {
    logWarn("RTC kehilangan power! Waktu mungkin tidak akurat.");
    logWarn("Set ke waktu compile sebagai fallback.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Nonaktifkan sinyal 32kHz (hemat daya)
  rtc.disable32K();

  // Nonaktifkan alarm 1 & 2
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
  if (!rtcReady) {
    logError("RTC belum siap");
    return;
  }

  DateTime now = rtc.now();

  // Nama hari
  const char* days[] = {"Minggu","Senin","Selasa","Rabu","Kamis","Jumat","Sabtu"};

  char timeBuf[64];
  snprintf(timeBuf, sizeof(timeBuf),
    "Waktu: %04d-%02d-%02d %02d:%02d:%02d (%s)",
    now.year(), now.month(), now.day(),
    now.hour(), now.minute(), now.second(),
    days[now.dayOfTheWeek()]
  );
  logInfo(timeBuf);

  // Suhu internal DS3231
  float temp = rtc.getTemperature();
  char tempBuf[40];
  snprintf(tempBuf, sizeof(tempBuf), "Suhu DS3231: %.2f C", temp);
  logInfo(tempBuf);
}

// ============================================================
//  SET WAKTU RTC DARI STRING
//  Format: YYYYMMDDHHMMSS (14 karakter)
//  Contoh: 20260901183000 = 2026-09-01 18:30:00
// ============================================================
void setRTCFromString(const String& input) {
  if (input.length() != 14) {
    logError("Format salah! Harus 14 karakter: YYYYMMDDHHMMSS");
    logError("Contoh: 20260901183000 (2026-09-01 18:30:00)");
    return;
  }

  int yr = input.substring(0, 4).toInt();
  int mo = input.substring(4, 6).toInt();
  int dy = input.substring(6, 8).toInt();
  int hr = input.substring(8, 10).toInt();
  int mn = input.substring(10, 12).toInt();
  int sc = input.substring(12, 14).toInt();

  // Validasi
  if (yr < 2020 || yr > 2099) { logError("Tahun tidak valid: " + String(yr)); return; }
  if (mo < 1  || mo > 12)     { logError("Bulan tidak valid: " + String(mo)); return; }
  if (dy < 1  || dy > 31)     { logError("Hari tidak valid: " + String(dy)); return; }
  if (hr < 0  || hr > 23)     { logError("Jam tidak valid: " + String(hr)); return; }
  if (mn < 0  || mn > 59)     { logError("Menit tidak valid: " + String(mn)); return; }
  if (sc < 0  || sc > 59)     { logError("Detik tidak valid: " + String(sc)); return; }

  rtc.adjust(DateTime(yr, mo, dy, hr, mn, sc));

  char buf[60];
  snprintf(buf, sizeof(buf),
    "RTC di-set ke: %04d-%02d-%02d %02d:%02d:%02d",
    yr, mo, dy, hr, mn, sc
  );
  logInfo(buf);

  // Verifikasi: baca balik
  delay(200);
  DateTime verify = rtc.now();
  char vBuf[60];
  snprintf(vBuf, sizeof(vBuf),
    "Verifikasi  : %04d-%02d-%02d %02d:%02d:%02d",
    verify.year(), verify.month(), verify.day(),
    verify.hour(), verify.minute(), verify.second()
  );
  logInfo(vBuf);
}

// ============================================================
//  TAMPILKAN MENU
// ============================================================
void printMenu() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("  PERINTAH SERIAL MONITOR");
  Serial.println("========================================");
  Serial.println("  s              --> I2C Scanner");
  Serial.println("  t              --> Tampilkan waktu RTC");
  Serial.println("  S              --> Set RTC ke waktu compile");
  Serial.println("  YYYYMMDDHHMMSS --> Set RTC manual");
  Serial.println("  m              --> Menu ini");
  Serial.println("----------------------------------------");
  Serial.println("  Contoh set manual: 20260901183000");
  Serial.println("  = 2026-09-01 18:30:00");
  Serial.println("========================================");
  Serial.println();
}

// ============================================================
//  PROSES INPUT SERIAL
// ============================================================
String serialBuffer = "";

void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();

    if (c == '\n' || c == '\r') {
      serialBuffer.trim();

      if (serialBuffer.length() > 0) {

        if (serialBuffer == "s") {
          scanI2C();
        }
        else if (serialBuffer == "t") {
          printRTC();
        }
        else if (serialBuffer == "S") {
          logInfo("Set RTC ke waktu compile...");
          rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
          delay(200);
          printRTC();
        }
        else if (serialBuffer == "m") {
          printMenu();
        }
        else if (serialBuffer.length() == 14) {
          // Cek apakah semua karakter angka
          bool isNum = true;
          for (unsigned int i = 0; i < serialBuffer.length(); i++) {
            if (!isDigit(serialBuffer[i])) { isNum = false; break; }
          }
          if (isNum) {
            setRTCFromString(serialBuffer);
          } else {
            logError("Perintah tidak dikenal: " + serialBuffer);
            printMenu();
          }
        }
        else {
          logError("Perintah tidak dikenal: " + serialBuffer);
          printMenu();
        }

        serialBuffer = "";
      }
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
  Serial.println("  STEP 1: ESP8266 + DS3231 Test");
  Serial.println("  Build: " __DATE__ " " __TIME__);
  Serial.println("========================================");

  // Inisialisasi I2C dengan pin eksplisit
  Wire.begin(SDA_PIN, SCL_PIN);
  logInfo("I2C initialized");
  logInfo("SDA = GPIO" + String(SDA_PIN));
  logInfo("SCL = GPIO" + String(SCL_PIN));

  // Scan I2C untuk diagnosis awal
  scanI2C();

  // Inisialisasi RTC
  rtcFound = initRTC();
  if (rtcFound) {
    rtcReady = true;
    logInfo("RTC siap digunakan");
    printRTC();
  } else {
    logError("RTC gagal! Periksa wiring dan coba lagi.");
    logError("Sistem dalam mode diagnostik saja.");
  }

  printMenu();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  // Proses perintah dari Serial Monitor
  handleSerial();

  // Cetak waktu setiap 1 detik (non-blocking via millis)
  if (rtcReady) {
    unsigned long nowMs = millis();
    if (nowMs - lastPrintMs >= PRINT_INTERVAL) {
      lastPrintMs = nowMs;
      printRTC();
    }
  }
}

/*
 * ============================================================
 *  INSTALASI LIBRARY (Arduino IDE)
 * ============================================================
 *  Tools --> Manage Libraries --> cari:
 *
 *  1. "RTClib" by Adafruit    --> Install
 *  2. "Wire" sudah built-in di ESP8266 Arduino core
 *
 *  Board Manager:
 *  File --> Preferences --> Additional Board URLs:
 *  https://arduino.esp8266.com/stable/package_esp8266com_index.json
 *
 *  Tools --> Board --> ESP8266 Boards --> NodeMCU 1.0 (ESP-12E)
 *  Tools --> CPU Frequency --> 80 MHz
 *  Tools --> Upload Speed --> 115200
 *  Tools --> Port --> (pilih port COM ESP8266)
 * ============================================================
 *
 *  WIRING DIAGRAM
 * ============================================================
 *  DS3231 Pin    ESP8266 NodeMCU
 *  ----------    ----------------
 *  VCC      -->  3.3V
 *  GND      -->  GND
 *  SDA      -->  GPIO4   (label D2 di board, ABAIKAN label)
 *  SCL      -->  GPIO5   (label D1 di board, ABAIKAN label)
 *
 *  PENTING:
 *  - Gunakan 3.3V untuk VCC jika ragu
 *  - Level logic GPIO ESP8266 adalah 3.3V
 *  - Jika I2C tidak terdeteksi, coba pull-up 4.7k ohm
 *    dari SDA ke 3.3V dan SCL ke 3.3V
 * ============================================================
 *
 *  CHECKLIST TEST STEP 1
 * ============================================================
 *  [TEST-01] I2C Scanner menampilkan:
 *            "Device ditemukan di alamat 0x68  <-- DS3231 RTC"
 *
 *  [TEST-02] Waktu RTC terbaca setiap detik di Serial Monitor
 *            Format: Waktu: YYYY-MM-DD HH:MM:SS (Hari)
 *
 *  [TEST-03] Ketik "20260901183000" lalu Enter:
 *            RTC berhasil di-set ke 2026-09-01 18:30:00
 *            Verifikasi terbaca benar
 *
 *  Jika TEST-01 GAGAL:
 *   --> Cek kabel (SDA=GPIO4, SCL=GPIO5)
 *   --> Cek power VCC dan GND
 *   --> Tambahkan pull-up 4.7k ohm
 *
 *  Jika TEST-01 PASS tapi waktu aneh (2000-01-01):
 *   --> Ketik 'S' untuk set dari compile time
 *   --> Atau set manual dengan format YYYYMMDDHHMMSS
 *
 *  Setelah semua TEST PASS --> Lanjut ke STEP 2: Servo
 * ============================================================
 */
