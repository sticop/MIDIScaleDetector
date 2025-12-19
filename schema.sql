-- MIDI Xplorer License Management Schema
-- Database: allonivf_midixplorer_licenses

CREATE TABLE IF NOT EXISTS licenses (
    id INT AUTO_INCREMENT PRIMARY KEY,
    license_key VARCHAR(64) NOT NULL UNIQUE,
    email VARCHAR(255) NOT NULL,
    customer_name VARCHAR(255),
    product_name VARCHAR(100) DEFAULT 'MIDI Xplorer',
    license_type ENUM('trial', 'personal', 'professional', 'enterprise') DEFAULT 'personal',
    status ENUM('active', 'expired', 'revoked', 'suspended') DEFAULT 'active',
    max_activations INT DEFAULT 3,
    current_activations INT DEFAULT 0,
    purchase_date DATETIME DEFAULT CURRENT_TIMESTAMP,
    expiry_date DATETIME NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_email (email),
    INDEX idx_license_key (license_key),
    INDEX idx_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS activations (
    id INT AUTO_INCREMENT PRIMARY KEY,
    license_id INT NOT NULL,
    machine_id VARCHAR(255) NOT NULL,
    machine_name VARCHAR(255),
    os_type VARCHAR(50),
    os_version VARCHAR(100),
    app_version VARCHAR(20),
    ip_address VARCHAR(45),
    activated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_seen DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    is_active BOOLEAN DEFAULT TRUE,
    deactivated_at DATETIME NULL,
    FOREIGN KEY (license_id) REFERENCES licenses(id) ON DELETE CASCADE,
    UNIQUE KEY unique_license_machine (license_id, machine_id),
    INDEX idx_machine_id (machine_id),
    INDEX idx_license_id (license_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS license_logs (
    id INT AUTO_INCREMENT PRIMARY KEY,
    license_id INT,
    action VARCHAR(50) NOT NULL,
    details TEXT,
    ip_address VARCHAR(45),
    user_agent VARCHAR(500),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (license_id) REFERENCES licenses(id) ON DELETE SET NULL,
    INDEX idx_license_id (license_id),
    INDEX idx_action (action),
    INDEX idx_created_at (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Insert a sample license for testing
INSERT INTO licenses (license_key, email, customer_name, license_type, status, max_activations)
VALUES ('MIDI-XPLR-TEST-0001-DEMO', 'test@example.com', 'Test User', 'trial', 'active', 1);
