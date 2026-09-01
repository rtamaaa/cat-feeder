# 📚 Dokumentasi Teknis & Desain UML: Smart Cat Feeder Pro

Dokumen ini berisi spesifikasi perancangan sistem **Smart Cat Feeder Pro** secara komprehensif menggunakan diagram standar **UML (Unified Modeling Language)** dan **PlantUML**.

---

## 📑 Daftar Isi Diagram
1. [🏛️ Desain Arsitektur Sistem (System Architecture & Deployment)](#1-desain-arsitektur-sistem)
2. [👥 Use Case Diagram](#2-use-case-diagram)
3. [🔄 Activity Diagram (Alur Kerja Pakan & Kalibrasi)](#3-activity-diagram)
4. [⏱️ Sequence Diagram (Interaksi End-to-End & Polling)](#4-sequence-diagram)
5. [🗄️ Entity Relationship Diagram (ERD Database)](#5-entity-relationship-diagram-erd)
6. [🧱 Class & Component Diagram](#6-class--component-diagram)
7. [⚙️ State Machine Diagram (Firmware Lifecycle)](#7-state-machine-diagram)

---

## 1. 🏛️ Desain Arsitektur Sistem
Diagram arsitektur menggambarkan pembagian 3 tier utama:
- **Client Tier:** Aplikasi Android/Desktop berbasis Python & Kivy dengan custom UI (BottomNav + Drawer).
- **Server Tier:** Backend REST API berbasis PHP PDO & Database MySQL (Laragon / Web Server).
- **IoT Hardware Tier:** NodeMCU ESP8266 dengan modul RTC DS3231 (I2C) & Motor Servo SG90/MG996R (PWM).

```plantuml
@startuml System_Architecture
!theme plain
skinparam backgroundColor #FFFFFF
skinparam componentStyle uml2

node "Mobile / Desktop Client" as ClientNode {
    [Aplikasi Android (Kivy / Python)] as KivyApp
    component "UI Layer\n(BottomNav + Drawer)" as UILayer
    component "API Service\n(Requests HTTP)" as HttpService
    
    KivyApp --> UILayer
    UILayer --> HttpService
}

node "Server Backend (Apache / Laragon)" as ServerNode {
    package "REST API Services (PHP)" as ApiPackage {
        [auth.php] as ApiAuth
        [device_status.php] as ApiStatus
        [feed.php] as ApiFeed
        [schedule.php] as ApiSched
        [servo_settings.php] as ApiServo
        [command_poll.php] as ApiPoll
        [command_result.php] as ApiResult
        [heartbeat.php] as ApiHeartbeat
        [rtc.php] as ApiRtc
        [wifi_set.php] as ApiWifi
    }
    
    database "MySQL / MariaDB\n(smart_cat_feeder)" as DatabaseNode {
        [devices]
        [device_settings]
        [settings_logs]
        [schedules]
        [feeding_logs]
        [commands]
        [android_users]
    }
    
    ApiPackage --> DatabaseNode : PDO Database Connection
}

node "IoT Hardware Device (Cat Feeder)" as HardwareNode {
    package "NodeMCU ESP8266 Firmware" as EspPackage {
        [WiFi Manager & HTTP Client] as EspWifi
        [Command Polling Engine] as EspPoll
        [Schedule Checker Engine] as EspSchedEngine
        [LittleFS Flash Storage] as EspFS
        [Servo State Machine] as EspServoSM
    }
    
    node "Sensors & Actuators" as ActuatorNode {
        [DS3231 Real Time Clock (I2C)] as RtcHw
        [SG90 / MG996R Servo Motor] as ServoHw
    }
    
    EspSchedEngine --> RtcHw : I2C (SDA: D2 / SCL: D1)
    EspServoSM --> ServoHw : PWM Signal (D5 / GPIO14)
    EspPackage --> EspFS : Read / Write Config (JSON)
}

HttpService --> ApiPackage : HTTP / JSON (Port 80)\n[Bearer Token Auth]
EspWifi --> ApiPackage : HTTP / JSON (Polling 5s, Heartbeat 10s)\n[Device Header Auth]
@enduml
```

---

## 2. 👥 Use Case Diagram
Menggambarkan interaksi antara **Pemilik Kucing (User)**, **NodeMCU ESP8266 (Device)**, dan **Modul DS3231 RTC (Timer)** terhadap fitur-fitur sistem.

```plantuml
@startuml UseCase_Diagram
!theme plain
skinparam backgroundColor #FFFFFF
skinparam actorStyle awesome

left to right direction

actor "Pemilik Kucing (User)" as User
actor "NodeMCU ESP8266 (Device)" as Device
actor "Modul DS3231 RTC (Timer)" as RTC

rectangle "Smart Cat Feeder System" {
    package "Autentikasi & Pengaturan Akun" {
        usecase "UC01: Login ke Aplikasi" as UC_Login
        usecase "UC02: Logout dari Aplikasi" as UC_Logout
    }
    
    package "Kontrol & Monitoring Pakan" {
        usecase "UC03: Beri Pakan Manual (Feed Now)" as UC_FeedNow
        usecase "UC04: Monitoring Status Online & Waktu RTC" as UC_Status
        usecase "UC05: Lihat Riwayat Log Pakan" as UC_History
    }
    
    package "Manajemen Jadwal Otomatis" {
        usecase "UC06: Lihat Daftar Jadwal" as UC_ViewSched
        usecase "UC07: Tambah Slot Jadwal Baru" as UC_AddSched
        usecase "UC08: Ubah Waktu / Toggle Status Jadwal" as UC_EditSched
        usecase "UC09: Hapus Slot Jadwal" as UC_DelSched
        usecase "UC10: Eksekusi Pakan Terjadwal (Otonom)" as UC_ExecSched
    }
    
    package "Kalibrasi Servo & Delay" {
        usecase "UC11: Atur Sudut Tutup (Close Angle)" as UC_SetClose
        usecase "UC12: Atur Sudut Buka (Open Angle)" as UC_SetOpen
        usecase "UC13: Atur Durasi Terbuka (Feed Delay)" as UC_SetDelay
        usecase "UC14: Lihat Riwayat Kalibrasi Servo" as UC_ServoLogs
    }
    
    package "Konektivitas & Sinkronisasi" {
        usecase "UC15: Sinkronisasi Jam HP ke DS3231 RTC" as UC_SyncRTC
        usecase "UC16: Konfigurasi Wi-Fi Mode AP (192.168.4.1)" as UC_APWifi
        usecase "UC17: Ganti Wi-Fi Jarak Jauh (Mode Server)" as UC_SrvWifi
        usecase "UC18: Kirim Heartbeat & Poll Command" as UC_HeartbeatPoll
    }
}

User --> UC_Login
User --> UC_Logout
User --> UC_FeedNow
User --> UC_Status
User --> UC_History
User --> UC_ViewSched
User --> UC_AddSched
User --> UC_EditSched
User --> UC_DelSched
User --> UC_SetClose
User --> UC_SetOpen
User --> UC_SetDelay
User --> UC_ServoLogs
User --> UC_SyncRTC
User --> UC_APWifi
User --> UC_SrvWifi

Device --> UC_HeartbeatPoll
Device --> UC_ExecSched
RTC --> UC_ExecSched

UC_AddSched ..> UC_ViewSched : <<include>>
UC_EditSched ..> UC_ViewSched : <<include>>
UC_DelSched ..> UC_ViewSched : <<include>>
@enduml
```

---

## 3. 🔄 Activity Diagram
Menggambarkan alur aktivitas pemberian pakan baik secara **Manual (Feed Now)** maupun **Otomatis Berbasis Waktu RTC DS3231**.

```plantuml
@startuml Activity_Diagram
!theme plain
skinparam backgroundColor #FFFFFF

title Activity Diagram: Alur Eksekusi Pakan (Manual & Jadwal Otomatis)

|User (Android App)|
start
:Buka Aplikasi & Login;
if (Pilih Aksi?) then (Feed Now Manual)
    :Klik tombol "FEED NOW";
    :Kirim request POST /api/feed.php;
    |Server Backend (PHP & MySQL)|
    :Validasi Token Bearer & Device ID;
    :Insert baris command 'FEED' (status: pending);
    :Kirim respon 200 OK ke Android;
    |User (Android App)|
    :Tampilkan status "Menunggu respon alat...";
    
    |NodeMCU ESP8266|
    :Loop Polling (setiap 5 detik);
    :Kirim GET /api/command_poll.php;
    |Server Backend (PHP & MySQL)|
    :Cari command pending terlama;
    :Ubah status menjadi 'processing';
    :Return data command ID & jenis 'FEED';
    |NodeMCU ESP8266|
    :Terima perintah 'FEED';
else (Pakan Terjadwal Otomatis)
    |NodeMCU ESP8266|
    :Baca Waktu Sekarang dari Modul DS3231 RTC;
    :Cek apakah jam & menit cocok dengan slot jadwal LittleFS;
    if (Waktu Cocok & Belum Diberi Pakan Menit Ini?) then (Ya)
        :Set trigger pakan terjadwal;
    else (Tidak)
        stop
    endif
endif

|NodeMCU ESP8266|
:Mulai Non-Blocking Servo State Machine;
:Gerakkan Servo ke Sudut Buka (Open Angle);
:Tunggu durasi pakan (Delay Durasi ms);
:Gerakkan Servo ke Sudut Tutup (Close Angle);
:Simpan log eksekusi pakan ke LittleFS (/feedlog.json);

if (Terhubung ke Jaringan Wi-Fi?) then (Ya)
    :Kirim POST /api/command_result.php (status: success);
    :Kirim POST /api/device_status.php sync log;
    |Server Backend (PHP & MySQL)|
    :Update tabel commands (status: success, executed_at: NOW);
    :Insert baris baru ke tabel feeding_logs;
else (Offline)
    |NodeMCU ESP8266|
    :Antrekan log pakan di flash LittleFS;
    :Sync log saat koneksi internet pulih kembali;
endif

|User (Android App)|
:Refresh otomatis status & riwayat pakan;
:Tampilkan badge "✅ Pakan Berhasil Diberikan";
stop
@enduml
```

---

## 4. ⏱️ Sequence Diagram
Menggambarkan urutan pertukaran pesan antar aktor dan komponen saat perintah pakan instan dikirimkan.

```plantuml
@startuml Sequence_Diagram
!theme plain
skinparam backgroundColor #FFFFFF
autonumber

actor "User" as User
participant "Android App\n(Kivy)" as App
participant "Backend API\n(PHP)" as Api
database "MySQL\nDatabase" as DB
participant "NodeMCU ESP8266\n(Firmware)" as ESP
participant "DS3231 RTC\n(I2C Sensor)" as RTC
participant "Servo SG90\n(Actuator)" as Servo

== 1. Autentikasi Pengguna ==
User -> App : Masukkan Username & Password
App -> Api : POST /api/auth.php?action=login\n{username, password}
Api -> DB : SELECT * FROM android_users WHERE username = ?
DB --> Api : Return hash password & user data
Api -> Api : Verify password_hash & generate API Token
Api -> DB : UPDATE android_users SET api_token, expires_at
Api --> App : 200 OK {token, expires_at, device_id}
App -> App : Simpan sesi ke session.json & Buka Dashboard

== 2. Pengiriman Perintah Pakan (Feed Now) ==
User -> App : Klik Tombol "🍖 FEED NOW"
App -> Api : POST /api/feed.php\nHeader: Bearer <token>\nBody: {device_id: "CAT_FEEDER_01"}
Api -> DB : SELECT status FROM devices WHERE device_id = ?
Api -> DB : INSERT INTO commands (device_id, command, status)\nVALUES ('CAT_FEEDER_01', 'FEED', 'pending')
DB --> Api : Return command_id: 31
Api --> App : 200 OK {success: true, command_id: 31}
App --> User : Update status: "Menunggu alat mengeksekusi..."

== 3. Polling Perintah oleh ESP8266 ==
loop Setiap 5 Detik (Non-blocking Timer)
    ESP -> Api : GET /api/command_poll.php\nHeaders: X-Device-Id, X-Device-Token
    Api -> DB : SELECT * FROM commands WHERE device_id = ? AND status = 'pending' LIMIT 1
    DB --> Api : Return row (command_id: 31, command: 'FEED')
    Api -> DB : UPDATE commands SET status = 'processing' WHERE id = 31
    Api --> ESP : 200 OK {command_id: 31, command: 'FEED'}
end

== 4. Eksekusi Pergerakan Servo ==
ESP -> RTC : Baca waktu real-time (I2C)
RTC --> ESP : 2026-09-01 12:30:00
ESP -> Servo : write(servoOpenAngle: 90°)
ESP -> ESP : Non-blocking delay (feedDurationMs: 2000ms)
ESP -> Servo : write(servoCloseAngle: 0°)
ESP -> ESP : Catat log ke LittleFS (/feedlog.json)

== 5. Pelaporan Status Eksekusi (Result Callback) ==
ESP -> Api : POST /api/command_result.php\nBody: {command_id: 31, status: "success", executed_at: "2026-09-01 12:30:02"}
Api -> DB : UPDATE commands SET status = 'success', executed_at = NOW() WHERE id = 31
Api -> DB : INSERT INTO feeding_logs (device_id, type, status, executed_at)\nVALUES ('CAT_FEEDER_01', 'manual', 'success', NOW())
Api --> ESP : 200 OK {success: true}

== 6. Update Real-Time Tampilan Aplikasi ==
App -> Api : GET /api/device_status.php?action=status
Api -> DB : Query status device, last_seen, last_feeding
DB --> Api : Data status terbaru
Api --> App : 200 OK {status: "online", last_feeding: {...}}
App --> User : Tampilkan badge "✅ Pakan Berhasil Diberikan"
@enduml
```

---

## 5. 🗄️ Entity Relationship Diagram (ERD)
Struktur entitas database relasional `smart_cat_feeder` di MySQL / MariaDB.

```plantuml
@startuml ERD_Diagram
!theme plain
skinparam backgroundColor #FFFFFF
skinparam linetype ortho

entity "devices" as dev {
  * **id** : INT <<PK, AUTO_INCREMENT>>
  --
  * **device_id** : VARCHAR(50) <<UK>>
  * device_name : VARCHAR(100)
  * api_key_hash : VARCHAR(255)
  last_seen : DATETIME
  status : ENUM('online','offline','unknown')
  * created_at : DATETIME
  * updated_at : DATETIME
}

entity "android_users" as usr {
  * **id** : INT <<PK, AUTO_INCREMENT>>
  --
  * **username** : VARCHAR(50) <<UK>>
  * password_hash : VARCHAR(255)
  api_token : VARCHAR(64)
  token_expires : DATETIME
  * device_id : VARCHAR(50) <<FK>>
  * created_at : DATETIME
}

entity "device_settings" as stg {
  * **id** : INT <<PK, AUTO_INCREMENT>>
  --
  * **device_id** : VARCHAR(50) <<FK, UK>>
  * close_angle : SMALLINT UNSIGNED
  * open_angle : SMALLINT UNSIGNED
  * duration_ms : INT UNSIGNED
  * updated_at : DATETIME
}

entity "settings_logs" as slog {
  * **id** : INT <<PK, AUTO_INCREMENT>>
  --
  * device_id : VARCHAR(50) <<FK>>
  * close_angle : SMALLINT UNSIGNED
  * open_angle : SMALLINT UNSIGNED
  * duration_ms : INT UNSIGNED
  * changed_by : VARCHAR(50)
  * created_at : DATETIME
}

entity "schedules" as sch {
  * **id** : INT <<PK, AUTO_INCREMENT>>
  --
  * device_id : VARCHAR(50) <<FK>>
  * slot : TINYINT UNSIGNED
  * enabled : TINYINT(1)
  * hour : TINYINT UNSIGNED
  * minute : TINYINT UNSIGNED
  * updated_at : DATETIME
}

entity "commands" as cmd {
  * **id** : INT <<PK, AUTO_INCREMENT>>
  --
  * device_id : VARCHAR(50) <<FK>>
  * command : VARCHAR(50)
  payload : JSON
  * status : ENUM('pending','processing','success','failed')
  * created_at : DATETIME
  executed_at : DATETIME
}

entity "feeding_logs" as flog {
  * **id** : INT <<PK, AUTO_INCREMENT>>
  --
  * device_id : VARCHAR(50) <<FK>>
  * type : ENUM('manual','schedule')
  schedule_slot : TINYINT UNSIGNED
  * status : ENUM('success','failed')
  * executed_at : DATETIME
  * synced : TINYINT(1)
  * created_at : DATETIME
}

dev ||--o{ usr : "1 to N"
dev ||--|| stg : "1 to 1"
dev ||--o{ slog : "1 to N"
dev ||--o{ sch : "1 to N"
dev ||--o{ cmd : "1 to N"
dev ||--o{ flog : "1 to N"
@enduml
```

---

## 6. 🧱 Class & Component Diagram
Struktur kelas object-oriented pada aplikasi Frontend Python Kivy dan Firmware ESP8266.

```plantuml
@startuml Class_Diagram
!theme plain
skinparam backgroundColor #FFFFFF
skinparam classAttributeIconSize 0

package "Kivy Mobile Frontend (app/frontend/main.py)" {
    class SmartCatFeederApp {
        + build() : RootLayout
    }
    class RootLayout {
        - _open : Boolean
        + open_drawer() : void
        + close_drawer() : void
        + refresh_page() : void
        + logout() : void
    }
    class LeftDrawer {
        + username_label : String
        + navigate(screen_name: String) : void
        + do_logout() : void
    }
    class MainScreen {
        + active_tab : String
        + init(server_url, token, username, root_ref) : void
        + go_to(target: String) : void
    }
    class BasePage {
        + root_ref : RootLayout
        + on_enter() : void
        + refresh() : void
        # _get(path, cb, **params) : void
        # _post(path, payload, cb) : void
    }
    class PageDash {
        + send_feed() : void
        + sync_rtc() : void
        + go_wifi() : void
    }
    class PageSched {
        + add_slot() : void
        + save_all() : void
    }
    class PageServo {
        + save_settings(ca, oa, dur) : void
        + load_history() : void
    }
    class PageHist {
        + load() : void
    }
    class PageWifi {
        + set_mode(ap: Boolean) : void
        + send() : void
    }

    SmartCatFeederApp --> RootLayout
    RootLayout *-- LeftDrawer
    RootLayout *-- MainScreen
    MainScreen *-- BasePage
    BasePage <|-- PageDash
    BasePage <|-- PageSched
    BasePage <|-- PageServo
    BasePage <|-- PageHist
    BasePage <|-- PageWifi
}

package "NodeMCU ESP8266 Firmware (app/nodemcu/nodemcu.ino)" {
    class FeederFirmware <<ESP8266 Engine>> {
        - servoCloseAngle : int = 0
        - servoOpenAngle : int = 90
        - feedDurationMs : int = 2000
        - feedingState : FeedingState
        + setup() : void
        + loop() : void
        + handleServerPolling() : void
        + handleHeartbeat() : void
        + handleScheduleCheck() : void
        + handleServoStateMachine() : void
        + startFeeding(slot: int) : void
    }
    enum FeedingState {
        IDLE
        OPENING
        HOLDING
        CLOSING
        SETTLING
    }
    FeederFirmware *-- FeedingState
}
@enduml
```

---

## 7. ⚙️ State Machine Diagram
Siklus pergerakan servo non-blocking dan penanganan konektivitas Wi-Fi (AP Hotspot vs Client Station).

```plantuml
@startuml State_Machine_Diagram
!theme plain
skinparam backgroundColor #FFFFFF

state "Servo Feeding State Machine" as ServoSM {
    [*] --> FEED_IDLE
    FEED_IDLE --> FEED_OPENING : startFeeding(slot)\n[Trigger Manual / Jadwal Cocok]
    FEED_OPENING : Entry: servoMoveTo(servoOpenAngle)
    FEED_OPENING --> FEED_HOLDING : setelah SERVO_SETTLE (400ms)
    FEED_HOLDING : Katup terbuka penuh (makanan mengalir)
    FEED_HOLDING --> FEED_CLOSING : setelah feedDurationMs (misal 2000ms)
    FEED_CLOSING : Entry: servoMoveTo(servoCloseAngle)
    FEED_CLOSING --> FEED_SETTLING : setelah SERVO_SETTLE (400ms)
    FEED_SETTLING : Simpan riwayat ke LittleFS & update status
    FEED_SETTLING --> FEED_IDLE : Selesai
}

state "WiFi Connectivity State Machine" as WifiSM {
    [*] --> STATE_CONNECTING : Boot / Restart
    STATE_CONNECTING : Coba koneksi ke SSID tersimpan (Timeout 20s)
    STATE_CONNECTING --> STATE_CONNECTED : Sukses Dapat IP LAN
    STATE_CONNECTING --> STATE_AP_MODE : Gagal / Kredensial Kosong
    
    STATE_CONNECTED : Heartbeat (10s), Polling (5s), Sync Log (2m)
    STATE_AP_MODE : Hotspot 'CatFeeder-Setup' @ 192.168.4.1
    
    STATE_CONNECTED --> STATE_CONNECTING : WiFi Terputus
    STATE_AP_MODE --> STATE_CONNECTING : Kredensial Baru Diterima -> Simpan & Restart
}
@enduml
```

---

## 🛠️ Cara Render / Ekspor Diagram PlantUML
File-file diagram individual berekstensi `.puml` tersimpan di direktori:
👉 **[`docs/plantuml/`](file:///c:/Users/LENOVO/Desktop/Pakan-kucing/docs/plantuml/)**

Untuk mengonversi diagram menjadi file gambar PNG/SVG:
1. **Online:** Copy-paste kode ke [PlantUML Online Server](https://www.plantuml.com/plantuml/uml/).
2. **VS Code:** Gunakan ekstensi *PlantUML* (oleh Jebbs) dan tekan `Alt + D` untuk preview langsung.
3. **CLI:** Jalankan `java -jar plantuml.jar docs/plantuml/*.puml`.
