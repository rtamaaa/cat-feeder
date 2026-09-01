<?php
// ============================================================
//  API JADWAL (Mendukung Tambah/Hapus/Ubah Jadwal Dinamis)
//  api/schedule.php
//
//  GET  /api/schedule.php  --> Ambil semua jadwal
//  POST /api/schedule.php  --> Simpan seluruh daftar jadwal
// ============================================================

require_once __DIR__ . '/../config/database.php';

setCorsHeaders();
rateLimit(60, 60);

$method   = $_SERVER['REQUEST_METHOD'];
$isDevice = isset($_SERVER['HTTP_X_DEVICE_ID']);

// ---- GET: Ambil jadwal ----
if ($method === 'GET') {

    if ($isDevice) {
        $device   = validateDevice();
        $deviceId = $device['device_id'];
    } else {
        $user     = validateAndroidUser();
        $deviceId = $user['device_id'];
    }

    $db  = getDB();
    $sql = 'SELECT slot, enabled, hour, minute, updated_at
            FROM schedules
            WHERE device_id = ?
            ORDER BY slot ASC';
    $st  = $db->prepare($sql);
    $st->execute([$deviceId]);
    $rows = $st->fetchAll();

    $schedules = [];
    foreach ($rows as $r) {
        $schedules['schedule' . $r['slot']] = [
            'slot'       => (int)$r['slot'],
            'enabled'    => (bool)$r['enabled'],
            'hour'       => (int)$r['hour'],
            'minute'     => (int)$r['minute'],
            'updated_at' => $r['updated_at']
        ];
    }

    jsonResponse(true, 'Schedule retrieved', [
        'device_id' => $deviceId,
        'count'     => count($schedules),
        'schedules' => $schedules
    ]);
}

// ---- POST: Simpan daftar jadwal dari Android ----
if ($method === 'POST') {

    $user     = validateAndroidUser();
    $deviceId = $user['device_id'];
    $body     = getJsonBody();

    // Body: { "schedules": [ {"slot": 1, "enabled": true, "hour": 7, "minute": 0}, ... ] }
    $schedules = $body['schedules'] ?? [];

    if (!is_array($schedules)) {
        jsonResponse(false, 'schedules array is required', [], 400, 'MISSING_SCHEDULES');
    }

    $db = getDB();
    $db->beginTransaction();

    try {
        // Hapus jadwal lama milik device ini agar sinkron dengan daftar baru
        $del = $db->prepare('DELETE FROM schedules WHERE device_id = ?');
        $del->execute([$deviceId]);

        $ins = $db->prepare('INSERT INTO schedules (device_id, slot, enabled, hour, minute) VALUES (?, ?, ?, ?, ?)');

        $savedList = [];
        $slotNum   = 1;

        foreach ($schedules as $s) {
            $enabled = isset($s['enabled']) ? (bool)$s['enabled'] : true;
            $hour    = isset($s['hour'])    ? (int)$s['hour']    : 0;
            $minute  = isset($s['minute'])  ? (int)$s['minute']  : 0;

            if ($hour < 0 || $hour > 23) {
                throw new InvalidArgumentException("Jam tidak valid: $hour (harus 0-23)");
            }
            if ($minute < 0 || $minute > 59) {
                throw new InvalidArgumentException("Menit tidak valid: $minute (harus 0-59)");
            }

            $ins->execute([$deviceId, $slotNum, $enabled ? 1 : 0, $hour, $minute]);
            $savedList[] = [
                'slot'    => $slotNum,
                'enabled' => $enabled,
                'hour'    => $hour,
                'minute'  => $minute
            ];
            $slotNum++;
        }

        // Buat command SET_SCHEDULE untuk ESP8266 dengan payload lengkap
        $payload = json_encode(['schedules' => $savedList]);
        $cmd = $db->prepare('INSERT INTO commands (device_id, command, payload, status)
                             VALUES (?, "SET_SCHEDULE", ?, "pending")');
        $cmd->execute([$deviceId, $payload]);

        $db->commit();

        jsonResponse(true, 'Jadwal berhasil diperbarui', [
            'device_id'   => $deviceId,
            'total_saved' => count($savedList),
            'schedules'   => $savedList,
            'command'     => 'SET_SCHEDULE pending'
        ]);

    } catch (InvalidArgumentException $e) {
        $db->rollBack();
        jsonResponse(false, $e->getMessage(), [], 400, 'VALIDATION_ERROR');
    } catch (Exception $e) {
        $db->rollBack();
        jsonResponse(false, 'Gagal menyimpan jadwal: ' . $e->getMessage(), [], 500, 'DB_ERROR');
    }
}

jsonResponse(false, 'Method not allowed', [], 405, 'METHOD_NOT_ALLOWED');
