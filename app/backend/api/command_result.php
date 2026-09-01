<?php
// ============================================================
//  POST /api/device/command-result
//  ESP8266 melaporkan hasil eksekusi command
//
//  Header: X-Device-ID, X-Device-Token
//  Body JSON:
//    {
//      "command_id": 123,
//      "status": "success" | "failed",
//      "message": "..." (opsional)
//    }
// ============================================================

require_once __DIR__ . '/../config/database.php';

setCorsHeaders();
rateLimit(120, 60);

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    jsonResponse(false, 'Method not allowed', [], 405, 'METHOD_NOT_ALLOWED');
}

$device = validateDevice();
$body   = getJsonBody();

// Validasi input
$commandId = isset($body['command_id']) ? (int)$body['command_id'] : 0;
$status    = trim($body['status'] ?? '');
$message   = trim($body['message'] ?? '');

if ($commandId <= 0) {
    jsonResponse(false, 'Invalid command_id', [], 400, 'INVALID_COMMAND_ID');
}

if (!in_array($status, ['success', 'failed'], true)) {
    jsonResponse(false, 'Status must be "success" or "failed"', [], 400, 'INVALID_STATUS');
}

$db = getDB();

// Pastikan command milik device ini dan statusnya 'processing'
$sql = 'SELECT id, command FROM commands WHERE id = ? AND device_id = ? LIMIT 1';
$st  = $db->prepare($sql);
$st->execute([$commandId, $device['device_id']]);
$cmd = $st->fetch();

if (!$cmd) {
    jsonResponse(false, 'Command not found or not owned by this device', [], 404, 'COMMAND_NOT_FOUND');
}

// Update status dan waktu eksekusi
$upd = $db->prepare('UPDATE commands SET status = ?, executed_at = NOW() WHERE id = ?');
$upd->execute([$status, $commandId]);

jsonResponse(true, 'Command result recorded', [
    'command_id' => $commandId,
    'command'    => $cmd['command'],
    'status'     => $status
]);
