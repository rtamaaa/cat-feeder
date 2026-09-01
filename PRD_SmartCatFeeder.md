# 🐱 Smart Cat Feeder — Product Requirements Document (PRD)

> **Versi:** 1.1  
> **Tanggal:** 01 September 2026  
> **Status:** Draft — Menunggu Review
> **Changelog v1.1:** Penambahan fitur WiFi Provisioning (AP Mode) dan ganti WiFi via Server Command

---

## 1. Ringkasan Eksekutif

Smart Cat Feeder adalah sistem IoT yang memungkinkan pemilik kucing mengontrol pemberian pakan secara otomatis dan manual dari lokasi mana saja melalui internet, tanpa bergantung pada cloud IoT pihak ketiga. Sistem dirancang agar alat tetap berfungsi memberikan pakan berdasarkan jadwal meskipun koneksi internet terputus.

---

## 2. Latar Belakang & Masalah

| Masalah | Dampak |
|---|---|
| Pemilik kucing tidak selalu berada di rumah | Kucing bisa kelaparan |
| Alat feeder murah tidak bisa dikontrol jarak jauh | Tidak fleksibel |
| Sistem cloud berbayar (Blynk, AWS IoT, dll.) | Biaya bulanan, ketergantungan vendor |
| Sistem MQTT memerlukan broker eksternal | Kompleksitas infrastruktur |
| IP ESP8266 tidak publik → server tidak bisa push | Firewall NAT di Rumah B |
| Konfigurasi WiFi ESP8266 harus hardcode di firmware | Tidak fleksibel saat ganti router/password |

**Solusi:** ESP8266 melakukan *polling* ke Home Server di Rumah A secara aktif, menggunakan REST API berbasis HTTPS. Server hanya menjadi perantara — tidak ada push langsung ke ESP8266.

---

## 3. Tujuan Produk

### Tujuan Utama
- Pemilik kucing dapat memberi makan dari jarak jauh (FEED NOW) melalui Android.
- Jadwal otomatis (07:00 & 18:00) harus berjalan meskipun internet mati.
- Sistem harus aman diakses dari internet publik.

### Tujuan Sekunder
- Monitoring status device secara real-time.
- Sinkronisasi waktu RTC dari Android.
- Riwayat pemberian pakan tersimpan di server.
- Logging offline ketika internet terputus, upload otomatis saat pulih.
- **Konfigurasi WiFi ESP8266 dari Android** tanpa perlu reflash firmware.
  - Setup awal via AP Mode (tanpa internet).
  - Ganti WiFi saat sudah online via Server Command.

---

## 4. Pemangku Kepentingan

| Peran | Lokasi | Interaksi |
|---|---|---|
| Pemilik Kucing | Rumah Z | Menggunakan Android |
| Alat Feeder (ESP8266) | Rumah B | Autonomous + polling server |
| Home Server | Rumah A | Perantara, REST API + DB |

---

## 5. Arsitektur Sistem

```
                         INTERNET
                            │
              ┌─────────────┴─────────────┐
              │                           │
              ▼                           ▼

       📱 RUMAH Z                    🏠 RUMAH A
       ANDROID                      HOME SERVER
       Python/Kivy                  PHP REST API
              │                     MySQL/MariaDB
              │ HTTPS                     │
              │                           │
              └──────────────┬────────────┘
                             │
                             ▼
                       🏠 RUMAH B
                       ESP8266
                           │
                  ┌────────┴────────┐
                  │                 │
               DS3231             SERVO
               RTC                Feeder
```

### Pola Komunikasi

```
Android ──HTTPS──▶ REST API Server ──▶ Database / Command Queue
                                              │
                              ESP8266 Polling ◀─────────────
                                    │
                                  Servo
```

> [!IMPORTANT]
> ESP8266 **tidak pernah** menerima koneksi masuk dari luar. Hanya ESP8266 yang melakukan koneksi keluar ke server.

---

## 6. Komponen Hardware

| Komponen | Spesifikasi | Peran |
|---|---|---|
| ESP8266 NodeMCU | MCU utama | Kontrol, polling, WiFi |
| DS3231 | RTC presisi tinggi | Sumber waktu utama feeding |
| Servo Motor | Standard servo | Buka/tutup dispenser pakan |

### Konfigurasi PIN

| Pin ESP8266 | Fungsi | Keterangan |
|---|---|---|
| GPIO4 | SDA (I2C) | DS3231 Data |
| GPIO5 | SCL (I2C) | DS3231 Clock |
| GPIO14 | Signal | Servo Motor |

> [!WARNING]
> Jangan gunakan istilah D1/D2 dalam kode maupun dokumentasi teknis. Gunakan selalu nomor GPIO eksplisit.

---

## 7. Komponen Software

### 7.1 Firmware ESP8266 (C++ Arduino)

**File:** `smart_cat_feeder.ino`

**Fitur:**
- Koneksi WiFi dengan auto-reconnect (non-blocking)
- I2C init dengan GPIO4/GPIO5
- DS3231 baca/tulis waktu
- Servo state machine (non-blocking via `millis()`)
- LittleFS — penyimpanan konfigurasi jadwal (JSON)
- LittleFS — offline queue feeding log
- HTTP polling GET `/api/device/commands`
- HTTP POST heartbeat, command result, feeding log
- Anti-duplicate command via `lastProcessedCommandId`
- Anti-duplicate feeding via `lastFeedDate` + `lastFeedSchedule`
- Serial logging dengan timestamp

### 7.2 Backend PHP (Home Server Rumah A)

**Struktur direktori:**

```
smart-cat-feeder/
├── api/
│   ├── auth.php              ← Helper autentikasi shared
│   ├── device_status.php     ← GET status device
│   ├── command_create.php    ← POST buat command baru
│   ├── command_poll.php      ← GET ESP8266 polling command
│   ├── command_result.php    ← POST ESP8266 lapor hasil
│   ├── schedule_get.php      ← GET jadwal dari DB
│   ├── schedule_save.php     ← POST simpan jadwal dari Android
│   ├── rtc_get.php           ← GET RTC saat ini
│   ├── rtc_set.php           ← POST set RTC (via command queue)
│   └── feeding_log.php       ← GET/POST log feeding
├── config/
│   └── database.php          ← Konfigurasi koneksi DB (PDO)
└── database/
    └── schema.sql            ← DDL lengkap
```

### 7.3 Database MySQL/MariaDB

**Tabel utama:**

| Tabel | Fungsi |
|---|---|
| `devices` | Registrasi device, API key, status |
| `commands` | Command queue (pending → processing → success/failed) |
| `schedules` | Konfigurasi jadwal feeding per device |
| `feeding_logs` | Riwayat semua feeding (manual & terjadwal) |

### 7.4 Android App (Python + Kivy)

**File:**

| File | Fungsi |
|---|---|
| `main.py` | Entry point, logika utama, threading |
| `api.py` | HTTP client wrapper (requests) |
| `storage.py` | Local storage konfigurasi Android |
| `ui.kv` | Deklarasi UI Kivy |
| `buildozer.spec` | Konfigurasi build APK |

---

## 8. Database Schema Detail

### Tabel `devices`
```sql
id           INT AUTO_INCREMENT PRIMARY KEY
device_id    VARCHAR(50) UNIQUE NOT NULL       -- "CAT_FEEDER_01"
device_name  VARCHAR(100)
api_key_hash VARCHAR(255)                      -- bcrypt/sha256
last_seen    DATETIME
status       ENUM('online','offline','unknown')
created_at   DATETIME
updated_at   DATETIME
```

### Tabel `commands`
```sql
id           INT AUTO_INCREMENT PRIMARY KEY
device_id    VARCHAR(50)
command      VARCHAR(50)                       -- FEED, SET_RTC, SET_SCHEDULE, SET_WIFI
payload      JSON                              -- data tambahan command
status       ENUM('pending','processing','success','failed')
created_at   DATETIME
executed_at  DATETIME
```

> [!NOTE]
> Command `SET_WIFI` hanya digunakan untuk **ganti WiFi** saat ESP8266 sudah online.
> Untuk **setup awal**, gunakan mekanisme AP Mode langsung ke ESP8266 (tanpa melalui server).

### Tabel `schedules`
```sql
id           INT AUTO_INCREMENT PRIMARY KEY
device_id    VARCHAR(50)
slot         INT                               -- 1 atau 2
enabled      TINYINT(1)
hour         INT
minute       INT
updated_at   DATETIME
```

### Tabel `feeding_logs`
```sql
id            INT AUTO_INCREMENT PRIMARY KEY
device_id     VARCHAR(50)
type          ENUM('manual','schedule')
schedule_slot INT NULL
status        ENUM('success','failed')
executed_at   DATETIME
synced        TINYINT(1) DEFAULT 0            -- sudah di-upload dari offline queue?
```

---

## 9. API Endpoints

### Device Endpoints (digunakan ESP8266)

| Method | Endpoint | Deskripsi |
|---|---|---|
| POST | `/api/device/heartbeat` | Kirim tanda hidup, update `last_seen` |
| GET | `/api/device/commands` | Polling command pending |
| POST | `/api/device/command-result` | Lapor hasil eksekusi command |
| POST | `/api/device/feeding-log` | Upload offline feeding log |
| GET | `/api/device/schedule` | Ambil jadwal terbaru dari server |

### Android Endpoints

| Method | Endpoint | Deskripsi |
|---|---|---|
| POST | `/api/feed` | Buat command FEED (Feed Now) |
| GET | `/api/device/status` | Cek status device (online/offline) |
| GET | `/api/schedule` | Ambil jadwal aktif |
| POST | `/api/schedule` | Simpan jadwal baru |
| POST | `/api/rtc/set` | Buat command SET_RTC |
| GET | `/api/rtc/get` | Ambil waktu RTC terakhir |
| GET | `/api/feeding-log` | Riwayat feeding |
| POST | `/api/wifi/set` | Buat command SET_WIFI (ganti WiFi via server) |

### ESP8266 AP Mode Endpoints (langsung, tanpa server)

> [!IMPORTANT]
> Endpoint ini hanya aktif saat ESP8266 dalam **mode AP** (setup awal).
> Android harus konek ke hotspot `CatFeeder-Setup` terlebih dahulu.
> Setelah konfigurasi selesai, mode AP dinonaktifkan otomatis.

| Method | Endpoint | Host | Deskripsi |
|---|---|---|---|
| GET | `/` | `192.168.4.1` | Halaman status AP mode |
| POST | `/wifi` | `192.168.4.1` | Kirim SSID + password WiFi rumah |
| GET | `/status` | `192.168.4.1` | Cek apakah sudah konek ke WiFi |

### Format Response Standar

**Success:**
```json
{
  "success": true,
  "message": "Command created",
  "data": { }
}
```

**Error:**
```json
{
  "success": false,
  "message": "Device not found",
  "error_code": "DEVICE_NOT_FOUND"
}
```

---

## 10. Alur End-to-End

### 10.1 Manual Feeding (Feed Now)

```
📱 Android (Rumah Z)
      │
      │ POST /api/feed  {device_id: "CAT_FEEDER_01"}
      ▼
🏠 Server (Rumah A)
      │
      │ INSERT commands (status=pending, command=FEED)
      ▼
🔄 ESP8266 polling setiap 3 detik
      │
      │ GET /api/device/commands → {command_id:123, command:"FEED"}
      ▼
⚙️  Servo buka → tunggu 1 detik → tutup
      │
      │ POST /api/device/command-result {command_id:123, status:"success"}
      ▼
🏠 Server update status = success
      │
📱 Android refresh → tampilkan ✅
```

### 10.2 Scheduled Feeding (Offline)

```
⏰ DS3231 → 07:00
      │
⚙️  ESP8266 checkSchedule()
      │
      │ Cocok jadwal 1 (07:00, enabled=true)
      │ lastFeedDate != today || lastFeedSchedule != 1
      ▼
⚙️  Servo buka → tunggu → tutup
      │
      │ Simpan log ke LittleFS (jika offline)
      ▼
🌐  Saat internet pulih → upload pending log ke server
```

### 10.3 Set RTC dari Android

```
📱 Android → POST /api/rtc/set {datetime: "2026-09-01 18:30:00"}
      ▼
🏠 Server → INSERT commands (SET_RTC, payload: {datetime: ...})
      ▼
⚙️  ESP8266 polling → terima SET_RTC
      ▼
⚙️  rtc.adjust(DateTime(...))
      ▼
⚙️  POST /api/device/command-result {status:"success", rtc:"2026-09-01 18:30:00"}
```

### 10.4 Sinkronisasi Jadwal

```
📱 Android → POST /api/schedule {slot:1, enabled:true, hour:7, minute:0}
      ▼
🏠 Server → UPDATE schedules + INSERT commands (SET_SCHEDULE)
      ▼
⚙️  ESP8266 polling → terima SET_SCHEDULE
      ▼
⚙️  Update konfigurasi JSON di LittleFS
      ▼
⚙️  POST /api/device/command-result {status:"success"}
```

### 10.5 Setup WiFi Awal via AP Mode

```
⚙️  ESP8266 nyala → tidak ada WiFi tersimpan di LittleFS
      │
      ▼
⚙️  ESP8266 masuk AP Mode
    SSID    : "CatFeeder-Setup"
    Password: "catfeeder123"
    IP      : 192.168.4.1
      │
📱 Android: user pilih "Setup Awal (AP Mode)"
      │
      │ User konek HP ke hotspot "CatFeeder-Setup"
      ▼
📱 Android → POST http://192.168.4.1/wifi
    { "ssid": "NamaWiFiRumahB", "password": "Passwordnya" }
      │
      ▼
⚙️  ESP8266 simpan ke LittleFS (wifi_config.json)
      │
      ▼
⚙️  Android polling GET http://192.168.4.1/status
    Tunggu konfirmasi "connected"
      │
      ▼
⚙️  ESP8266 keluar AP Mode → restart → konek ke WiFi Rumah B ✅
      │
📱 Android tampilkan: "✅ ESP8266 berhasil dikonfigurasi!"
   → Instruksikan user: konek kembali HP ke WiFi biasa
```

### 10.6 Ganti WiFi via Server (ESP8266 Sudah Online)

```
📱 Android → POST /api/wifi/set
    { "device_id": "CAT_FEEDER_01", "ssid": "WiFiBaru", "password": "PasswordBaru" }
      │
      ▼
🏠 Server → INSERT commands (command=SET_WIFI, payload: {ssid:..., password:...})
      │
      ▼
⚙️  ESP8266 polling → terima SET_WIFI
      │
      ▼
⚙️  Update wifi_config.json di LittleFS
      │
      ▼
⚙️  POST /api/device/command-result {status:"success"}
      │
      ▼
⚙️  ESP8266 reconnect ke WiFi baru
      │
📱 Android tampilkan: ✅ WiFi berhasil diperbarui
```

> [!WARNING]
> Untuk ganti WiFi via server, pastikan ESP8266 masih online saat command dikirim.
> Jika WiFi baru salah/tidak ada, ESP8266 akan fallback ke AP Mode otomatis setelah 3x gagal reconnect.

---

## 11. Mekanisme Keamanan

| Aspek | Implementasi |
|---|---|
| Transport | HTTPS (TLS/SSL wajib) |
| Auth Device | `device_id` + `device_token` header per request |
| Auth Android | Username + password → JWT token atau API key |
| DB Query | PDO + Prepared Statements (no SQL injection) |
| Input Validation | Semua input divalidasi di PHP sebelum DB |
| API Key Storage | Di-hash (SHA-256/bcrypt) di database |
| Rate Limiting | Sederhana via counter per IP per menit |

> [!CAUTION]
> Jangan simpan `device_token` atau password database di APK Android dalam bentuk plaintext. Gunakan konfigurasi runtime atau secure storage.

---

## 12. Mekanisme Anti-Failure

### Anti Duplicate Command
- ESP8266 menyimpan `lastProcessedCommandId` di RAM
- Sebelum eksekusi command, cek apakah `command_id == lastProcessedCommandId`
- Server mengubah status command menjadi `processing` saat dikirim → tidak akan dikirim ulang

### Anti Duplicate Feeding (per jadwal)
```
lastFeedDate     = "2026-09-01"
lastFeedSchedule = 1

Saat 07:00:
  if today == lastFeedDate && schedule == lastFeedSchedule → SKIP
  else → FEED + update lastFeedDate + lastFeedSchedule
```

### Fail-Safe Kondisi

| Kondisi | Respons Sistem |
|---|---|
| Internet Rumah B mati | Jadwal lokal tetap jalan via DS3231 + LittleFS |
| Server Rumah A mati | ESP8266 retry polling dengan backoff, jadwal tetap jalan |
| RTC gagal | Tolak feeding berbasis jadwal, log error |
| Servo sedang aktif | Tolak command FEED kedua (`isFeedingInProgress` flag) |
| WiFi terputus | Auto-reconnect non-blocking, jadwal tetap jalan |
| WiFi baru salah (SET_WIFI) | Retry 3x → fallback ke AP Mode otomatis |
| AP Mode: password WiFi salah | Android cek `/status` → tampilkan error, user input ulang |

---

## 13. UI Android

```
╔══════════════════════════════════╗
║       🐱 SMART CAT FEEDER        ║
╠══════════════════════════════════╣
║                                  ║
║ Device: CAT_FEEDER_01            ║
║                                  ║
║ Status: 🟢 ONLINE                ║
║ Last seen: 2 detik lalu          ║
║                                  ║
║ RTC: 18:32:15                    ║
║      01/09/2026                  ║
║                                  ║
║ ┌────────────────────────────┐   ║
║ │       🍖 FEED NOW          │   ║
║ └────────────────────────────┘   ║
║                                  ║
║ Jadwal 1                         ║
║ [ ON ]  07 : 00                  ║
║                                  ║
║ Jadwal 2                         ║
║ [ ON ]  18 : 00                  ║
║                                  ║
║ [ SIMPAN JADWAL ]                ║
║                                  ║
║ [ SET RTC ]                      ║
║                                  ║
║ Feeding terakhir:                ║
║ 18:00:02 - Otomatis              ║
╠══════════════════════════════════╣
║  ⚙️  PENGATURAN WIFI ESP8266     ║
╠══════════════════════════════════╣
║                                  ║
║ Mode:                            ║
║ ( ) Setup Awal (AP Mode)         ║
║ ( ) Ganti WiFi (Via Server)      ║
║                                  ║
║ SSID WiFi Rumah B:               ║
║ [____________________________]   ║
║                                  ║
║ Password:                        ║
║ [____________________________]   ║
║                                  ║
║ [ 📡 KIRIM KE ESP8266 ]          ║
║                                  ║
║ ℹ️  Mode AP: konek HP ke         ║
║    "CatFeeder-Setup" dulu        ║
╚══════════════════════════════════╝
```

### Status Device Logic
| Kondisi `last_seen` | Status Tampilan |
|---|---|
| < 30 detik | 🟢 ONLINE |
| 30 – 120 detik | 🟡 WARNING |
| > 120 detik | 🔴 OFFLINE |

---

## 14. Pilihan Akses Internet Server (Rumah A)

| Opsi | Deskripsi | Keamanan | Kemudahan |
|---|---|---|---|
| **A** | Public IP + port forwarding + Let's Encrypt HTTPS | ⭐⭐⭐ | ⭐⭐⭐⭐ |
| **B** | DDNS (No-IP/DuckDNS) + port forwarding + HTTPS | ⭐⭐⭐ | ⭐⭐⭐ |
| **C** | Cloudflare Tunnel (Zero Trust) — *Direkomendasikan* | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |
| **D** | WireGuard/Tailscale VPN | ⭐⭐⭐⭐⭐ | ⭐⭐ |

> [!TIP]
> **Opsi C (Cloudflare Tunnel)** adalah pilihan terbaik untuk home server karena:
> - Tidak perlu IP publik statis
> - Tidak perlu port forwarding
> - HTTPS otomatis dengan domain custom
> - DDoS protection gratis
> - ESP8266 Rumah B **tidak terekspos sama sekali**

---

## 15. Logging & Monitoring

### ESP8266 Serial Log Format
```
[HH:MM:SS] [LEVEL] Message
[18:30:00] [INFO] WiFi connected — IP: 192.168.1.105
[18:30:01] [INFO] Server reachable
[18:30:02] [INFO] Heartbeat sent
[18:31:00] [INFO] Command received: FEED (ID:123)
[18:31:00] [INFO] Servo OPEN
[18:31:01] [INFO] Servo CLOSE
[18:31:01] [INFO] Feeding SUCCESS
[18:31:01] [INFO] Result reported to server
```

### Log Levels
| Level | Kondisi |
|---|---|
| `[INFO]` | Operasi normal |
| `[WARN]` | Kondisi tidak ideal (WiFi retry, server timeout) |
| `[ERROR]` | Kegagalan kritis (RTC tidak terdeteksi, servo gagal) |

---

## 16. Rencana Pengujian

### Hardware Tests
| ID | Test | Ekspektasi |
|---|---|---|
| TEST-01 | DS3231 detected via I2C | `0x68 detected` di Serial |
| TEST-02 | RTC read | Waktu valid terbaca |
| TEST-03 | RTC set | Waktu berubah sesuai input |
| TEST-04 | Servo open/close | Bergerak 0° → 90° → 0° |
| TEST-05 | Manual feeding | Servo aktif 1 detik |
| TEST-06 | Scheduled feeding | Servo aktif tepat di jam jadwal |

### Network Tests
| ID | Test | Ekspektasi |
|---|---|---|
| TEST-07 | ESP8266 internet | IP terdapat, ping berhasil |
| TEST-08 | ESP8266 polling | GET `/api/device/commands` → 200 |
| TEST-09 | Android → server | POST `/api/feed` → 200 |
| TEST-10 | Server → ESP8266 | Command diterima saat polling |
| TEST-11 | ESP8266 → server result | POST result → command status = success |

### Failure Tests
| ID | Test | Ekspektasi |
|---|---|---|
| TEST-12 | Internet Rumah B mati | Jadwal tetap jalan, log disimpan lokal |
| TEST-13 | Server Rumah A mati | Polling retry, jadwal tetap jalan |
| TEST-14 | Android offline | Tampilkan pesan error di UI |
| TEST-15 | Duplicate command | Command kedua diabaikan |
| TEST-16 | RTC lost power | Log error, feeding berbasis jadwal diblokir |

---

## 17. Tahapan Implementasi

| Step | Komponen | Deliverable |
|---|---|---|
| **1** | DS3231 + ESP8266 | RTC terbaca di Serial Monitor |
| **2** | Servo + GPIO14 | Servo bergerak via Serial command |
| **3** | LittleFS + Jadwal | Jadwal tersimpan & feeding otomatis |
| **4** | WiFi ESP8266 | ESP8266 terhubung internet |
| **5** | PHP + MySQL | API endpoints bisa diakses via browser/curl |
| **6** | ESP8266 polling | ESP8266 bisa ambil command dari server |
| **7** | Feed Now via API | FEED command dari curl → servo bergerak |
| **8** | Sinkronisasi RTC | SET_RTC via API → DS3231 berubah |
| **9** | Sinkronisasi jadwal | SET_SCHEDULE via API → LittleFS update |
| **10** | Android Kivy | UI berfungsi, bisa connect ke server |
| **11** | Remote control | Feed Now dari Android Rumah Z berhasil |
| **12** | HTTPS + Auth | Semua endpoint dilindungi token |
| **13** | Offline queue | Log offline berhasil di-upload |
| **14** | End-to-end test | Semua TEST-01 s/d TEST-16 pass |

---

## 18. Konfigurasi Kunci

### ESP8266
```cpp
const char* WIFI_SSID        = "...";
const char* WIFI_PASSWORD    = "...";
const char* API_BASE_URL     = "https://feeder.example.com";
const char* DEVICE_ID        = "CAT_FEEDER_01";
const char* DEVICE_TOKEN     = "...";

const int   SERVO_PIN        = 14;    // GPIO14
const int   SERVO_CLOSE      = 0;     // derajat
const int   SERVO_OPEN       = 90;    // derajat
const int   FEED_DURATION    = 1000;  // ms

const int   POLL_INTERVAL    = 3000;  // ms
const int   HEARTBEAT_INTERVAL = 10000; // ms
```

### Android
```python
API_BASE_URL  = "https://feeder.example.com"
DEVICE_ID     = "CAT_FEEDER_01"
ANDROID_TOKEN = "..."   # token login Android
REFRESH_INTERVAL = 5    # detik
```

---

## 19. Definisi Done (Definition of Done)

Sistem dianggap selesai ketika:

- [ ] Pemilik di Rumah Z dapat menekan FEED NOW dan servo di Rumah B bergerak dalam < 10 detik.
- [ ] Jadwal 07:00 dan 18:00 berjalan otomatis tanpa intervensi.
- [ ] Ketika internet Rumah B diputus, jadwal tetap berjalan.
- [ ] Log feeding offline ter-upload ke server saat internet pulih.
- [ ] Tidak ada feeding ganda pada menit yang sama.
- [ ] Tidak ada command yang dieksekusi dua kali.
- [ ] Semua komunikasi menggunakan HTTPS.
- [ ] Semua TEST-01 s/d TEST-16 lulus.
- [ ] RTC dapat disinkronisasi dari Android.
- [ ] Jadwal dapat diubah dari Android dan langsung aktif di ESP8266.

---

## 20. Batasan & Asumsi

| Batasan/Asumsi | Keterangan |
|---|---|
| Server Rumah A memiliki akses internet publik | Diperlukan untuk akses dari Rumah Z dan Rumah B |
| ESP8266 terhubung ke WiFi Rumah B | Diperlukan untuk polling server |
| DS3231 memiliki baterai backup | Agar waktu tidak hilang saat ESP8266 mati |
| Servo kompatibel dengan 3.3V/5V logic | NodeMCU output GPIO 3.3V |
| PHP 7.4+ dan MySQL 5.7+ / MariaDB 10.4+ di server | Kompatibilitas PDO dan JSON |
| Android minimal versi 5.0 (Lollipop) | Target buildozer |
| Tidak menggunakan Blynk, MQTT, atau cloud IoT pihak ketiga | Sesuai requirement |

---

*Dokumen ini akan diperbarui seiring progress implementasi.*
