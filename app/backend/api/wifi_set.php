<?php
// ============================================================
//  POST /api/wifi/set   --> Android ganti WiFi ESP8266 via server
//
//  Android: Header Authorization: Bearer <token>
//  Body JSON:
//    {
//      "device_id": "CAT_FEEDER_01",
//      "ssid":     "NamaWiFiBaru",
//      "password": "PasswordWiFiBaru"
//    }
//
//  Flow:
//  1. Android POST ke sini
//  2. Server buat command SET_WIFI dengan payload {ssid, password}
//  3. ESP8266 polling --> terima SET_WIFI
//  4. ESP8266 update wifi_config.json di LittleFS
//  5. ESP8266 restart --> konek ke WiFi baru
//  6. ESP8266 POST command-result --> server update status
// ============================================================

require_once __DIR__ . '/../config/database.php';

setCorsHeaders();
rateLimit(10, 60); // batasi ketat untuk keamanan

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    jsonResponse(false, 'Method not allowed', [], 405, 'METHOD_NOT_ALLOWED');
}

$user     = validateAndroidUser();
$deviceId = $user['device_id'];
$body     = getJsonBody();

$reqDeviceId = trim($body['device_id'] ?? '');
$ssid        = trim($body['ssid']      ?? '');
$password    = $body['password']       ?? '';  // password boleh kosong (WiFi open)

// Validasi device_id
if (!empty($reqDeviceId) && $reqDeviceId !== $deviceId) {
    jsonResponse(false, 'Access denied to this device', [], 403, 'DEVICE_ACCESS_DENIED');
}

// Validasi SSID
if (empty($ssid)) {
    jsonResponse(false, 'SSID tidak boleh kosong', [], 400, 'MISSING_SSID');
}

if (strlen($ssid) > 32) {
    jsonResponse(false, 'SSID maksimal 32 karakter', [], 400, 'SSID_TOO_LONG');
}

if (strlen($password) > 63) {
    jsonResponse(false, 'Password WiFi maksimal 63 karakter', [], 400, 'PASSWORD_TOO_LONG');
}

$db = getDB();

// Cek device ada
$st = $db->prepare('SELECT device_id, status FROM devices WHERE device_id = ? LIMIT 1');
$st->execute([$deviceId]);
if (!$st->fetch()) {
    jsonResponse(false, 'Device not found', [], 404, 'DEVICE_NOT_FOUND');
}

// Cek apakah sudah ada command SET_WIFI pending
$chk = $db->prepare('SELECT COUNT(*) as cnt FROM commands
                      WHERE device_id = ? AND command = "SET_WIFI"
                      AND status IN ("pending","processing")');
$chk->execute([$deviceId]);
$row = $chk->fetch();
if ((int)$row['cnt'] > 0) {
    jsonResponse(false, 'SET_WIFI command already pending', [], 409, 'COMMAND_ALREADY_PENDING');
}

// Buat command SET_WIFI
// PENTING: password dikirim apa adanya karena ESP8266 butuh plain text
// untuk WPA2. Pastikan koneksi HTTPS!
$payload = json_encode([
    'ssid'     => $ssid,
    'password' => $password
]);

$cmd = $db->prepare('INSERT INTO commands (device_id, command, payload, status) VALUES (?, "SET_WIFI", ?, "pending")');
$cmd->execute([$deviceId, $payload]);
$commandId = (int)$db->lastInsertId();

jsonResponse(true, 'SET_WIFI command created', [
    'command_id' => $commandId,
    'device_id'  => $deviceId,
    'ssid'       => $ssid,
    'note'       => 'ESP8266 akan menerima command ini saat polling berikutnya dan restart otomatis'
]);
