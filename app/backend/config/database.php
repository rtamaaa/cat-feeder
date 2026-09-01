<?php
// ============================================================
//  SMART CAT FEEDER -- Database Configuration
//  config/database.php
// ============================================================

define('DB_HOST', 'localhost');
define('DB_PORT', '3306');
define('DB_NAME', 'smart_cat_feeder');
define('DB_USER', 'catfeeder_user');
define('DB_PASS', 'bukalah11');

// Set Timezone ke WIB (Asia/Jakarta)
date_default_timezone_set('Asia/Jakarta');

// API base URL
define('API_BASE_URL', 'http://localhost/smart-cat-feeder');

// Durasi token Android (detik) -- 7 hari
define('ANDROID_TOKEN_EXPIRY', 7 * 24 * 3600);

// ============================================================
//  PDO Connection (Singleton)
// ============================================================
function getDB(): PDO {
    static $pdo = null;

    if ($pdo === null) {
        $dsn = sprintf(
            'mysql:host=%s;port=%s;dbname=%s;charset=utf8mb4',
            DB_HOST, DB_PORT, DB_NAME
        );

        try {
            $pdo = new PDO($dsn, DB_USER, DB_PASS, [
                PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
                PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
                PDO::ATTR_EMULATE_PREPARES   => false,
            ]);
            $pdo->exec("SET time_zone = '+07:00'");
        } catch (PDOException $e) {
            http_response_code(500);
            header('Content-Type: application/json');
            echo json_encode([
                'success' => false,
                'message' => 'Database connection failed',
                'error_code' => 'DB_CONNECTION_ERROR'
            ]);
            exit;
        }
    }

    return $pdo;
}

// ============================================================
//  Helper: JSON Response
// ============================================================
function jsonResponse(bool $success, string $message, array $data = [], int $httpCode = 200, string $errorCode = ''): void {
    http_response_code($httpCode);
    header('Content-Type: application/json');
    header('X-Content-Type-Options: nosniff');

    $resp = ['success' => $success, 'message' => $message];
    if (!empty($data))      $resp['data']       = $data;
    if (!empty($errorCode)) $resp['error_code'] = $errorCode;

    echo json_encode($resp, JSON_UNESCAPED_UNICODE);
    exit;
}

// ============================================================
//  Helper: Validasi device (untuk request dari ESP8266)
//  Header: X-Device-ID dan X-Device-Token
// ============================================================
function validateDevice(): array {
    $deviceId    = $_SERVER['HTTP_X_DEVICE_ID']    ?? '';
    $deviceToken = $_SERVER['HTTP_X_DEVICE_TOKEN'] ?? '';

    if (empty($deviceId) || empty($deviceToken)) {
        jsonResponse(false, 'Missing device credentials', [], 401, 'MISSING_CREDENTIALS');
    }

    $db  = getDB();
    $sql = 'SELECT id, device_id, device_name, status FROM devices WHERE device_id = ? AND api_key_hash = SHA2(?, 256) LIMIT 1';
    $st  = $db->prepare($sql);
    $st->execute([$deviceId, $deviceToken]);
    $device = $st->fetch();

    if (!$device) {
        jsonResponse(false, 'Invalid device credentials', [], 401, 'INVALID_CREDENTIALS');
    }

    // Update status and last_seen on any device communication
    $upd = $db->prepare('UPDATE devices SET last_seen = NOW(), status = "online", updated_at = NOW() WHERE device_id = ?');
    $upd->execute([$device['device_id']]);

    return $device;
}

// ============================================================
//  Helper: Validasi Android user (Bearer token)
//  Header: Authorization: Bearer <token>
// ============================================================
function validateAndroidUser(): array {
    $authHeader = $_SERVER['HTTP_AUTHORIZATION'] ?? $_SERVER['REDIRECT_HTTP_AUTHORIZATION'] ?? '';

    if (empty($authHeader) && function_exists('getallheaders')) {
        $headers = getallheaders();
        $authHeader = $headers['Authorization'] ?? $headers['authorization'] ?? '';
    }

    if (!str_starts_with($authHeader, 'Bearer ')) {
        jsonResponse(false, 'Missing authorization token', [], 401, 'MISSING_TOKEN');
    }

    $token = substr($authHeader, 7);
    if (empty($token)) {
        jsonResponse(false, 'Empty token', [], 401, 'EMPTY_TOKEN');
    }

    $db  = getDB();
    $sql = 'SELECT id, username, device_id FROM android_users
            WHERE api_token = ? AND token_expires > NOW() LIMIT 1';
    $st  = $db->prepare($sql);
    $st->execute([$token]);
    $user = $st->fetch();

    if (!$user) {
        jsonResponse(false, 'Token invalid or expired', [], 401, 'INVALID_TOKEN');
    }

    return $user;
}

// ============================================================
//  Helper: Rate limiting sederhana (by IP)
// ============================================================
function rateLimit(int $maxReq = 60, int $windowSec = 60): void {
    $ip       = $_SERVER['REMOTE_ADDR'] ?? '0.0.0.0';
    $script   = $_SERVER['SCRIPT_NAME'] ?? 'api';
    $cacheKey = sys_get_temp_dir() . '/rl_' . md5($ip . '_' . $script) . '_' . floor(time() / $windowSec);

    $count = 0;
    if (file_exists($cacheKey)) {
        $count = (int)file_get_contents($cacheKey);
    }

    if ($count >= $maxReq) {
        http_response_code(429);
        header('Content-Type: application/json');
        echo json_encode(['success' => false, 'message' => 'Too many requests', 'error_code' => 'RATE_LIMIT']);
        exit;
    }

    file_put_contents($cacheKey, $count + 1);
}

// ============================================================
//  Helper: CORS headers
// ============================================================
function setCorsHeaders(): void {
    header('Access-Control-Allow-Origin: *');
    header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
    header('Access-Control-Allow-Headers: Content-Type, Authorization, X-Device-ID, X-Device-Token');

    if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
        http_response_code(204);
        exit;
    }
}

// ============================================================
//  Helper: Baca JSON body dari request
// ============================================================
function getJsonBody(): array {
    $raw = file_get_contents('php://input');
    if (empty($raw)) return [];

    $data = json_decode($raw, true);
    if (json_last_error() !== JSON_ERROR_NONE) {
        jsonResponse(false, 'Invalid JSON body', [], 400, 'INVALID_JSON');
    }

    return $data ?? [];
}
