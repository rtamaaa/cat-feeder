<?php
// ============================================================
//  API PENGATURAN SERVO & RIWAYAT PERUBAHAN
//  api/servo_settings.php
//
//  GET  ?action=get   --> Ambil pengaturan saat ini (Android)
//  POST ?action=set   --> Simpan pengaturan baru (Android)
//  GET  ?action=logs  --> Riwayat perubahan pengaturan (Android)
// ============================================================

require_once __DIR__ . '/../config/database.php';

setCorsHeaders();
rateLimit(60, 60);

$method   = $_SERVER['REQUEST_METHOD'];
$action   = trim($_GET['action'] ?? 'get');
$isDevice = isset($_SERVER['HTTP_X_DEVICE_ID']);

// ============================================================
//  GET ?action=get (Android & ESP8266)
// ============================================================
if ($method === 'GET' && ($action === 'get' || $action === '')) {

    if ($isDevice) {
        $device   = validateDevice();
        $deviceId = $device['device_id'];
    } else {
        $user     = validateAndroidUser();
        $deviceId = $user['device_id'];
    }

    $db = getDB();
    $st = $db->prepare('SELECT servo_close_angle, servo_open_angle, feed_duration_ms, updated_at FROM device_settings WHERE device_id = ? LIMIT 1');
    $st->execute([$deviceId]);
    $settings = $st->fetch();

    if (!$settings) {
        $settings = [
            'servo_close_angle' => 0,
            'servo_open_angle'  => 90,
            'feed_duration_ms'  => 2000,
            'updated_at'        => date('Y-m-d H:i:s')
        ];
    }

    jsonResponse(true, 'Servo settings retrieved', [
        'device_id'   => $deviceId,
        'settings'    => [
            'close_angle' => (int)$settings['servo_close_angle'],
            'open_angle'  => (int)$settings['servo_open_angle'],
            'duration_ms' => (int)$settings['feed_duration_ms'],
            'updated_at'  => $settings['updated_at']
        ]
    ]);
}

// ============================================================
//  POST ?action=set (Android)
// ============================================================
if ($method === 'POST' && $action === 'set') {

    $user     = validateAndroidUser();
    $deviceId = $user['device_id'];
    $body     = getJsonBody();

    $closeAngle = isset($body['close_angle']) ? (int)$body['close_angle'] : 0;
    $openAngle  = isset($body['open_angle'])  ? (int)$body['open_angle']  : 90;
    $durationMs = isset($body['duration_ms']) ? (int)$body['duration_ms'] : 2000;

    // Validasi
    $closeAngle = max(0, min(180, $closeAngle));
    $openAngle  = max(0, min(180, $openAngle));
    $durationMs = max(500, min(10000, $durationMs)); // 0.5s - 10s

    $db = getDB();
    $db->beginTransaction();

    try {
        // 1. Simpan ke device_settings
        $upsert = $db->prepare('INSERT INTO device_settings (device_id, servo_close_angle, servo_open_angle, feed_duration_ms, updated_at)
                                VALUES (?, ?, ?, ?, NOW())
                                ON DUPLICATE KEY UPDATE
                                  servo_close_angle = VALUES(servo_close_angle),
                                  servo_open_angle  = VALUES(servo_open_angle),
                                  feed_duration_ms  = VALUES(feed_duration_ms),
                                  updated_at        = NOW()');
        $upsert->execute([$deviceId, $closeAngle, $openAngle, $durationMs]);

        // 2. Simpan ke settings_logs (Riwayat Pengaturan)
        $insLog = $db->prepare('INSERT INTO settings_logs (device_id, servo_close_angle, servo_open_angle, feed_duration_ms, changed_by, created_at)
                               VALUES (?, ?, ?, ?, ?, NOW())');
        $insLog->execute([$deviceId, $closeAngle, $openAngle, $durationMs, $user['username']]);

        // 3. Buat command SET_SERVO_CONFIG untuk ESP8266
        $payload = json_encode([
            'close_angle' => $closeAngle,
            'open_angle'  => $openAngle,
            'duration_ms' => $durationMs
        ]);
        $cmd = $db->prepare('INSERT INTO commands (device_id, command, payload, status)
                             VALUES (?, "SET_SERVO_CONFIG", ?, "pending")');
        $cmd->execute([$deviceId, $payload]);

        $db->commit();

        jsonResponse(true, 'Pengaturan servo berhasil disimpan', [
            'device_id'   => $deviceId,
            'close_angle' => $closeAngle,
            'open_angle'  => $openAngle,
            'duration_ms' => $durationMs,
            'command'     => 'SET_SERVO_CONFIG pending'
        ]);

    } catch (Exception $e) {
        $db->rollBack();
        jsonResponse(false, 'Gagal menyimpan pengaturan servo: ' . $e->getMessage(), [], 500, 'DB_ERROR');
    }
}

// ============================================================
//  GET ?action=logs (Android -- Riwayat Pengaturan)
// ============================================================
if ($method === 'GET' && $action === 'logs') {

    $user     = validateAndroidUser();
    $deviceId = $user['device_id'];
    $db       = getDB();

    $limit = min((int)($_GET['limit'] ?? 20), 50);
    $sql   = 'SELECT id, servo_close_angle, servo_open_angle, feed_duration_ms, changed_by, created_at
              FROM settings_logs
              WHERE device_id = ?
              ORDER BY created_at DESC
              LIMIT ?';
    $st    = $db->prepare($sql);
    $st->execute([$deviceId, $limit]);
    $logs  = $st->fetchAll();

    $formatted = [];
    foreach ($logs as $l) {
        $formatted[] = [
            'id'          => (int)$l['id'],
            'close_angle' => (int)$l['servo_close_angle'],
            'open_angle'  => (int)$l['servo_open_angle'],
            'duration_ms' => (int)$l['feed_duration_ms'],
            'changed_by'  => $l['changed_by'],
            'created_at'  => $l['created_at']
        ];
    }

    jsonResponse(true, 'Settings history retrieved', [
        'device_id' => $deviceId,
        'count'     => count($formatted),
        'logs'      => $formatted
    ]);
}

jsonResponse(false, 'Action tidak valid. Gunakan: ?action=get | set | logs', [], 400, 'INVALID_ACTION');
