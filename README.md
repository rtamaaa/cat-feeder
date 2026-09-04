# 🐱 Smart Cat Feeder Pro (IoT + Web Admin Portal) 🐾

Sistem otomatisasi pakan kucing pintar berbasis **NodeMCU ESP8266**, pewaktu presisi **DS3231 RTC**, katup pakan **Motor Servo SG90/MG996R**, **REST API Backend (PHP & MySQL)**, dan **Web Portal Administrator Modern**.

---

## 📁 Struktur Bersih & Terorganisir Proyek

```text
Pakan-kucing/
├── README.md                           # Panduan lengkap instalasi & deployment
├── PRD_SmartCatFeeder.md               # Product Requirements Document & arsitektur sistem
├── start_app.bat                       # Launcher 1-klik Web Dashboard di Windows
├── .gitignore                          # Filter git untuk file sementara & log
│
├── docs/                               # 📚 [DOKUMENTASI & DESAIN SISTEM]
│   ├── UML_DOCUMENTATION.md            # Dokumentasi UML (Use Case, Activity, Sequence, ERD, Class, Pinout)
│   ├── AAPANEL_DEPLOYMENT_GUIDE.md     # Panduan Deploy Backend ke VPS / aaPanel (SSL & Nginx)
│   └── plantuml/                       # File mentah diagram (.puml)
│
├── app/                                # 🚀 [PRODUCTION CORE APPS]
│   │
│   ├── frontend/                       # 🌐 Web Administrator Portal (HTML5 / Vanilla CSS / JS)
│   │   └── index.html                  # Dashboard Admin, Live Telemetry, Feed Controller & Log Table
│   │
│   ├── backend/                        # ⚙️ REST API Backend (PHP & MySQL)
│   │   ├── index.html                  # Web portal copy untuk root web server
│   │   ├── api/                        # 10 Endpoint REST API JSON
│   │   │   ├── auth.php                # Autentikasi token user / admin
│   │   │   ├── device_status.php       # Telemetri online/offline & feeding log
│   │   │   ├── servo_settings.php      # Kalibrasi sudut & delay servo + audit log
│   │   │   ├── schedule.php            # Manajemen slot jadwal dinamis (hingga 6 slot)
│   │   │   ├── heartbeat.php           # Heartbeat berkala ESP8266 ke server
│   │   │   ├── command_poll.php        # Polling antrean command oleh ESP8266
│   │   │   ├── command_result.php      # Laporan hasil eksekusi command oleh ESP8266
│   │   │   ├── feed.php                # Trigger pakan instan (FEED NOW)
│   │   │   ├── rtc.php                 # Sinkronisasi waktu RTC DS3231
│   │   │   └── wifi_set.php            # Pengaturan jarak jauh WiFi ESP8266
│   │   ├── config/
│   │   │   └── database.php            # Konfigurasi PDO MySQL, timezone, CORS & rate limiter
│   │   ├── database/
│   │   │   └── schema.sql              # Schema DDL lengkap 7 tabel database
│   │   ├── setup.php                   # Installer 1-klik database & tabel MySQL
│   │   └── .htaccess                   # Konfigurasi Apache & Authorization header
│   │
│   └── nodemcu/                        # 🤖 Firmware Production ESP8266 (Arduino C++)
│       └── nodemcu.ino                 # Firmware non-blocking: Polling, Heartbeat, LittleFS, RTC & Servo
│
└── steps/                              # 🧪 [MODULAR TESTING & PROTOTYPING]
    ├── step1_rtc_test/                 # Pengujian modul DS3231 RTC via I2C
    ├── step2_servo_test/               # Pengujian gerak servo non-blocking
    ├── step3_schedule_littlefs/        # Pengujian persistensi LittleFS Flash
    └── step4_wifi/                     # Pengujian AP Mode Provisioning (192.168.4.1)
```

---

## ⚡ Skema Rangkaian Hardware (Pinout)

| Komponen Hardware | Pin Modul | Pin NodeMCU ESP8266 | Deskripsi / Fungsi |
|---|---|---|---|
| **DS3231 RTC** | `SDA` | `D2` (GPIO 4) | Jalur Data Serial I2C |
| | `SCL` | `D1` (GPIO 5) | Jalur Clock Serial I2C |
| | `VCC` | `3.3V` / `5V` (Vin) | Catu Daya Modul RTC |
| | `GND` | `GND` | Ground Bersama |
| **Motor Servo (SG90 / MG996R)** | `Signal` (Kuning/Oranye) | `D5` (GPIO 14) | Sinyal Kontrol PWM Sudut Servo |
| | `VCC` (Merah) | `Vin` (5V eksternal) | Catu Daya Motor Servo |
| | `GND` (Cokelat/Hitam) | `GND` | Common Ground dengan ESP |

---

## 🚀 Panduan Menjalankan Sistem

### 1. Menjalankan Frontend Web Dashboard
* Cukup klik ganda file **`start_app.bat`** di folder utama, atau buka file:
  👉 **`app/frontend/index.html`** langsung di browser Anda.
* Dashboard otomatis terhubung ke cloud server: `https://catfeeder.tamamici.my.id`.

### 2. Backend REST API (Laragon / Web Server)
1. Buka Laragon atau server Apache + MySQL.
2. Tempatkan isi folder `app/backend` ke dalam web root:
   - Path Windows: `C:\laragon\www\smart-cat-feeder`
3. Akses installer otomatis di browser:
   👉 `http://localhost/smart-cat-feeder/setup.php`

### 3. Firmware NodeMCU ESP8266 (Arduino IDE)
1. Buka Arduino IDE > buka file `app/nodemcu/nodemcu.ino`.
2. Library yang dibutuhkan:
   - `RTClib` (by Adafruit)
   - `Servo` (bawaan ESP8266)
   - `ArduinoJson` (v6.x)
   - `LittleFS`
3. Pilih board **NodeMCU 1.0 (ESP-12E Module)** > klik **Upload**.
