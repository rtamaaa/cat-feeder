<?php
// ============================================================
//  POST /api/heartbeat.php
//  Dipanggil ESP8266 setiap ~10 detik
//  Update last_seen, rtc_time, dan status = online
//
//  Header: X-Device-ID, X-Device-Token
//  Body JSON: { "rtc": "2026-09-01 11:40:00" } (opsional)
// ============================================================

require_once __DIR__ . '/../config/database.php';

setCorsHeaders();
rateLimit(120, 60);

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    jsonResponse(false, 'Method not allowed', [], 405, 'METHOD_NOT_ALLOWED');
}

$device = validateDevice();
$body   = getJsonBody();
$rtcStr = trim($body['rtc'] ?? '');

$db = getDB();

if (!empty($rtcStr) && DateTime::createFromFormat('Y-m-d H:i:s', $rtcStr)) {
    try {
        $sql = 'UPDATE devices SET last_seen = NOW(), rtc_time = ?, status = "online", updated_at = NOW() WHERE device_id = ?';
        $st  = $db->prepare($sql);
        $st->execute([$rtcStr, $device['device_id']]);
    } catch (PDOException $e) {
        try {
            $db->exec('ALTER TABLE devices ADD COLUMN rtc_time DATETIME NULL AFTER last_seen');
            $sql = 'UPDATE devices SET last_seen = NOW(), rtc_time = ?, status = "online", updated_at = NOW() WHERE device_id = ?';
            $st  = $db->prepare($sql);
            $st->execute([$rtcStr, $device['device_id']]);
        } catch (Exception $ex) {
            $sql = 'UPDATE devices SET last_seen = NOW(), status = "online", updated_at = NOW() WHERE device_id = ?';
            $st  = $db->prepare($sql);
            $st->execute([$device['device_id']]);
        }
    }
} else {
    $sql = 'UPDATE devices SET last_seen = NOW(), status = "online", updated_at = NOW() WHERE device_id = ?';
    $st  = $db->prepare($sql);
    $st->execute([$device['device_id']]);
}

jsonResponse(true, 'Heartbeat received', [
    'device_id'   => $device['device_id'],
    'server_time' => date('Y-m-d H:i:s')
]);
