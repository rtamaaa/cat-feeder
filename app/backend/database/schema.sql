-- ============================================================
--  SMART CAT FEEDER -- Database Schema
--  MySQL / MariaDB
--  STEP 5: Backend REST API
-- ============================================================

CREATE DATABASE IF NOT EXISTS smart_cat_feeder
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

USE smart_cat_feeder;

-- ============================================================
--  TABLE: devices
--  Registrasi setiap alat feeder
-- ============================================================
CREATE TABLE IF NOT EXISTS devices (
  id           INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  device_id    VARCHAR(50)  NOT NULL UNIQUE,
  device_name  VARCHAR(100) NOT NULL DEFAULT '',
  api_key_hash VARCHAR(255) NOT NULL,        -- SHA-256 dari device token
  last_seen    DATETIME     NULL,
  rtc_time     DATETIME     NULL,
  status       ENUM('online','offline','unknown') NOT NULL DEFAULT 'unknown',
  created_at   DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at   DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  INDEX idx_device_id (device_id)
) ENGINE=InnoDB;

ALTER TABLE devices ADD COLUMN IF NOT EXISTS rtc_time DATETIME NULL AFTER last_seen;

-- ============================================================
--  TABLE: commands
--  Queue command dari Android ke ESP8266
--  ESP8266 polling tabel ini setiap beberapa detik
-- ============================================================
CREATE TABLE IF NOT EXISTS commands (
  id          INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  device_id   VARCHAR(50)  NOT NULL,
  command     VARCHAR(50)  NOT NULL,         -- FEED, SET_RTC, SET_SCHEDULE, SET_WIFI
  payload     JSON         NULL,             -- data tambahan (opsional)
  status      ENUM('pending','processing','success','failed') NOT NULL DEFAULT 'pending',
  created_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  executed_at DATETIME     NULL,
  INDEX idx_device_status (device_id, status),
  INDEX idx_created (created_at),
  FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- ============================================================
--  TABLE: schedules
--  Konfigurasi jadwal feeding per device
-- ============================================================
CREATE TABLE IF NOT EXISTS schedules (
  id         INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  device_id  VARCHAR(50) NOT NULL,
  slot       TINYINT UNSIGNED NOT NULL,      -- 1 atau 2
  enabled    TINYINT(1) NOT NULL DEFAULT 1,
  hour       TINYINT UNSIGNED NOT NULL,      -- 0-23
  minute     TINYINT UNSIGNED NOT NULL,      -- 0-59
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  UNIQUE KEY uq_device_slot (device_id, slot),
  FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- ============================================================
--  TABLE: feeding_logs
--  Riwayat semua feeding (manual & terjadwal)
--  Termasuk upload dari offline queue ESP8266
-- ============================================================
CREATE TABLE IF NOT EXISTS feeding_logs (
  id            INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  device_id     VARCHAR(50)  NOT NULL,
  type          ENUM('manual','schedule') NOT NULL,
  schedule_slot TINYINT UNSIGNED NULL,       -- NULL jika manual
  status        ENUM('success','failed')    NOT NULL DEFAULT 'success',
  executed_at   DATETIME     NOT NULL,
  synced        TINYINT(1)   NOT NULL DEFAULT 1,  -- 0 = dari offline queue
  created_at    DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_device_time (device_id, executed_at),
  FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- ============================================================
--  TABLE: device_settings
--  Pengaturan dinamis servo (sudut buka, tutup, delay)
-- ============================================================
CREATE TABLE IF NOT EXISTS device_settings (
  id            INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  device_id     VARCHAR(50) NOT NULL UNIQUE,
  close_angle   SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  open_angle    SMALLINT UNSIGNED NOT NULL DEFAULT 90,
  duration_ms   INT UNSIGNED NOT NULL DEFAULT 2000,
  updated_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- ============================================================
--  TABLE: settings_logs
--  Audit trail riwayat perubahan setting servo
-- ============================================================
CREATE TABLE IF NOT EXISTS settings_logs (
  id            INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  device_id     VARCHAR(50) NOT NULL,
  close_angle   SMALLINT UNSIGNED NOT NULL,
  open_angle    SMALLINT UNSIGNED NOT NULL,
  duration_ms   INT UNSIGNED NOT NULL,
  changed_by    VARCHAR(50) NOT NULL DEFAULT 'system',
  created_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_device_time (device_id, created_at),
  FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- ============================================================
--  TABLE: android_users
--  Autentikasi user Android
-- ============================================================
CREATE TABLE IF NOT EXISTS android_users (
  id            INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  username      VARCHAR(50)  NOT NULL UNIQUE,
  password_hash VARCHAR(255) NOT NULL,       -- password_hash() PHP
  api_token     VARCHAR(64)  NULL,           -- token sesi aktif
  token_expires DATETIME     NULL,
  device_id     VARCHAR(50)  NOT NULL,       -- device yang bisa dikontrol
  created_at    DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_username (username),
  INDEX idx_token (api_token)
) ENGINE=InnoDB;

-- ============================================================
--  DATA AWAL (SEED)
-- ============================================================

-- Device CAT_FEEDER_01
-- device_token: catfeeder_secret_token_2026
-- api_key_hash = SHA256("catfeeder_secret_token_2026")
INSERT IGNORE INTO devices (device_id, device_name, api_key_hash, status)
VALUES (
  'CAT_FEEDER_01',
  'Cat Feeder Rumah B',
  SHA2('catfeeder_secret_token_2026', 256),
  'unknown'
);

-- Jadwal default: 07:00 dan 18:00
INSERT IGNORE INTO schedules (device_id, slot, enabled, hour, minute)
VALUES
  ('CAT_FEEDER_01', 1, 1, 7,  0),
  ('CAT_FEEDER_01', 2, 1, 18, 0);

-- Pengaturan servo default: Tutup 0 deg, Buka 90 deg, Delay 2000 ms
INSERT IGNORE INTO device_settings (device_id, close_angle, open_angle, duration_ms)
VALUES ('CAT_FEEDER_01', 0, 90, 2000);

-- User Android default
-- username: admin
-- password: admin123 (ganti setelah setup!)
INSERT IGNORE INTO android_users (username, password_hash, device_id)
VALUES (
  'admin',
  '$2y$10$placeholder_change_this_hash_in_production_xxxxx',
  'CAT_FEEDER_01'
);

-- ============================================================
--  CATATAN KEAMANAN
-- ============================================================
-- 1. Ganti device_token di firmware ESP8266 dan di sini
-- 2. Ganti password admin setelah instalasi pertama
--    Gunakan: php -r "echo password_hash('password_baru', PASSWORD_DEFAULT);"
-- 3. Batasi akses MySQL hanya dari localhost jika server dan
--    PHP di mesin yang sama
-- ============================================================
