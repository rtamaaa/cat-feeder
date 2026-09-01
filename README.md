# 🐱 Smart Cat Feeder Pro (IoT + Android App) 🐾

Sistem otomatisasi pakan kucing berbasis **NodeMCU ESP8266**, modul RTC presisi **DS3231**, motor servo **SG90 / MG996R**, **REST API Backend (PHP & MySQL)**, dan **Aplikasi Android / Desktop (Python + Kivy)**.

---

## 📁 Struktur Folder Project

```text
Pakan-kucing/
├── README.md                           # Panduan lengkap instalasi & deployment
├── PRD_SmartCatFeeder.md               # Product Requirements Document & arsitektur sistem
├── start_app.bat                       # Launcher 1-klik aplikasi di Windows
├── .gitignore                          # Filter git untuk build artifacts & log
│
├── docs/                               # 📚 [DOKUMENTASI & DESAIN SISTEM]
│   ├── UML_DOCUMENTATION.md            # Dokumentasi UML (Use Case, Activity, Sequence, ERD, Class)
│   ├── AAPANEL_DEPLOYMENT_GUIDE.md     # Panduan Deploy Backend ke VPS / aaPanel (SSL & Nginx)
│   └── plantuml/                       # File mentah diagram (.puml)
│
├── app/                                # 🚀 [PRODUCTION CORE APPS]
│   ├── frontend/                       # 📱 Aplikasi Android / Desktop (Python Kivy)
│   │   ├── main.py                     # Kode utama UI (Bottom Nav, Left Drawer, Emoticon support)
│   │   ├── requirements.txt            # Library Python (kivy, requests, certifi, urllib3)
│   │   └── buildozer.spec              # Konfigurasi build APK Android (API 33 ready)
│   │
│   ├── backend/                        # 🌐 REST API Backend (PHP & MySQL)
│   │   ├── api/                        # Endpoint REST API JSON
│   │   │   ├── auth.php                # Autentikasi token user Android
│   │   │   ├── device_status.php       # Status online/offline & feeding history
│   │   │   ├── servo_settings.php      # Get/Set sudut & delay servo, serta audit logs
│   │   │   ├── schedule.php            # Manajemen jadwal dinamis (hingga 6 slot)
│   │   │   ├── heartbeat.php           # Heartbeat dari ESP8266 ke server
│   │   │   ├── command_poll.php        # Polling antrean command oleh ESP8266
│   │   │   ├── command_result.php      # Laporan hasil eksekusi command oleh ESP8266
│   │   │   ├── feed.php                # Trigger pakan instan (FEED NOW)
│   │   │   ├── rtc.php                 # Sinkronisasi waktu RTC DS3231
│   │   │   └── wifi_set.php            # Pengaturan remote WiFi ESP8266
│   │   ├── config/
│   │   │   └── database.php            # Konfigurasi PDO MySQL, timezone, CORS & rate limiter
│   │   ├── database/
│   │   │   └── schema.sql              # Schema DDL lengkap 6 tabel & seed data awal
│   │   ├── setup.php                   # Installer 1-klik database & tabel MySQL
│   │   └── .htaccess                   # Header & security rewrite
│   │
│   └── nodemcu/                        # 🤖 Firmware Production ESP8266 (Arduino IDE)
│       └── nodemcu.ino                 # Polling command, heartbeat, dynamic servo, LittleFS, RTC
│
└── steps/                              # 🧪 [MODULAR TESTING & PROTOTYPING]
    ├── step1_rtc_test/                 # Uji coba pembacaan waktu modul DS3231 RTC via I2C
    │   └── step1_rtc_test.ino
    ├── step2_servo_test/               # Uji coba pergerakan Servo non-blocking
    │   └── step2_servo_test.ino
    ├── step3_schedule_littlefs/        # Uji coba persistensi jadwal di chip flash LittleFS
    │   └── step3_schedule_littlefs.ino
    └── step4_wifi/                     # Uji coba AP Mode Setup (192.168.4.1) & Auto Reconnect
        └── step4_wifi.ino
```

---

## ⚡ Skema Rangkaian Hardware (Pinout)

| Komponen | Pin Modul | Pin NodeMCU ESP8266 | Keterangan |
|---|---|---|---|
| **DS3231 RTC** | SDA | `D2` (GPIO 4) | Komunikasi I2C Data |
| | SCL | `D1` (GPIO 5) | Komunikasi I2C Clock |
| | VCC | `3.3V` / `5V` (Vin) | Daya RTC |
| | GND | `GND` | Ground |
| **Servo Motor** | Signal (Kuning/Oranye) | `D5` (GPIO 14) | PWM Control |
| | VCC (Merah) | `Vin` (5V eksternal) | Daya Servo |
| | GND (Cokelat/Hitam) | `GND` | Common Ground dengan ESP |

---

## 🚀 Panduan Deployment & Instalasi

### 1. Backend REST API (Laragon / Web Server)
1. Buka Laragon atau server Apache + MySQL Anda.
2. Tempatkan isi folder `app/backend` ke dalam web root:
   - Path Windows: `C:\laragon\www\smart-cat-feeder`
3. Buka browser dan jalankan setup otomatis:
   👉 `http://localhost/smart-cat-feeder/setup.php`
4. Konfigurasi default di `app/backend/config/database.php`:
   - Database: `smart_cat_feeder`
   - User: `catfeeder_user` (atau `root`)
   - Pass: `(sesuaikan dengan passwordmu)`
   - Timezone: `Asia/Jakarta` (WIB)

### 2. Firmware NodeMCU ESP8266 (Arduino IDE)
1. Buka Arduino IDE.
2. Buka file:
   👉 `app/nodemcu/nodemcu.ino`
3. Pastikan library terpasang:
   - `RTClib` (by Adafruit)
   - `Servo` (bawaan ESP8266)
   - `ArduinoJson` (v6.x)
   - `LittleFS`
4. Sesuaikan `SERVER_HOST` dengan IP Laptop/Server Anda (contoh: `192.168.0.101`).
5. Upload ke board **NodeMCU 1.0 (ESP-12E Module)**.

### 3. Aplikasi Frontend Mobile / Desktop (Python + Kivy)
#### Menjalankan di Komputer / Laptop:
1. Pastikan Python 3.9 - 3.13 terpasang.
2. Install dependensi:
   ```bash
   pip install -r app/frontend/requirements.txt
   ```
3. Jalankan aplikasi:
   - Klik ganda file `start_app.bat`, atau
   - Ketik di terminal:
     ```bash
     python app/frontend/main.py
     ```
4. Login default:
   - **Username:** `admin`
   - **Password:** `bukalah11`

#### Build Menjadi File APK Android:
Gunakan Linux / WSL / Google Colab dengan tool Buildozer:
```bash
cd app/frontend
buildozer -v android debug
```
File APK siap instal akan berada di folder `app/frontend/bin/`.

---

## 📋 Fitur Utama Sistem

- 🍖 **FEED NOW (Instan):** Kasih pakan kucing langsung dari aplikasi secara *real-time*.
- ⏰ **Jadwal Dinamis:** Tambah/hapus hingga 6 slot jadwal makan otomatis (tersimpan di LittleFS ESP8266 & dijalankan mandiri oleh RTC DS3231 walau internet terputus).
- ⚙️ **Kalibrasi Servo Fleksibel:** Pengaturan sudut tutup (`0°-180°`), sudut buka (`0°-180°`), dan durasi delay porsi pakan langsung dari slider aplikasi.
- 📜 **Audit Trail & Riwayat:** Log pemberian pakan dan riwayat perubahan kalibrasi servo tercatat lengkap dengan timestamp.
- 🕒 **Sinkronisasi RTC:** Selaraskan jam modul RTC DS3231 ke waktu jam HP hanya dengan 1 klik.
- 📡 **Manajemen Wi-Fi Ganda:** Mode AP Hotspot (`CatFeeder-Setup` @ `192.168.4.1`) untuk konfigurasi awal + Mode Remote untuk ganti Wi-Fi jarak jauh via server.
- 🎨 **UI/UX Modern:** Tema Dark Glassmorphism, Bottom Navigation Bar, Left Drawer, dan dukungan ikon emoji penuh.
