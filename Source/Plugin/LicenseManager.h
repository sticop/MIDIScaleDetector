#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

/**
 * License Manager for MIDI Xplorer
 * Handles license validation, activation, and deactivation
 */
class LicenseManager : public juce::Timer
{
public:
    static constexpr const char* LICENSE_API_URL = "https://valoraconsulting.net/midixplorerlicense/api.php";

    enum class LicenseStatus
    {
        Unknown,
        Valid,
        Invalid,
        Expired,
        NotActivated,
        MaxActivationsReached,
        NetworkError
    };

    struct LicenseInfo
    {
        juce::String licenseKey;
        juce::String customerName;
        juce::String expiresAt;
        int daysRemaining = 0;
        LicenseStatus status = LicenseStatus::Unknown;
        juce::String errorMessage;
    };

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void licenseStatusChanged(const LicenseInfo& info) = 0;
    };

    LicenseManager()
    {
        loadStoredLicense();
    }

    ~LicenseManager() override
    {
        stopTimer();
    }

    void addListener(Listener* listener) { listeners.add(listener); }
    void removeListener(Listener* listener) { listeners.remove(listener); }

    const LicenseInfo& getLicenseInfo() const { return currentLicense; }
    bool isLicensed() const { return currentLicense.status == LicenseStatus::Valid; }

    /**
     * Activate a license key on this machine
     */
    void activateLicense(const juce::String& licenseKey)
    {
        juce::Thread::launch([this, licenseKey]() {
            auto result = performActivation(licenseKey);
            juce::MessageManager::callAsync([this, result]() {
                currentLicense = result;
                if (result.status == LicenseStatus::Valid) {
                    storeLicense(result.licenseKey);
                    startTimer(CHECK_INTERVAL_MS);
                }
                notifyListeners();
            });
        });
    }

    /**
     * Deactivate the current license from this machine
     */
    void deactivateLicense()
    {
        if (currentLicense.licenseKey.isEmpty()) return;

        juce::Thread::launch([this]() {
            performDeactivation(currentLicense.licenseKey);
            juce::MessageManager::callAsync([this]() {
                clearStoredLicense();
                currentLicense = LicenseInfo();
                stopTimer();
                notifyListeners();
            });
        });
    }

    /**
     * Check if the current license is still valid
     */
    void checkLicense()
    {
        if (currentLicense.licenseKey.isEmpty()) return;

        juce::Thread::launch([this]() {
            auto result = performCheck(currentLicense.licenseKey);
            juce::MessageManager::callAsync([this, result]() {
                currentLicense = result;
                notifyListeners();
            });
        });
    }

    /**
     * Get a unique machine identifier
     */
    static juce::String getMachineId()
    {
        juce::Array<juce::MACAddress> macs;
        juce::MACAddress::findAllAddresses(macs);

        juce::String machineId;
        for (const auto& mac : macs) {
            machineId += mac.toString().removeCharacters(":");
        }

        // Add computer name for uniqueness
        machineId += "_" + juce::SystemStats::getComputerName();

        // Simple hash using String's built-in hash
        auto hash = machineId.hashCode64();
        return juce::String::toHexString(hash);
    }

    /**
     * Get first MAC address
     */
    static juce::String getMacAddress()
    {
        juce::Array<juce::MACAddress> macs;
        juce::MACAddress::findAllAddresses(macs);
        return macs.isEmpty() ? "" : macs[0].toString();
    }

private:
    static constexpr int CHECK_INTERVAL_MS = 24 * 60 * 60 * 1000; // Check daily

    LicenseInfo currentLicense;
    juce::ListenerList<Listener> listeners;

    void timerCallback() override
    {
        checkLicense();
    }

    void notifyListeners()
    {
        listeners.call([this](Listener& l) { l.licenseStatusChanged(currentLicense); });
    }

    LicenseInfo performActivation(const juce::String& licenseKey)
    {
        LicenseInfo result;
        result.licenseKey = licenseKey;

        juce::URL url(LICENSE_API_URL);
        url = url.withPOSTData(juce::String("action=activate")
            + "&license_key=" + juce::URL::addEscapeChars(licenseKey, true)
            + "&machine_id=" + juce::URL::addEscapeChars(getMachineId(), true)
            + "&mac_address=" + juce::URL::addEscapeChars(getMacAddress(), true)
            + "&hostname=" + juce::URL::addEscapeChars(juce::SystemStats::getComputerName(), true)
            + "&os_info=" + juce::URL::addEscapeChars(juce::SystemStats::getOperatingSystemName(), true));

        auto response = url.readEntireTextStream();

        if (response.isEmpty()) {
            result.status = LicenseStatus::NetworkError;
            result.errorMessage = "Network error - please check your connection";
            return result;
        }

        auto json = juce::JSON::parse(response);

        if (json.getProperty("success", false)) {
            result.status = LicenseStatus::Valid;
            result.customerName = json.getProperty("customer_name", "").toString();
            result.expiresAt = json.getProperty("expires_at", "").toString();
        } else {
            result.status = LicenseStatus::Invalid;
            result.errorMessage = json.getProperty("error", "Unknown error").toString();

            if (result.errorMessage.containsIgnoreCase("expired"))
                result.status = LicenseStatus::Expired;
            else if (result.errorMessage.containsIgnoreCase("maximum"))
                result.status = LicenseStatus::MaxActivationsReached;
        }

        return result;
    }

    void performDeactivation(const juce::String& licenseKey)
    {
        juce::URL url(LICENSE_API_URL);
        url = url.withPOSTData(juce::String("action=deactivate")
            + "&license_key=" + juce::URL::addEscapeChars(licenseKey, true)
            + "&machine_id=" + juce::URL::addEscapeChars(getMachineId(), true));

        url.readEntireTextStream(); // Ignore response
    }

    LicenseInfo performCheck(const juce::String& licenseKey)
    {
        LicenseInfo result;
        result.licenseKey = licenseKey;

        juce::URL url(LICENSE_API_URL);
        url = url.withPOSTData(juce::String("action=check")
            + "&license_key=" + juce::URL::addEscapeChars(licenseKey, true)
            + "&machine_id=" + juce::URL::addEscapeChars(getMachineId(), true));

        auto response = url.readEntireTextStream();

        if (response.isEmpty()) {
            // On network error during check, keep current status
            result.status = currentLicense.status;
            return result;
        }

        auto json = juce::JSON::parse(response);

        if (json.getProperty("valid", false)) {
            result.status = LicenseStatus::Valid;
            result.expiresAt = json.getProperty("expires_at", "").toString();
            result.daysRemaining = (int)json.getProperty("days_remaining", 0);
            result.customerName = currentLicense.customerName;
        } else {
            result.status = LicenseStatus::Invalid;
            result.errorMessage = json.getProperty("error", "License invalid").toString();
        }

        return result;
    }

    void loadStoredLicense()
    {
        auto settingsFile = getSettingsFile();
        if (settingsFile.existsAsFile()) {
            auto json = juce::JSON::parse(settingsFile);
            if (json.isObject()) {
                currentLicense.licenseKey = json.getProperty("license_key", "").toString();
                if (currentLicense.licenseKey.isNotEmpty()) {
                    currentLicense.status = LicenseStatus::Unknown;
                    // Verify on startup
                    checkLicense();
                    startTimer(CHECK_INTERVAL_MS);
                }
            }
        }
    }

    void storeLicense(const juce::String& licenseKey)
    {
        auto settingsFile = getSettingsFile();
        auto dir = settingsFile.getParentDirectory();
        if (!dir.exists()) dir.createDirectory();

        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("license_key", licenseKey);
        obj->setProperty("activated_at", juce::Time::getCurrentTime().toISO8601(true));

        settingsFile.replaceWithText(juce::JSON::toString(obj.get()));
    }

    void clearStoredLicense()
    {
        auto settingsFile = getSettingsFile();
        if (settingsFile.existsAsFile()) {
            settingsFile.deleteFile();
        }
    }

    static juce::File getSettingsFile()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("MIDI Xplorer")
            .getChildFile("license.json");
    }
};
