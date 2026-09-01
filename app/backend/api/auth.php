<?php
// ============================================================
//  POST /api/auth.php?action=login          --> Login Android
//  POST /api/auth.php?action=logout         --> Logout
//  POST /api/auth.php?action=change-password --> Ganti password
//
//  Login Body: { "username": "...", "password": "..." }
// ============================================================

require_once __DIR__ . '/../config/database.php';

setCorsHeaders();
rateLimit(60, 60);

$method = $_SERVER['REQUEST_METHOD'];
$action = trim($_GET['action'] ?? '');

// ============================================================
//  POST ?action=register
// ============================================================
if ($method === 'POST' && $action === 'register') {

    $body     = getJsonBody();
    $username = trim($body['username'] ?? '');
    $password = $body['password'] ?? '';
    $deviceId = trim($body['device_id'] ?? 'CAT_FEEDER_01');

    if (empty($username) || empty($password)) {
        jsonResponse(false, 'Username dan password wajib diisi', [], 400, 'MISSING_CREDENTIALS');
    }

    if (strlen($username) < 3) {
        jsonResponse(false, 'Username minimal 3 karakter', [], 400, 'USERNAME_TOO_SHORT');
    }

    if (strlen($password) < 6) {
        jsonResponse(false, 'Password minimal 6 karakter', [], 400, 'PASSWORD_TOO_SHORT');
    }

    if (empty($deviceId)) {
        $deviceId = 'CAT_FEEDER_01';
    }

    $db = getDB();

    // Pastikan device_id terdaftar di tabel devices
    $chkDev = $db->prepare('SELECT device_id FROM devices WHERE device_id = ? LIMIT 1');
    $chkDev->execute([$deviceId]);
    if (!$chkDev->fetch()) {
        $insDev = $db->prepare('INSERT INTO devices (device_id, device_name, api_key_hash, status) VALUES (?, ?, SHA2(?, 256), ?)');
        $insDev->execute([$deviceId, 'Smart Feeder ' . $deviceId, 'catfeeder_secret_token_2026', 'unknown']);
    }

    // Cek apakah username sudah ada
    $st = $db->prepare('SELECT id FROM android_users WHERE username = ? LIMIT 1');
    $st->execute([$username]);
    if ($st->fetch()) {
        jsonResponse(false, 'Username sudah terdaftar! Pilih username lain.', [], 409, 'USERNAME_EXISTS');
    }

    // Insert user baru
    $hash    = password_hash($password, PASSWORD_DEFAULT);
    $token   = bin2hex(random_bytes(32));
    $expires = date('Y-m-d H:i:s', time() + ANDROID_TOKEN_EXPIRY);

    $ins = $db->prepare('INSERT INTO android_users (username, password_hash, api_token, token_expires, device_id) VALUES (?, ?, ?, ?, ?)');
    $ins->execute([$username, $hash, $token, $expires, $deviceId]);

    jsonResponse(true, 'Registrasi berhasil! Selamat datang.', [
        'token'      => $token,
        'expires_at' => $expires,
        'username'   => $username,
        'device_id'  => $deviceId
    ], 201);
}

// ============================================================
//  POST ?action=login
// ============================================================
if ($method === 'POST' && $action === 'login') {

    $body     = getJsonBody();
    $username = trim($body['username'] ?? '');
    $password = $body['password'] ?? '';

    if (empty($username) || empty($password)) {
        jsonResponse(false, 'Username and password required', [], 400, 'MISSING_CREDENTIALS');
    }

    $db  = getDB();
    $sql = 'SELECT id, username, password_hash, device_id FROM android_users WHERE username = ? LIMIT 1';
    $st  = $db->prepare($sql);
    $st->execute([$username]);
    $user = $st->fetch();

    if (!$user || !password_verify($password, $user['password_hash'])) {
        usleep(random_int(200000, 400000));
        jsonResponse(false, 'Username atau password salah', [], 401, 'INVALID_CREDENTIALS');
    }

    $token   = bin2hex(random_bytes(32));
    $expires = date('Y-m-d H:i:s', time() + ANDROID_TOKEN_EXPIRY);

    $upd = $db->prepare('UPDATE android_users SET api_token = ?, token_expires = ? WHERE id = ?');
    $upd->execute([$token, $expires, $user['id']]);

    jsonResponse(true, 'Login berhasil', [
        'token'      => $token,
        'expires_at' => $expires,
        'username'   => $user['username'],
        'device_id'  => $user['device_id']
    ]);
}

// ============================================================
//  POST ?action=logout
// ============================================================
if ($method === 'POST' && $action === 'logout') {

    $user = validateAndroidUser();

    $db  = getDB();
    $upd = $db->prepare('UPDATE android_users SET api_token = NULL, token_expires = NULL WHERE id = ?');
    $upd->execute([$user['id']]);

    jsonResponse(true, 'Logout berhasil');
}

// ============================================================
//  POST ?action=change-password
// ============================================================
if ($method === 'POST' && $action === 'change-password') {

    $user = validateAndroidUser();
    $body = getJsonBody();

    $oldPassword = $body['old_password'] ?? '';
    $newPassword = $body['new_password'] ?? '';

    if (empty($oldPassword) || empty($newPassword)) {
        jsonResponse(false, 'old_password and new_password required', [], 400, 'MISSING_PASSWORDS');
    }

    if (strlen($newPassword) < 6) {
        jsonResponse(false, 'Password minimal 6 karakter', [], 400, 'PASSWORD_TOO_SHORT');
    }

    $db  = getDB();
    $sql = 'SELECT password_hash FROM android_users WHERE id = ? LIMIT 1';
    $st  = $db->prepare($sql);
    $st->execute([$user['id']]);
    $row = $st->fetch();

    if (!password_verify($oldPassword, $row['password_hash'])) {
        jsonResponse(false, 'Password lama salah', [], 401, 'INVALID_OLD_PASSWORD');
    }

    $newHash = password_hash($newPassword, PASSWORD_DEFAULT);
    $upd = $db->prepare('UPDATE android_users SET password_hash = ? WHERE id = ?');
    $upd->execute([$newHash, $user['id']]);

    jsonResponse(true, 'Password berhasil diubah');
}

// Tidak ada action yang cocok
jsonResponse(false, 'Action tidak valid. Gunakan: ?action=login | logout | change-password', [], 400, 'INVALID_ACTION');
