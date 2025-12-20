<?php
/**
 * MIDI Xplorer License API
 * Handles license activation, deactivation, and validation
 */

header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST, GET, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');

// Handle preflight requests
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit();
}

// Database configuration
define('DB_HOST', 'localhost');
define('DB_NAME', 'allonivf_midixplorer_licenses');
define('DB_USER', 'allonivf_midilicense');
define('DB_PASS', 'MidiXplorer2024Secure');

// Get JSON input
$input = json_decode(file_get_contents('php://input'), true);

if (!$input) {
    sendError('Invalid request', 'invalid_request');
}

// Database connection
try {
    $pdo = new PDO(
        "mysql:host=" . DB_HOST . ";dbname=" . DB_NAME . ";charset=utf8mb4",
        DB_USER,
        DB_PASS,
        [
            PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
            PDO::ATTR_EMULATE_PREPARES => false
        ]
    );
} catch (PDOException $e) {
    sendError('Database connection failed', 'database_error');
}

// Get action
$action = $input['action'] ?? '';

switch ($action) {
    case 'activate':
        handleActivation($pdo, $input);
        break;
    case 'deactivate':
        handleDeactivation($pdo, $input);
        break;
    case 'validate':
        handleValidation($pdo, $input);
        break;
    default:
        sendError('Invalid action', 'invalid_action');
}

/**
 * Handle license activation
 */
function handleActivation($pdo, $input) {
    $licenseKey = $input['license_key'] ?? '';
    $machineId = $input['machine_id'] ?? '';
    $machineName = $input['machine_name'] ?? '';
    $osType = $input['os_type'] ?? '';
    $osVersion = $input['os_version'] ?? '';
    $appVersion = $input['app_version'] ?? '';
    $ipAddress = $_SERVER['REMOTE_ADDR'] ?? '';

    if (empty($licenseKey) || empty($machineId)) {
        sendError('License key and machine ID are required', 'missing_params');
    }

    // Find the license
    $stmt = $pdo->prepare("SELECT * FROM licenses WHERE license_key = ?");
    $stmt->execute([$licenseKey]);
    $license = $stmt->fetch();

    if (!$license) {
        logAction($pdo, null, 'activation_failed_invalid', "Key: $licenseKey, Machine: $machineId", $ipAddress);
        sendError('Invalid license key', 'invalid_key');
    }

    // Check license status
    if ($license['status'] !== 'active') {
        logAction($pdo, $license['id'], 'activation_failed_status', "Status: {$license['status']}", $ipAddress);
        
        if ($license['status'] === 'expired') {
            sendError('License has expired', 'expired');
        } elseif ($license['status'] === 'revoked') {
            sendError('License has been revoked', 'revoked');
        } else {
            sendError('License is not active', 'inactive');
        }
    }

    // Check expiry date
    if ($license['expiry_date'] && strtotime($license['expiry_date']) < time()) {
        // Update status to expired
        $stmt = $pdo->prepare("UPDATE licenses SET status = 'expired' WHERE id = ?");
        $stmt->execute([$license['id']]);
        
        logAction($pdo, $license['id'], 'activation_failed_expired', "Expiry: {$license['expiry_date']}", $ipAddress);
        sendError('License has expired', 'expired');
    }

    // Check if this machine is already activated
    $stmt = $pdo->prepare("SELECT * FROM activations WHERE license_id = ? AND machine_id = ? AND is_active = 1");
    $stmt->execute([$license['id'], $machineId]);
    $existingActivation = $stmt->fetch();

    if ($existingActivation) {
        // Update last seen
        $stmt = $pdo->prepare("UPDATE activations SET last_seen = NOW(), app_version = ?, ip_address = ? WHERE id = ?");
        $stmt->execute([$appVersion, $ipAddress, $existingActivation['id']]);
        
        logAction($pdo, $license['id'], 'reactivation', "Machine: $machineName", $ipAddress);
        
        sendSuccess('License reactivated successfully', [
            'email' => $license['email'],
            'customer_name' => $license['customer_name'],
            'license_type' => $license['license_type'],
            'expiry_date' => $license['expiry_date'],
            'max_activations' => $license['max_activations'],
            'current_activations' => $license['current_activations']
        ]);
    }

    // Check max activations
    if ($license['current_activations'] >= $license['max_activations']) {
        logAction($pdo, $license['id'], 'activation_failed_max', "Current: {$license['current_activations']}, Max: {$license['max_activations']}", $ipAddress);
        sendError("Maximum activations reached ({$license['max_activations']}). Please deactivate another device first.", 'max_activations');
    }

    // Create new activation
    $stmt = $pdo->prepare("
        INSERT INTO activations (license_id, machine_id, machine_name, os_type, os_version, app_version, ip_address)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    ");
    $stmt->execute([$license['id'], $machineId, $machineName, $osType, $osVersion, $appVersion, $ipAddress]);

    // Update activation count
    $stmt = $pdo->prepare("UPDATE licenses SET current_activations = current_activations + 1 WHERE id = ?");
    $stmt->execute([$license['id']]);

    logAction($pdo, $license['id'], 'activation_success', "Machine: $machineName, OS: $osType $osVersion", $ipAddress);

    sendSuccess('License activated successfully', [
        'email' => $license['email'],
        'customer_name' => $license['customer_name'],
        'license_type' => $license['license_type'],
        'expiry_date' => $license['expiry_date'],
        'max_activations' => $license['max_activations'],
        'current_activations' => $license['current_activations'] + 1
    ]);
}

/**
 * Handle license deactivation
 */
function handleDeactivation($pdo, $input) {
    $licenseKey = $input['license_key'] ?? '';
    $machineId = $input['machine_id'] ?? '';
    $ipAddress = $_SERVER['REMOTE_ADDR'] ?? '';

    if (empty($licenseKey) || empty($machineId)) {
        sendError('License key and machine ID are required', 'missing_params');
    }

    // Find the license
    $stmt = $pdo->prepare("SELECT * FROM licenses WHERE license_key = ?");
    $stmt->execute([$licenseKey]);
    $license = $stmt->fetch();

    if (!$license) {
        sendError('Invalid license key', 'invalid_key');
    }

    // Find and deactivate the machine
    $stmt = $pdo->prepare("
        UPDATE activations 
        SET is_active = 0, deactivated_at = NOW() 
        WHERE license_id = ? AND machine_id = ? AND is_active = 1
    ");
    $stmt->execute([$license['id'], $machineId]);

    if ($stmt->rowCount() > 0) {
        // Decrease activation count
        $stmt = $pdo->prepare("UPDATE licenses SET current_activations = GREATEST(current_activations - 1, 0) WHERE id = ?");
        $stmt->execute([$license['id']]);

        logAction($pdo, $license['id'], 'deactivation', "Machine: $machineId", $ipAddress);
        sendSuccess('License deactivated successfully');
    } else {
        sendError('No active activation found for this machine', 'not_found');
    }
}

/**
 * Handle license validation
 */
function handleValidation($pdo, $input) {
    $licenseKey = $input['license_key'] ?? '';
    $machineId = $input['machine_id'] ?? '';
    $ipAddress = $_SERVER['REMOTE_ADDR'] ?? '';

    if (empty($licenseKey) || empty($machineId)) {
        sendError('License key and machine ID are required', 'missing_params');
    }

    // Find the license
    $stmt = $pdo->prepare("SELECT * FROM licenses WHERE license_key = ?");
    $stmt->execute([$licenseKey]);
    $license = $stmt->fetch();

    if (!$license) {
        sendError('Invalid license key', 'invalid_key');
    }

    // Check if machine is activated
    $stmt = $pdo->prepare("SELECT * FROM activations WHERE license_id = ? AND machine_id = ? AND is_active = 1");
    $stmt->execute([$license['id'], $machineId]);
    $activation = $stmt->fetch();

    if (!$activation) {
        sendError('This machine is not activated', 'not_activated');
    }

    // Check license status
    if ($license['status'] !== 'active') {
        if ($license['status'] === 'expired') {
            sendError('License has expired', 'expired');
        } elseif ($license['status'] === 'revoked') {
            sendError('License has been revoked', 'revoked');
        } else {
            sendError('License is not active', 'inactive');
        }
    }

    // Check expiry date
    if ($license['expiry_date'] && strtotime($license['expiry_date']) < time()) {
        $stmt = $pdo->prepare("UPDATE licenses SET status = 'expired' WHERE id = ?");
        $stmt->execute([$license['id']]);
        sendError('License has expired', 'expired');
    }

    // Update last seen
    $stmt = $pdo->prepare("UPDATE activations SET last_seen = NOW(), ip_address = ? WHERE id = ?");
    $stmt->execute([$ipAddress, $activation['id']]);

    sendSuccess('License is valid', [
        'email' => $license['email'],
        'customer_name' => $license['customer_name'],
        'license_type' => $license['license_type'],
        'expiry_date' => $license['expiry_date'],
        'max_activations' => $license['max_activations'],
        'current_activations' => $license['current_activations']
    ]);
}

/**
 * Log an action
 */
function logAction($pdo, $licenseId, $action, $details, $ipAddress) {
    $userAgent = $_SERVER['HTTP_USER_AGENT'] ?? '';
    
    $stmt = $pdo->prepare("
        INSERT INTO license_logs (license_id, action, details, ip_address, user_agent)
        VALUES (?, ?, ?, ?, ?)
    ");
    $stmt->execute([$licenseId, $action, $details, $ipAddress, $userAgent]);
}

/**
 * Send success response
 */
function sendSuccess($message, $data = null) {
    $response = [
        'success' => true,
        'message' => $message
    ];
    
    if ($data !== null) {
        $response['data'] = $data;
    }
    
    echo json_encode($response);
    exit();
}

/**
 * Send error response
 */
function sendError($message, $errorCode = 'error') {
    echo json_encode([
        'success' => false,
        'message' => $message,
        'error_code' => $errorCode
    ]);
    exit();
}
