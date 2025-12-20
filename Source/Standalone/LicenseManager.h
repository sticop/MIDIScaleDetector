#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * License Manager for MIDI Xplorer
 * Handles license validation, activation, deactivation, and trial period
 */
class LicenseManager : public juce::Timer
{
public:
    // License status
    enum class LicenseStatus
    {
        Unknown,
        Valid,
        Trial,
        TrialExpired,
        Invalid,
        Expired,
        Revoked,
        MaxActivationsReached,
        NetworkError,
        ServerError
    };

    // License info structure
    struct LicenseInfo
    {
        juce::String licenseKey;
        juce::String email;
        juce::String customerName;
        juce::String licenseType;
        juce::String expiryDate;
        int maxActivations = 0;
        int currentActivations = 0;
        bool isValid = false;
    };

    // Trial info structure
    struct TrialInfo
    {
        juce::Time firstLaunchDate;
        int trialDays = 14;
        int daysRemaining = 14;
        bool isTrialActive = false;
        bool isTrialExpired = false;
    };

    // Listener interface for license status changes
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void licenseStatusChanged(LicenseStatus status, const LicenseInfo& info) = 0;
    };

    LicenseManager();
    ~LicenseManager() override;

    // Singleton access
    static LicenseManager& getInstance();

    // License operations
    void activateLicense(const juce::String& licenseKey, std::function<void(LicenseStatus, const juce::String&)> callback);
    void deactivateLicense(std::function<void(bool, const juce::String&)> callback);
    void validateLicense(std::function<void(LicenseStatus, const LicenseInfo&)> callback);

    // Status checks
    bool isLicenseValid() const { return currentStatus == LicenseStatus::Valid; }
    bool isTrialValid() const { return currentStatus == LicenseStatus::Trial; }
    bool isAppUsable() const { return currentStatus == LicenseStatus::Valid || currentStatus == LicenseStatus::Trial; }
    LicenseStatus getCurrentStatus() const { return currentStatus; }
    const LicenseInfo& getLicenseInfo() const { return licenseInfo; }
    const TrialInfo& getTrialInfo() const { return trialInfo; }

    // Trial management
    void initializeTrial();
    void checkTrialStatus();
    int getTrialDaysRemaining() const { return trialInfo.daysRemaining; }
    bool isInTrialPeriod() const { return trialInfo.isTrialActive && !trialInfo.isTrialExpired; }

    // License key storage
    void saveLicenseKey(const juce::String& key);
    juce::String loadLicenseKey() const;
    void clearLicenseKey();

    // Machine ID generation
    juce::String getMachineId() const;
    juce::String getMachineName() const;
    juce::String getOSType() const;
    juce::String getOSVersion() const;
    juce::String getAppVersion() const;

    // Listener management
    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    // Check license periodically
    void startPeriodicValidation(int intervalSeconds = 3600);
    void stopPeriodicValidation();

private:
    // Timer callback for periodic validation
    void timerCallback() override;

    // HTTP request helpers
    void sendPostRequest(const juce::String& endpoint, const juce::var& postData,
                         std::function<void(int, const juce::var&)> callback);

    // Server configuration
    const juce::String serverUrl = "https://reliablehandy.ca/midixplorer/api";

    // Current state
    LicenseStatus currentStatus = LicenseStatus::Unknown;
    LicenseInfo licenseInfo;
    TrialInfo trialInfo;

    // Listeners
    juce::ListenerList<Listener> listeners;

    // Settings file
    juce::File getSettingsFile() const;
    juce::File getTrialFile() const;

    // Trial file operations
    void saveTrialStartDate(juce::Time date);
    juce::Time loadTrialStartDate() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseManager)
};
