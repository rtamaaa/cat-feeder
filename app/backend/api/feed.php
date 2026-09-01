<?php
// ============================================================
//  POST /api/feed
//  Android menekan "FEED NOW"
//  Membuat command FEED dengan status pending
//
//  Header: Authorization: Bearer <token>
//  Body JSON:
//    { "device_id": "CAT_FEEDER_01" }
// ============================================================

require_once __DIR__ . '/../config/database.php';

setCorsHeaders();
rateLimit(20, 60); // max 20 feed request per menit per IP

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    jsonResponse(false, 'Method not allowed', [], 405, 'METHOD_NOT_ALLOWED');
}

$user = validateAndroidUser();
$body = getJsonBody();

$deviceId = trim($body['device_id'] ?? '');

if (empty($deviceId)) {
    jsonResponse(false, 'device_id is required', [], 400, 'MISSING_DEVICE_ID');
}

// Pastikan user hanya bisa kontrol device yang terdaftar untuknya
if ($user['device_id'] !== $deviceId) {
    jsonResponse(false, 'Access denied to this device', [], 403, 'DEVICE_ACCESS_DENIED');
}

$db = getDB();

// Cek device ada
$st = $db->prepare('SELECT device_id, status FROM devices WHERE device_id = ? LIMIT 1');
$st->execute([$deviceId]);
$device = $st->fetch();

if (!$device) {
    jsonResponse(false, 'Device not found', [], 404, 'DEVICE_NOT_FOUND');
}

// Cek apakah sudah ada command FEED pending/processing
// (hindari flooding command)
$chk = $db->prepare('SELECT COUNT(*) as cnt FROM commands
                      WHERE device_id = ? AND command = "FEED"
                      AND status IN ("pending","processing")');
$chk->execute([$deviceId]);
$row = $chk->fetch();

if ((int)$row['cnt'] > 0) {
    jsonResponse(false, 'A FEED command is already pending or processing', [], 409, 'COMMAND_ALREADY_PENDING');
}

// Buat command FEED baru
$ins = $db->prepare('INSERT INTO commands (device_id, command, status) VALUES (?, "FEED", "pending")');
$ins->execute([$deviceId]);
$commandId = (int)$db->lastInsertId();

jsonResponse(true, 'Feed command created', [
    'command_id' => $commandId,
    'device_id'  => $deviceId,
    'command'    => 'FEED',
    'status'     => 'pending'
]);
