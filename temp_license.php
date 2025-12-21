<?php
/**
 * MIDI Xplorer License API
 * Endpoints for license validation, activation, and deactivation
 */

require_once __DIR__ . '/config.php';

header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST, GET, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    exit(0);
}

// Parse JSON body if Content-Type is application/json
$jsonInput = [];
$contentType = $_SERVER['CONTENT_TYPE'] ?? '';
if (strpos($contentType, 'application/json') !== false) {
    $rawBody = file_get_contents('php://input');
    $jsonInput = json_decode($rawBody, true) ?: [];
}

// Get action from query string, POST data, or JSON body
$action = $_GET['action'] ?? $_POST['action'] ?? $jsonInput['action'] ?? '';

// Merge JSON input with POST for compatibility
if (!empty($jsonInput)) {
    $_POST = array_merge($_POST, $jsonInput);
}

try {
    switch ($action) {
        case 'validate':
            validateLicense();
            break;
        case 'activate':
            activateLicense();
            break;
        case 'deactivate':
            deactivateLicense();
            break;
        case 'check':
            checkLicense();
            break;
        default:
            jsonResponse(['error' => 'Invalid action: ' . $action], 400);
    }
} catch (Exception $e) {
    error_log("API Error: " . $e->getMessage());
    jsonResponse(['error' => 'Server error'], 500);
}

/**
 * Validate a license key without activating
 */
function validateLicense() {
    $licenseKey = trim($_POST['license_key'] ?? '');

    if (!isValidLicenseFormat($licenseKey)) {
        jsonResponse(['valid' => false, 'error' => 'Invalid license format'], 400);
    }

    $db = getDB();
    $stmt = $db->prepare("SELECT * FROM licenses WHERE license_key = ?");
    $stmt->execute([$licenseKey]);
    $license = $stmt->fetch();

    if (!$license) {
        jsonResponse(['valid' => false, 'error' => 'License not found']);
    }

    if (!$license['is_active']) {
        jsonResponse(['valid' => false, 'error' => 'License is inactive']);
    }

    // Check expiry
    if ($license['expires_at'] && strtotime($license['expires_at']) < time()) {
        jsonResponse(['valid' => false, 'error' => 'License expired']);
    }

    jsonResponse([
        'valid' => true,
        'license_type' => $license['license_type'],
        'customer_name' => $license['customer_name'],
        'customer_email' => $license['customer_email'],
        'expires_at' => $license['expires_at'],
        'max_activations' => $license['max_activations'],
        'current_activations' => $license['current_activations']
    ]);
}

/**
 * Activate a license
 */
function activateLicense() {
    $licenseKey = trim($_POST['license_key'] ?? '');
    $machineId = trim($_POST['machine_id'] ?? '');
    $machineName = trim($_POST['machine_name'] ?? '');
    $osType = trim($_POST['os_type'] ?? '');
    $osVersion = trim($_POST['os_version'] ?? '');
    $appVersion = trim($_POST['app_version'] ?? '');

    if (!isValidLicenseFormat($licenseKey)) {
        jsonResponse(['success' => false, 'error' => 'Invalid license format'], 400);
    }

    if (empty($machineId)) {
        jsonResponse(['success' => false, 'error' => 'Machine ID required'], 400);
    }

    $db = getDB();

    // Get license
    $stmt = $db->prepare("SELECT * FROM licenses WHERE license_key = ?");
    $stmt->execute([$licenseKey]);
    $license = $stmt->fetch();

    if (!$license) {
        jsonResponse(['success' => false, 'error' => 'License not found']);
    }

    if (!$license['is_active']) {
        jsonResponse(['success' => false, 'error' => 'License is inactive']);
    }

    // Check expiry
    if ($license['expires_at'] && strtotime($license['expires_at']) < time()) {
        jsonResponse(['success' => false, 'error' => 'License expired']);
    }

    // Check if already activated on this machine
    $stmt = $db->prepare("SELECT * FROM activations WHERE license_id = ? AND machine_id = ?");
    $stmt->execute([$license['id'], $machineId]);
    $existing = $stmt->fetch();

    if ($existing) {
        // Update existing activation
        $stmt = $db->prepare("UPDATE activations SET last_seen = NOW(), app_version = ? WHERE id = ?");
        $stmt->execute([$appVersion, $existing['id']]);

        jsonResponse([
            'success' => true,
            'message' => 'License already activated on this machine',
            'license_type' => $license['license_type'],
            'customer_name' => $license['customer_name'],
            'expires_at' => $license['expires_at']
        ]);
    }

    // Check activation limit
    if ($license['current_activations'] >= $license['max_activations']) {
        jsonResponse(['success' => false, 'error' => 'Maximum activations reached']);
    }

    // Create new activation
    $stmt = $db->prepare("INSERT INTO activations (license_id, machine_id, machine_name, os_type, os_version, app_version, ip_address) VALUES (?, ?, ?, ?, ?, ?, ?)");
    $stmt->execute([$license['id'], $machineId, $machineName, $osType, $osVersion, $appVersion, getClientIP()]);

    // Increment activation count
    $stmt = $db->prepare("UPDATE licenses SET current_activations = current_activations + 1 WHERE id = ?");
    $stmt->execute([$license['id']]);

    jsonResponse([
        'success' => true,
        'message' => 'License activated successfully',
        'license_type' => $license['license_type'],
        'customer_name' => $license['customer_name'],
        'expires_at' => $license['expires_at']
    ]);
}

/**
 * Deactivate a license from a machine
 */
function deactivateLicense() {
    $licenseKey = trim($_POST['license_key'] ?? '');
    $machineId = trim($_POST['machine_id'] ?? '');

    if (!isValidLicenseFormat($licenseKey)) {
        jsonResponse(['success' => false, 'error' => 'Invalid license format'], 400);
    }

    $db = getDB();

    // Get license
    $stmt = $db->prepare("SELECT * FROM licenses WHERE license_key = ?");
    $stmt->execute([$licenseKey]);
    $license = $stmt->fetch();

    if (!$license) {
        jsonResponse(['success' => false, 'error' => 'License not found']);
    }

    // Delete activation
    $stmt = $db->prepare("DELETE FROM activations WHERE license_id = ? AND machine_id = ?");
    $stmt->execute([$license['id'], $machineId]);

    if ($stmt->rowCount() > 0) {
        // Decrement activation count
        $stmt = $db->prepare("UPDATE licenses SET current_activations = GREATEST(0, current_activations - 1) WHERE id = ?");
        $stmt->execute([$license['id']]);

        jsonResponse(['success' => true, 'message' => 'License deactivated successfully']);
    } else {
        jsonResponse(['success' => false, 'error' => 'Activation not found']);
    }
}

/**
 * Check license status (for periodic validation)
 */
function checkLicense() {
    $licenseKey = trim($_POST['license_key'] ?? '');
    $machineId = trim($_POST['machine_id'] ?? '');

    if (!isValidLicenseFormat($licenseKey)) {
        jsonResponse(['valid' => false, 'error' => 'Invalid license format'], 400);
    }

    $db = getDB();

    // Get license
    $stmt = $db->prepare("SELECT * FROM licenses WHERE license_key = ?");
    $stmt->execute([$licenseKey]);
    $license = $stmt->fetch();

    if (!$license) {
        jsonResponse(['valid' => false, 'error' => 'License not found']);
    }

    if (!$license['is_active']) {
        jsonResponse(['valid' => false, 'error' => 'License is inactive']);
    }

    if ($license['expires_at'] && strtotime($license['expires_at']) < time()) {
        jsonResponse(['valid' => false, 'error' => 'License expired']);
    }

    // Verify activation if machine_id provided
    if (!empty($machineId)) {
        $stmt = $db->prepare("SELECT * FROM activations WHERE license_id = ? AND machine_id = ?");
        $stmt->execute([$license['id'], $machineId]);
        $activation = $stmt->fetch();

        if (!$activation) {
            jsonResponse(['valid' => false, 'error' => 'Not activated on this machine']);
        }

        // Update last seen
        $stmt = $db->prepare("UPDATE activations SET last_seen = NOW() WHERE id = ?");
        $stmt->execute([$activation['id']]);
    }

    jsonResponse([
        'valid' => true,
        'license_type' => $license['license_type'],
        'customer_name' => $license['customer_name'],
        'expires_at' => $license['expires_at']
    ]);
}
