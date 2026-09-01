<?php
// ============================================================
//  GET /api/device/commands
//  Dipanggil ESP8266 setiap ~3 detik (polling)
//  Mengembalikan 1 command pending tertua
//  Langsung ubah status ke 'processing' sehingga
//  tidak dikirim ulang ke polling berikutnya
//
//  Header: X-Device-ID, X-Device-Token
// ============================================================

require_once __DIR__ . '/../config/database.php';

setCorsHeaders();
rateLimit(120, 60);

if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
    jsonResponse(false, 'Method not allowed', [], 405, 'METHOD_NOT_ALLOWED');
}

$device = validateDevice();
$db     = getDB();

// Ambil command pending tertua untuk device ini
$sql = 'SELECT id, command, payload
        FROM commands
        WHERE device_id = ? AND status = "pending"
        ORDER BY created_at ASC
        LIMIT 1';
$st  = $db->prepare($sql);
$st->execute([$device['device_id']]);
$cmd = $st->fetch();

if (!$cmd) {
    // Tidak ada command
    jsonResponse(true, 'No command', ['command' => null]);
}

// Tandai sebagai 'processing' agar tidak dikirim ulang
$upd = $db->prepare('UPDATE commands SET status = "processing" WHERE id = ?');
$upd->execute([$cmd['id']]);

// Decode payload JSON jika ada
$payload = null;
if (!empty($cmd['payload'])) {
    $payload = json_decode($cmd['payload'], true);
}

jsonResponse(true, 'Command available', [
    'command_id' => (int)$cmd['id'],
    'command'    => $cmd['command'],
    'payload'    => $payload
]);
