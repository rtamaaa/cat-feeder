<?php
// ============================================================
//  SMART CAT FEEDER -- Setup / Instalasi Pertama
//  Akses: http://server-kamu/smart-cat-feeder/setup.php
//
//  HAPUS file ini setelah instalasi selesai!
// ============================================================

// Kunci akses setup (ganti sebelum upload)
define('SETUP_KEY', 'setup_secret_2026');

if (($_GET['key'] ?? '') !== SETUP_KEY) {
    http_response_code(403);
    die('Akses ditolak. Tambahkan ?key=setup_secret_2026 ke URL.');
}

require_once __DIR__ . '/config/database.php';

$action  = $_POST['action'] ?? 'form';
$message = '';
$msgType = '';

// ---- AKSI: Generate Password Hash ----
if ($action === 'genhash') {
    $password = $_POST['password'] ?? '';
    if (!empty($password)) {
        $hash = password_hash($password, PASSWORD_DEFAULT);
        $message = "Hash untuk password <strong>" . htmlspecialchars($password) . "</strong>:<br><code>$hash</code>";
        $msgType = 'success';
    }
}

// ---- AKSI: Update Admin Password ----
if ($action === 'setpassword') {
    $username = trim($_POST['username'] ?? 'admin');
    $password = $_POST['new_password'] ?? '';
    $confirm  = $_POST['confirm_password'] ?? '';

    if ($password !== $confirm) {
        $message = 'Password tidak cocok!';
        $msgType = 'error';
    } elseif (strlen($password) < 6) {
        $message = 'Password minimal 6 karakter!';
        $msgType = 'error';
    } else {
        try {
            $db   = getDB();
            $hash = password_hash($password, PASSWORD_DEFAULT);
            $st   = $db->prepare('UPDATE android_users SET password_hash = ? WHERE username = ?');
            $st->execute([$hash, $username]);

            if ($st->rowCount() > 0) {
                $message = "Password untuk user <strong>$username</strong> berhasil diperbarui!";
                $msgType = 'success';
            } else {
                $message = "User '$username' tidak ditemukan.";
                $msgType = 'error';
            }
        } catch (Exception $e) {
            $message = 'Database error: ' . htmlspecialchars($e->getMessage());
            $msgType = 'error';
        }
    }
}

// ---- AKSI: Test Koneksi Database ----
if ($action === 'testdb') {
    try {
        $db  = getDB();
        $st  = $db->query('SELECT COUNT(*) as cnt FROM devices');
        $row = $st->fetch();
        $message = "Koneksi database OK! Jumlah device: <strong>{$row['cnt']}</strong>";
        $msgType = 'success';
    } catch (Exception $e) {
        $message = 'Koneksi GAGAL: ' . htmlspecialchars($e->getMessage());
        $msgType = 'error';
    }
}

// ---- AKSI: Migrasi & Fix Status ----
if ($action === 'migrate') {
    try {
        $db = getDB();
        try {
            $db->exec('ALTER TABLE devices ADD COLUMN rtc_time DATETIME NULL AFTER last_seen');
        } catch (Exception $ex) {
            // Kolom mungkin sudah ada
        }
        $db->exec('UPDATE devices SET status = "online", last_seen = NOW(), updated_at = NOW() WHERE device_id = "CAT_FEEDER_01"');
        $message = "Migrasi sukses! Kolom rtc_time telah dipastikan ada dan status CAT_FEEDER_01 telah diaktifkan.";
        $msgType = 'success';
    } catch (Exception $e) {
        $message = 'Error migrasi: ' . htmlspecialchars($e->getMessage());
        $msgType = 'error';
    }
}

// ---- AKSI: Jalankan Schema SQL ----
if ($action === 'runschema') {
    try {
        $db  = getDB();
        $sql = file_get_contents(__DIR__ . '/database/schema.sql');
        // Split per statement
        $statements = array_filter(
            array_map('trim', explode(';', $sql)),
            fn($s) => !empty($s) && !str_starts_with(ltrim($s), '--')
        );

        $count = 0;
        foreach ($statements as $stmt) {
            if (!empty($stmt)) {
                $db->exec($stmt);
                $count++;
            }
        }
        $message = "Schema berhasil dijalankan! ($count statement dieksekusi)";
        $msgType = 'success';
    } catch (Exception $e) {
        $message = 'Error menjalankan schema: ' . htmlspecialchars($e->getMessage());
        $msgType = 'error';
    }
}

?><!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Cat Feeder Setup</title>
<style>
body { font-family: Arial, sans-serif; max-width: 700px; margin: 30px auto; padding: 20px; background: #f5f5f5; }
h1 { color: #333; border-bottom: 2px solid #4CAF50; padding-bottom: 10px; }
h2 { color: #555; margin-top: 30px; }
.card { background: #fff; padding: 20px; border-radius: 8px; margin: 15px 0; box-shadow: 0 2px 4px rgba(0,0,0,.1); }
.success { background: #d4edda; border: 1px solid #c3e6cb; padding: 12px; border-radius: 5px; color: #155724; margin: 10px 0; }
.error   { background: #f8d7da; border: 1px solid #f5c6cb; padding: 12px; border-radius: 5px; color: #721c24; margin: 10px 0; }
input, select { width: 100%; padding: 8px; margin: 6px 0 12px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
button { background: #4CAF50; color: #fff; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; }
button:hover { background: #45a049; }
button.danger { background: #dc3545; }
code { background: #eee; padding: 4px 8px; border-radius: 3px; word-break: break-all; display: block; margin: 8px 0; }
.warning { background: #fff3cd; border: 1px solid #ffc107; padding: 12px; border-radius: 5px; color: #856404; margin: 10px 0; }
</style>
</head>
<body>
<h1>Cat Feeder Setup</h1>

<div class="warning">
  <strong>PERINGATAN:</strong> Hapus file <code>setup.php</code> ini setelah instalasi selesai!
</div>

<?php if ($message): ?>
<div class="<?= $msgType ?>"><?= $message ?></div>
<?php endif; ?>

<!-- Test DB -->
<div class="card">
  <h2>1. Test Koneksi Database</h2>
  <form method="POST">
    <input type="hidden" name="action" value="testdb">
    <button type="submit">Test Koneksi</button>
  </form>
</div>

<!-- Run Schema -->
<div class="card">
  <h2>2. Jalankan Schema SQL</h2>
  <p>Buat tabel dan data awal (aman dijalankan berkali-kali -- menggunakan IF NOT EXISTS).</p>
  <form method="POST">
    <input type="hidden" name="action" value="runschema">
    <button type="submit">Jalankan Schema</button>
  </form>
</div>

<!-- Set Password Admin -->
<div class="card">
  <h2>3. Set Password Admin Android</h2>
  <form method="POST">
    <input type="hidden" name="action" value="setpassword">
    <label>Username:</label>
    <input type="text" name="username" value="admin">
    <label>Password Baru:</label>
    <input type="password" name="new_password" placeholder="min 6 karakter">
    <label>Konfirmasi Password:</label>
    <input type="password" name="confirm_password">
    <button type="submit">Simpan Password</button>
  </form>
</div>

<!-- Generate Hash -->
<div class="card">
  <h2>4. Generate Password Hash (Opsional)</h2>
  <p>Untuk membuat hash manual yang bisa di-paste ke database.</p>
  <form method="POST">
    <input type="hidden" name="action" value="genhash">
    <label>Password:</label>
    <input type="text" name="password" placeholder="Password yang ingin di-hash">
    <button type="submit">Generate Hash</button>
  </form>
</div>

<!-- Checklist -->
<div class="card">
  <h2>5. Checklist Deployment</h2>
  <ul>
    <li>Edit <code>config/database.php</code> -- ubah DB_HOST, DB_USER, DB_PASS, DB_NAME</li>
    <li>Buat database MySQL: <code>CREATE DATABASE smart_cat_feeder;</code></li>
    <li>Buat user MySQL: <code>CREATE USER 'catfeeder_user'@'localhost' IDENTIFIED BY 'password';</code></li>
    <li>Grant privileges: <code>GRANT ALL ON smart_cat_feeder.* TO 'catfeeder_user'@'localhost';</code></li>
    <li>Jalankan Schema SQL (step 2 di atas)</li>
    <li>Set password admin (step 3 di atas)</li>
    <li>Update device_token di firmware ESP8266 agar cocok dengan database</li>
    <li><strong>HAPUS file setup.php ini setelah selesai!</strong></li>
  </ul>
</div>

<p style="color:#888; font-size:12px;">Smart Cat Feeder v1.1 | Setup Tool</p>
</body>
</html>
