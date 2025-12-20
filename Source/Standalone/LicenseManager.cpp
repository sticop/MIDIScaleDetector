#include "LicenseManager.h"
#include <juce_cryptography/juce_cryptography.h>

// Singleton instance
static std::unique_ptr<LicenseManager> licenseManagerInstance;

LicenseManager::LicenseManager()
{
    // Load any existing license on startup
    juce::String savedKey = loadLicenseKey();
    if (savedKey.isNotEmpty())
    {
        licenseInfo.licenseKey = savedKey;
    }
}

LicenseManager::~LicenseManager()
{
    stopTimer();
}

LicenseManager& LicenseManager::getInstance()
{
    if (!licenseManagerInstance)
    {
        licenseManagerInstance = std::make_unique<LicenseManager>();
    }
    return *licenseManagerInstance;
}

juce::String LicenseManager::getMachineId() const
{
    // Generate a unique machine ID based on system info
    juce::String systemInfo;

    #if JUCE_MAC
    // Use system serial number or UUID on Mac
    systemInfo = juce::SystemStats::getComputerName() +
                 juce::SystemStats::getUserName() +
                 juce::String(juce::SystemStats::getNumCpus());
    #elif JUCE_WINDOWS
    systemInfo = juce::SystemStats::getComputerName() +
                 juce::SystemStats::getUserName() +
                 juce::String(juce::SystemStats::getNumCpus());
    #else
    systemInfo = juce::SystemStats::getComputerName() +
                 juce::SystemStats::getUserName();
    #endif

    // Hash to create consistent ID
    return juce::SHA256(systemInfo.toUTF8()).toHexString().substring(0, 32);
}

juce::String LicenseManager::getMachineName() const
{
    return juce::SystemStats::getComputerName();
}

juce::String LicenseManager::getOSType() const
{
    #if JUCE_MAC
    return "macOS";
    #elif JUCE_WINDOWS
    return "Windows";
    #elif JUCE_LINUX
    return "Linux";
    #else
    return "Unknown";
    #endif
}

juce::String LicenseManager::getOSVersion() const
{
    return juce::SystemStats::getOperatingSystemName();
}

juce::String LicenseManager::getAppVersion() const
{
    return "1.0.0";
}

juce::File LicenseManager::getSettingsFile() const
{
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("MIDI Xplorer");
    appDataDir.createDirectory();
    return appDataDir.getChildFile("license.dat");
}

void LicenseManager::saveLicenseKey(const juce::String& key)
{
    auto file = getSettingsFile();

    // Simple obfuscation (not secure, but prevents casual viewing)
    juce::MemoryOutputStream stream;
    stream.writeString(key);

    juce::MemoryBlock block;
    block.append(stream.getData(), stream.getDataSize());

    // XOR with simple key
    for (size_t i = 0; i < block.getSize(); ++i)
    {
        static_cast<char*>(block.getData())[i] ^= 0x5A;
    }

    file.replaceWithData(block.getData(), block.getSize());
    licenseInfo.licenseKey = key;
}

juce::String LicenseManager::loadLicenseKey() const
{
    auto file = getSettingsFile();
    if (!file.existsAsFile())
        return {};

    juce::MemoryBlock block;
    if (!file.loadFileAsData(block))
        return {};

    // XOR to decode
    for (size_t i = 0; i < block.getSize(); ++i)
    {
        static_cast<char*>(block.getData())[i] ^= 0x5A;
    }

    juce::MemoryInputStream stream(block, false);
    return stream.readString();
}

void LicenseManager::clearLicenseKey()
{
    auto file = getSettingsFile();
    file.deleteFile();
    licenseInfo = LicenseInfo();
    currentStatus = LicenseStatus::Unknown;
}

void LicenseManager::sendPostRequest(const juce::String& endpoint, const juce::var& postData,
                                      std::function<void(int, const juce::var&)> callback)
{
    auto url = juce::URL(serverUrl + "/" + endpoint)
                   .withPOSTData(juce::JSON::toString(postData));

    // Create a thread to perform the request
    juce::Thread::launch([url, callback]()
    {
        juce::URL::InputStreamOptions options(juce::URL::ParameterHandling::inPostData);
        options.withExtraHeaders("Content-Type: application/json");
        options.withConnectionTimeoutMs(10000);

        auto stream = url.createInputStream(options);

        if (stream == nullptr)
        {
            juce::MessageManager::callAsync([callback]()
            {
                callback(-1, juce::var());
            });
            return;
        }

        auto response = stream->readEntireStreamAsString();
        int statusCode = dynamic_cast<juce::WebInputStream*>(stream.get())
                             ? dynamic_cast<juce::WebInputStream*>(stream.get())->getStatusCode()
                             : 200;

        auto jsonResult = juce::JSON::parse(response);

        juce::MessageManager::callAsync([callback, statusCode, jsonResult]()
        {
            callback(statusCode, jsonResult);
        });
    });
}

void LicenseManager::activateLicense(const juce::String& licenseKey,
                                      std::function<void(LicenseStatus, const juce::String&)> callback)
{
    juce::DynamicObject::Ptr postData = new juce::DynamicObject();
    postData->setProperty("action", "activate");
    postData->setProperty("license_key", licenseKey);
    postData->setProperty("machine_id", getMachineId());
    postData->setProperty("machine_name", getMachineName());
    postData->setProperty("os_type", getOSType());
    postData->setProperty("os_version", getOSVersion());
    postData->setProperty("app_version", getAppVersion());

    sendPostRequest("license.php", juce::var(postData.get()),
        [this, licenseKey, callback](int statusCode, const juce::var& response)
    {
        if (statusCode < 0)
        {
            currentStatus = LicenseStatus::NetworkError;
            callback(currentStatus, "Could not connect to license server. Please check your internet connection.");
            return;
        }

        if (statusCode != 200)
        {
            currentStatus = LicenseStatus::ServerError;
            callback(currentStatus, "Server error. Please try again later.");
            return;
        }

        bool success = response.getProperty("success", false);
        juce::String message = response.getProperty("message", "Unknown error").toString();

        if (success)
        {
            // Save the license key
            saveLicenseKey(licenseKey);

            // Update license info from response
            auto data = response.getProperty("data", juce::var());
            if (data.isObject())
            {
                licenseInfo.email = data.getProperty("email", "").toString();
                licenseInfo.customerName = data.getProperty("customer_name", "").toString();
                licenseInfo.licenseType = data.getProperty("license_type", "").toString();
                licenseInfo.expiryDate = data.getProperty("expiry_date", "").toString();
                licenseInfo.maxActivations = data.getProperty("max_activations", 3);
                licenseInfo.currentActivations = data.getProperty("current_activations", 1);
                licenseInfo.isValid = true;
            }

            currentStatus = LicenseStatus::Valid;
            callback(currentStatus, "License activated successfully!");

            // Notify listeners
            listeners.call([this](Listener& l) { l.licenseStatusChanged(currentStatus, licenseInfo); });
        }
        else
        {
            juce::String errorCode = response.getProperty("error_code", "").toString();

            if (errorCode == "max_activations")
                currentStatus = LicenseStatus::MaxActivationsReached;
            else if (errorCode == "expired")
                currentStatus = LicenseStatus::Expired;
            else if (errorCode == "revoked")
                currentStatus = LicenseStatus::Revoked;
            else
                currentStatus = LicenseStatus::Invalid;

            callback(currentStatus, message);
        }
    });
}

void LicenseManager::deactivateLicense(std::function<void(bool, const juce::String&)> callback)
{
    juce::String currentKey = loadLicenseKey();
    if (currentKey.isEmpty())
    {
        callback(false, "No license to deactivate.");
        return;
    }

    juce::DynamicObject::Ptr postData = new juce::DynamicObject();
    postData->setProperty("action", "deactivate");
    postData->setProperty("license_key", currentKey);
    postData->setProperty("machine_id", getMachineId());

    sendPostRequest("license.php", juce::var(postData.get()),
        [this, callback](int statusCode, const juce::var& response)
    {
        if (statusCode < 0)
        {
            callback(false, "Could not connect to license server.");
            return;
        }

        bool success = response.getProperty("success", false);
        juce::String message = response.getProperty("message", "Unknown error").toString();

        if (success)
        {
            clearLicenseKey();
            currentStatus = LicenseStatus::Unknown;
            listeners.call([this](Listener& l) { l.licenseStatusChanged(currentStatus, licenseInfo); });
        }

        callback(success, message);
    });
}

void LicenseManager::validateLicense(std::function<void(LicenseStatus, const LicenseInfo&)> callback)
{
    juce::String currentKey = loadLicenseKey();
    if (currentKey.isEmpty())
    {
        currentStatus = LicenseStatus::Invalid;
        callback(currentStatus, licenseInfo);
        return;
    }

    juce::DynamicObject::Ptr postData = new juce::DynamicObject();
    postData->setProperty("action", "validate");
    postData->setProperty("license_key", currentKey);
    postData->setProperty("machine_id", getMachineId());

    sendPostRequest("license.php", juce::var(postData.get()),
        [this, callback](int statusCode, const juce::var& response)
    {
        if (statusCode < 0)
        {
            // Network error - allow offline usage if previously validated
            if (licenseInfo.isValid)
            {
                currentStatus = LicenseStatus::Valid;
            }
            else
            {
                currentStatus = LicenseStatus::NetworkError;
            }
            callback(currentStatus, licenseInfo);
            return;
        }

        bool success = response.getProperty("success", false);

        if (success)
        {
            auto data = response.getProperty("data", juce::var());
            if (data.isObject())
            {
                licenseInfo.email = data.getProperty("email", "").toString();
                licenseInfo.customerName = data.getProperty("customer_name", "").toString();
                licenseInfo.licenseType = data.getProperty("license_type", "").toString();
                licenseInfo.expiryDate = data.getProperty("expiry_date", "").toString();
                licenseInfo.maxActivations = data.getProperty("max_activations", 3);
                licenseInfo.currentActivations = data.getProperty("current_activations", 1);
                licenseInfo.isValid = true;
            }

            currentStatus = LicenseStatus::Valid;
        }
        else
        {
            juce::String errorCode = response.getProperty("error_code", "").toString();

            if (errorCode == "expired")
                currentStatus = LicenseStatus::Expired;
            else if (errorCode == "revoked")
                currentStatus = LicenseStatus::Revoked;
            else
                currentStatus = LicenseStatus::Invalid;

            licenseInfo.isValid = false;
        }

        callback(currentStatus, licenseInfo);
        listeners.call([this](Listener& l) { l.licenseStatusChanged(currentStatus, licenseInfo); });
    });
}

void LicenseManager::addListener(Listener* listener)
{
    listeners.add(listener);
}

void LicenseManager::removeListener(Listener* listener)
{
    listeners.remove(listener);
}

void LicenseManager::startPeriodicValidation(int intervalSeconds)
{
    startTimer(intervalSeconds * 1000);
}

void LicenseManager::stopPeriodicValidation()
{
    stopTimer();
}

void LicenseManager::timerCallback()
{
    validateLicense([](LicenseStatus, const LicenseInfo&) {
        // Periodic validation callback - listeners are notified automatically
    });
}
