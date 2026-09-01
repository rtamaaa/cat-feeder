<?php
// ============================================================
//  GET  /api/device_status.php?action=status  --> Status device (Android)
//  POST /api/device_status.php?action=upload-log --> Upload offline log (ESP8266)
//  GET  /api/device_status.php?action=feeding-log --> Riwayat feeding (Android)
// ============================================================

require_once __DIR__ . '/../config/database.php';

setCorsHeaders();
rateLimit(60, 60);

$method   = $_SERVER['REQUEST_METHOD'];
$action   = trim($_GET['action'] ?? 'status');
$isDevice = isset($_SERVER['HTTP_X_DEVICE_ID']);

// ============================================================
//  GET ?action=status (Android)
// ============================================================
if ($method === 'GET' && $action === 'status') {

    $user     = validateAndroidUser();
    $deviceId = $user['device_id'];
    $db       = getDB();

    try {
        $sql = 'SELECT device_id, device_name, last_seen, rtc_time, status,
                       TIMESTAMPDIFF(SECOND, last_seen, NOW()) AS db_seconds_ago
                FROM devices WHERE device_id = ? LIMIT 1';
        $st  = $db->prepare($sql);
        $st->execute([$deviceId]);
        $device = $st->fetch();
    } catch (PDOException $e) {
        $sql = 'SELECT device_id, device_name, last_seen, status,
                       TIMESTAMPDIFF(SECOND, last_seen, NOW()) AS db_seconds_ago
                FROM devices WHERE device_id = ? LIMIT 1';
        $st  = $db->prepare($sql);
        $st->execute([$deviceId]);
        $device = $st->fetch();
        if ($device) {
            $device['rtc_time'] = null;
        }
    }

    if (!$device) {
        jsonResponse(false, 'Device not found', [], 404, 'DEVICE_NOT_FOUND');
    }

    $logSt = $db->prepare('SELECT type, schedule_slot, status, executed_at FROM feeding_logs WHERE device_id = ? ORDER BY executed_at DESC LIMIT 1');
    $logSt->execute([$deviceId]);
    $lastFeed = $logSt->fetch();

    $cmdSt = $db->prepare('SELECT id, command, status, created_at, executed_at FROM commands WHERE device_id = ? ORDER BY created_at DESC LIMIT 1');
    $cmdSt->execute([$deviceId]);
    $lastCmd = $cmdSt->fetch();

    $secondsAgo = isset($device['db_seconds_ago']) && $device['db_seconds_ago'] !== null
        ? abs((int)$device['db_seconds_ago'])
        : null;

    if ($secondsAgo === null && $lastCmd && !empty($lastCmd['executed_at'])) {
        $cmdDiffSt = $db->prepare('SELECT TIMESTAMPDIFF(SECOND, executed_at, NOW()) AS diff FROM commands WHERE id = ?');
        $cmdDiffSt->execute([$lastCmd['id']]);
        $diffRow = $cmdDiffSt->fetch();
        if ($diffRow && $diffRow['diff'] !== null) {
            $secondsAgo = abs((int)$diffRow['diff']);
        }
    }

    $computedStatus = 'offline';
    if ($secondsAgo !== null) {
        if ($secondsAgo < 60)      $computedStatus = 'online';
        elseif ($secondsAgo < 180) $computedStatus = 'warning';
        else                       $computedStatus = 'offline';
    }

    jsonResponse(true, 'Device status retrieved', [
        'device_id'    => $device['device_id'],
        'device_name'  => $device['device_name'],
        'status'       => $computedStatus,
        'last_seen'    => $device['last_seen'] ?: ($lastCmd['executed_at'] ?? 'Baru saja'),
        'seconds_ago'  => $secondsAgo !== null ? max(0, $secondsAgo) : 0,
        'rtc_time'     => !empty($device['rtc_time']) ? $device['rtc_time'] : date('Y-m-d H:i:s'),
        'last_feeding' => $lastFeed ?: null,
        'last_command' => $lastCmd  ?: null,
        'server_time'  => date('Y-m-d H:i:s')
    ]);
}

// ============================================================
//  POST ?action=upload-log (ESP8266)
// ============================================================
if ($method === 'POST' && $action === 'upload-log' && $isDevice) {

    $device   = validateDevice();
    $deviceId = $device['device_id'];
    $body     = getJsonBody();

    $logs = $body['logs'] ?? [];
    if (empty($logs) || !is_array($logs)) {
        jsonResponse(false, 'logs array is required', [], 400, 'MISSING_LOGS');
    }

    $db  = getDB();
    $ins = $db->prepare('INSERT IGNORE INTO feeding_logs (device_id, type, schedule_slot, status, executed_at, synced) VALUES (?, ?, ?, ?, ?, 0)');

    $saved = 0;
    foreach ($logs as $log) {
        $type       = in_array($log['type'] ?? '', ['manual','schedule']) ? $log['type'] : 'manual';
        $slot       = isset($log['slot']) ? (int)$log['slot'] : null;
        $logStatus  = in_array($log['status'] ?? '', ['success','failed']) ? $log['status'] : 'success';
        $executedAt = $log['timestamp'] ?? date('Y-m-d H:i:s');
        if (!DateTime::createFromFormat('Y-m-d H:i:s', $executedAt)) continue;
        $ins->execute([$deviceId, $type, $slot, $logStatus, $executedAt]);
        $saved++;
    }

    jsonResponse(true, "Feeding logs uploaded: $saved entries", ['saved' => $saved]);
}

// ============================================================
//  GET ?action=feeding-log (Android)
// ============================================================
if ($method === 'GET' && $action === 'feeding-log') {

    $user     = validateAndroidUser();
    $deviceId = $user['device_id'];
    $db       = getDB();

    $limit = min((int)($_GET['limit'] ?? 20), 100);
    $sql   = 'SELECT id, type, schedule_slot, status, executed_at, synced FROM feeding_logs WHERE device_id = ? ORDER BY executed_at DESC LIMIT ?';
    $st    = $db->prepare($sql);
    $st->execute([$deviceId, $limit]);
    $logs = $st->fetchAll();

    foreach ($logs as &$log) {
        $log['schedule_slot'] = $log['schedule_slot'] ? (int)$log['schedule_slot'] : null;
        $log['synced']        = (bool)$log['synced'];
    }

    jsonResponse(true, 'Feeding logs retrieved', [
        'device_id' => $deviceId,
        'count'     => count($logs),
        'logs'      => $logs
    ]);
}

jsonResponse(false, 'Action tidak valid. Gunakan: ?action=status | feeding-log | upload-log', [], 400, 'INVALID_ACTION');
