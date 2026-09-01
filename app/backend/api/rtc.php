<?php
// ============================================================
//  GET  /api/rtc.php?action=get   --> Android baca status RTC
//  POST /api/rtc.php?action=set   --> Android set RTC via command
//
//  Set Body: { "datetime": "2026-09-01 18:30:00" }  (opsional, default: waktu server)
// ============================================================

require_once __DIR__ . '/../config/database.php';

setCorsHeaders();
rateLimit(30, 60);

$method = $_SERVER['REQUEST_METHOD'];
$action = trim($_GET['action'] ?? '');

// ---- POST ?action=set ----
if ($method === 'POST' && $action === 'set') {

    $user     = validateAndroidUser();
    $deviceId = $user['device_id'];
    $body     = getJsonBody();

    $datetimeStr = trim($body['datetime'] ?? date('Y-m-d H:i:s'));

    $dt = DateTime::createFromFormat('Y-m-d H:i:s', $datetimeStr);
    if (!$dt) {
        jsonResponse(false, 'Invalid datetime format. Use: YYYY-MM-DD HH:MM:SS', [], 400, 'INVALID_DATETIME');
    }

    $db = getDB();
    $st = $db->prepare('SELECT device_id FROM devices WHERE device_id = ? LIMIT 1');
    $st->execute([$deviceId]);
    if (!$st->fetch()) {
        jsonResponse(false, 'Device not found', [], 404, 'DEVICE_NOT_FOUND');
    }

    $payload = json_encode(['datetime' => $datetimeStr]);
    $cmd = $db->prepare('INSERT INTO commands (device_id, command, payload, status) VALUES (?, "SET_RTC", ?, "pending")');
    $cmd->execute([$deviceId, $payload]);
    $commandId = (int)$db->lastInsertId();

    jsonResponse(true, 'SET_RTC command created', [
        'command_id' => $commandId,
        'device_id'  => $deviceId,
        'datetime'   => $datetimeStr
    ]);
}

// ---- GET ?action=get ----
if ($method === 'GET' && ($action === 'get' || $action === '')) {

    $user     = validateAndroidUser();
    $deviceId = $user['device_id'];
    $db       = getDB();

    $sql = 'SELECT c.payload, c.executed_at, c.status
            FROM commands c
            WHERE c.device_id = ? AND c.command = "SET_RTC"
            ORDER BY c.created_at DESC LIMIT 1';
    $st = $db->prepare($sql);
    $st->execute([$deviceId]);
    $row = $st->fetch();

    if (!$row) {
        jsonResponse(true, 'No RTC sync history', ['rtc' => null]);
    }

    $payload = json_decode($row['payload'] ?? '{}', true);
    jsonResponse(true, 'RTC history retrieved', [
        'last_set'    => $payload['datetime'] ?? null,
        'executed_at' => $row['executed_at'],
        'status'      => $row['status']
    ]);
}

jsonResponse(false, 'Action tidak valid. Gunakan: ?action=set | get', [], 400, 'INVALID_ACTION');
