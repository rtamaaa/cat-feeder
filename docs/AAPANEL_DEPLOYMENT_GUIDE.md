# 🚀 Panduan Lengkap Deploy Backend ke aaPanel (VPS / Cloud Server)

Dokumen ini berisi panduan *step-by-step* untuk mendeploy backend REST API **Smart Cat Feeder Pro** (`app/backend`) ke server VPS yang menggunakan **aaPanel** (Nginx/Apache + PHP + MySQL/MariaDB).

---

## 📋 Daftar Isi
1. [Prasyarat Server & Lingkungan](#1-prasyarat-server--lingkungan)
2. [Langkah 1: Tambah Website Baru di aaPanel](#langkah-1-tambah-website-baru-di-aapanel)
3. [Langkah 2: Buat Database & Import Schema](#langkah-2-buat-database--import-schema)
4. [Langkah 3: Upload Source Code Backend](#langkah-3-upload-source-code-backend)
5. [Langkah 4: Konfigurasi File database.php](#langkah-4-konfigurasi-file-databasephp)
6. [Langkah 5: Pasang SSL (HTTPS) & Konfigurasi Web Server](#langkah-5-pasang-ssl-https--konfigurasi-web-server)
7. [Langkah 6: Test Endpoint REST API](#langkah-6-test-endpoint-rest-api)
8. [Langkah 7: Update Firmware NodeMCU ESP8266](#langkah-7-update-firmware-nodemcu-esp8266)
9. [Langkah 8: Update Aplikasi Android Kivy](#langkah-8-update-aplikasi-android-kivy)
10. [Troubleshooting & Solusi Error](#troubleshooting--solusi-error)

---

## 1. Prasyarat Server & Lingkungan

Pastikan server VPS Anda telah terinstall aaPanel dengan paket berikut di menu **App Store**:
- **Web Server:** Nginx (v1.20+) atau Apache (v2.4+)
- **PHP:** PHP 7.4, 8.0, 8.1, atau 8.2
  - Ekstensi PHP wajib aktif: `pdo_mysql`, `json`, `curl`, `mbstring`, `openssl`
- **Database:** MySQL 5.7+ atau MariaDB 10.5+
- **phpMyAdmin:** (Opsional, untuk mempermudah kelola database)

---

## Langkah 1: Tambah Website Baru di aaPanel

1. Buka dashboard **aaPanel** (contoh: `http://IP-VPS:8888`).
2. Masuk ke menu **Website** > klik tombol **Add site**.
3. Isi form pembuatan site:
   - **Domain:** Masukkan domain / subdomain Anda (contoh: `feeder.namadomain.com`) atau gunakan IP Public VPS jika belum memiliki domain.
   - **Description:** `Smart Cat Feeder Backend API`
   - **Root Directory:** Default `/www/wwwroot/feeder.namadomain.com`
   - **FTP:** Tidak perlu (bisa upload via File Manager).
   - **Database:** Pilih **MySQL** (bisa sekaligus dibuat di sini, atau buat di Langkah 2).
   - **PHP Version:** Pilih `PHP-8.1` atau `PHP-7.4`.
4. Klik **Submit**.

---

## Langkah 2: Buat Database & Import Schema

1. Buka menu **Databases** di sidebar aaPanel.
2. Jika belum dibuat, klik **Add Database**:
   - **DB Name:** `smart_cat_feeder`
   - **DB User:** `catfeeder_user` (atau generate otomatis)
   - **Password:** Masukkan password yang kuat (contoh: `PassFeeder2026!#`)
   - **Access Permission:** Pilih `Localhost (127.0.0.1)`
   - Klik **Submit**.
3. Klik tombol **Import** pada database yang baru dibuat > pilih file:
   👉 **[`app/backend/database/schema.sql`](file:///c:/Users/LENOVO/Desktop/Pakan-kucing/app/backend/database/schema.sql)**
4. Klik **Upload** lalu klik **Import**.
5. *Alternatif:* Buka **phpMyAdmin** > pilih database `smart_cat_feeder` > menu **Import** > pilih `schema.sql` > klik **Go**.

---

## Langkah 3: Upload Source Code Backend

1. Di komputer lokal Anda, kompres isi folder **`app/backend/`** menjadi file **`.zip`** (pastikan folder `api/`, `config/`, `database/`, `.htaccess`, `setup.php` ada di dalam zip).
2. Di dashboard aaPanel, masuk ke menu **Files** > buka direktori:
   👉 `/www/wwwroot/feeder.namadomain.com/`
3. Hapus file default bawaan (`index.html`, `404.html`).
4. Klik tombol **Upload** > pilih file `.zip` yang tadi dikompres.
5. Klik kanan file `.zip` di aaPanel > pilih **Uncompress**.
6. Pastikan struktur folder di `/www/wwwroot/feeder.namadomain.com/` menjadi:
   ```text
   /www/wwwroot/feeder.namadomain.com/
   ├── api/
   │   ├── auth.php
   │   ├── device_status.php
   │   ├── feed.php
   │   ├── heartbeat.php
   │   ├── command_poll.php
   │   ├── command_result.php
   │   ├── schedule.php
   │   ├── servo_settings.php
   │   ├── rtc.php
   │   └── wifi_set.php
   ├── config/
   │   └── database.php
   ├── database/
   │   └── schema.sql
   ├── setup.php
   └── .htaccess
   ```
7. Set permission direktori:
   - Pilih semua folder > klik **Permission** > set ke `755` dengan Owner `www:www`.

---

## Langkah 4: Konfigurasi File database.php

1. Buka file `/www/wwwroot/feeder.namadomain.com/config/database.php` via editor aaPanel.
2. Sesuaikan kredensial koneksi database:
   ```php
   define('DB_HOST', '127.0.0.1');
   define('DB_PORT', 3306);
   define('DB_NAME', 'smart_cat_feeder');
   define('DB_USER', 'catfeeder_user');      // Sesuaikan user DB aaPanel
   define('DB_PASS', 'PassFeeder2026!#');     // Sesuaikan password DB aaPanel
   define('DB_CHARSET', 'utf8mb4');
   
   // Timezone WIB (Jakarta)
   date_default_timezone_set('Asia/Jakarta');
   ```
3. Klik **Save**.

---

## Langkah 5: Pasang SSL (HTTPS) & Konfigurasi Web Server

### A. Pasang SSL Gratis (Let's Encrypt):
1. Masuk ke menu **Website** > klik nama domain Anda.
2. Pilih tab **SSL** > klik tab **Let's Encrypt**.
3. Centang nama domain Anda > klik tombol **Apply**.
4. Aktifkan sakelar **Force HTTPS** (Otomatis redirect HTTP ke HTTPS).

### B. Konfigurasi Nginx (CORS & Header Support):
Jika menggunakan **Nginx**, buka tab **Config** website Anda di aaPanel dan pastikan header Authorization diteruskan ke PHP:
```nginx
location / {
    try_files $uri $uri/ /index.php?$query_string;
}

location ~ \.php$ {
    try_files $uri =404;
    fastcgi_pass unix:/tmp/php-cgi-81.sock; # Sesuaikan versi PHP Anda
    fastcgi_index index.php;
    include fastcgi.conf;
    
    # Meneruskan header Bearer Token & Device Headers
    fastcgi_param HTTP_AUTHORIZATION $http_authorization;
    fastcgi_param HTTP_X_DEVICE_ID $http_x_device_id;
    fastcgi_param HTTP_X_DEVICE_TOKEN $http_x_device_token;
}
```

---

## Langkah 6: Test Endpoint REST API

Buka browser atau gunakan cURL / Postman untuk memverifikasi deployment:

### 1. Test Login API:
```bash
curl -X POST "https://feeder.namadomain.com/api/auth.php?action=login" \
     -H "Content-Type: application/json" \
     -d '{"username": "admin", "password": "bukalah11"}'
```
**Respon yang Diharapkan (`200 OK`):**
```json
{
  "success": true,
  "message": "Login berhasil",
  "data": {
    "token": "d8218f9d629954a37d3f9fb5d9c...",
    "expires_at": "2026-09-08 12:00:00",
    "username": "admin",
    "device_id": "CAT_FEEDER_01"
  }
}
```

### 2. Test Device Status API:
```bash
curl -X GET "https://feeder.namadomain.com/api/device_status.php?action=status" \
     -H "Authorization: Bearer <TOKEN_DARI_LOGIN>"
```

---

## Langkah 7: Update Firmware NodeMCU ESP8266

Buka file **[`app/nodemcu/nodemcu.ino`](file:///c:/Users/LENOVO/Desktop/Pakan-kucing/app/nodemcu/nodemcu.ino)** di Arduino IDE:

1. Ubah konfigurasi server dari IP lokal ke domain server produksi Anda:
   ```cpp
   // ============================================================
   //  KONFIGURASI SERVER PRODUKSI
   // ============================================================
   const char* SERVER_HOST = "feeder.namadomain.com"; // Domain atau IP VPS
   const int   SERVER_PORT = 443;                     // 443 jika HTTPS, 80 jika HTTP
   const char* BASE_PATH   = "/api";                  // Path API di server
   ```
2. Jika menggunakan **HTTPS (Port 443)** pada ESP8266:
   - Gunakan `WiFiClientSecure client;`
   - Tambahkan `client.setInsecure();` agar ESP8266 dapat berkomunikasi tanpa perlu menyimpan sertifikat root CA manual.
3. Hubungkan NodeMCU ke PC via kabel Micro USB > Pilih Board `NodeMCU 1.0 (ESP-12E Module)` > Klik **Upload**.

---

## Langkah 8: Buka & Uji Web Administrator Portal

1. Buka browser di PC, Laptop, atau Smartphone Anda:
   👉 `https://feeder.namadomain.com/` (atau `https://catfeeder.tamamici.my.id/`)
2. Masuk menggunakan akun admin Anda:
   - **Username:** `admin` (atau username terdaftar Anda)
   - **Password:** `[password_anda]`
3. Periksa panel dashboard:
   - Indikator status perangkat harus menunjukkan **🟢 ONLINE**.
   - Coba lakukan uji coba pakan instan dengan menekan tombol **🍖 KASIH PAKAN SEKARANG**.

---

## 🔧 Troubleshooting & Solusi Error

| Masalah | Penyebab | Solusi |
|---|---|---|
| **Error 401: `Missing authorization token`** | Nginx tidak mem-forward header `Authorization` ke PHP. | Buka tab **Config** website di aaPanel, tambahkan `fastcgi_param HTTP_AUTHORIZATION $http_authorization;` lalu restart Nginx. |
| **Error 404: `Not Found` pada endpoint `/api/...`** | File berada di sub-folder yang salah. | Pastikan folder `api/` berada tepat di dalam root direktori website aaPanel (`/www/wwwroot/domain.com/api/`). |
| **Error 500: `Database connection failed`** | Kredensial di `config/database.php` salah atau MySQL service mati. | Cek status service MySQL di aaPanel dan pastikan `DB_USER` & `DB_PASS` sama persis dengan yang dibuat di menu Databases. |
| **ESP8266 Gagal Konek (HTTP -1)** | Firewall VPS memblokir port atau SSL certificate mismatch. | Buka menu **Security** di aaPanel > Buka port `80` dan `443`. Pada firmware ESP8266 pastikan memanggil `client.setInsecure()`. |
| **Error 429: `Too many requests`** | Rate limit terpicu karena polling berlebihan. | Atur nilai `rateLimit(60, 60)` di `app/backend/config/database.php`. |
