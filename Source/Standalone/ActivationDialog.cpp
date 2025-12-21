#include "ActivationDialog.h"
#include "BinaryData.h"

//==============================================================================
// ActivationDialog
//==============================================================================

ActivationDialog::ActivationDialog()
{
    setSize(450, 400);  // Increased height for logo

    // Load logo from binary data
    loadLogoImage();

    // Title
    titleLabel.setText("MIDI Xplorer License Activation", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, textColour);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    // Status label
    statusLabel.setText("Enter your license key to activate", juce::dontSendNotification);
    statusLabel.setFont(juce::Font(14.0f));
    statusLabel.setColour(juce::Label::textColourId, textColour.withAlpha(0.7f));
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    // License key label
    licenseKeyLabel.setText("License Key:", juce::dontSendNotification);
    licenseKeyLabel.setFont(juce::Font(14.0f));
    licenseKeyLabel.setColour(juce::Label::textColourId, textColour);
    addAndMakeVisible(licenseKeyLabel);

    // License key editor
    licenseKeyEditor.setFont(juce::Font(16.0f, juce::Font::FontStyleFlags::plain));
    licenseKeyEditor.setTextToShowWhenEmpty("MIDI-XXXX-XXXX-XXXX-XXXX", juce::Colours::grey);
    licenseKeyEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff3a3a3a));
    licenseKeyEditor.setColour(juce::TextEditor::textColourId, textColour);
    licenseKeyEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff5a5a5a));
    licenseKeyEditor.setColour(juce::TextEditor::focusedOutlineColourId, accentColour);
    addAndMakeVisible(licenseKeyEditor);

    // Load existing license key if any
    juce::String existingKey = LicenseManager::getInstance().loadLicenseKey();
    if (existingKey.isNotEmpty())
    {
        licenseKeyEditor.setText(existingKey, false);
    }

    // Activate button
    activateButton.setButtonText("Activate License");
    activateButton.setColour(juce::TextButton::buttonColourId, accentColour);
    activateButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    activateButton.addListener(this);
    addAndMakeVisible(activateButton);

    // Deactivate button
    deactivateButton.setButtonText("Deactivate");
    deactivateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff5a5a5a));
    deactivateButton.addListener(this);
    addAndMakeVisible(deactivateButton);

    // Close button
    closeButton.setButtonText("Close");
    closeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a4a4a));
    closeButton.addListener(this);
    addAndMakeVisible(closeButton);

    // Info label
    infoLabel.setText("Need a license? Visit our website to purchase.", juce::dontSendNotification);
    infoLabel.setFont(juce::Font(12.0f));
    infoLabel.setColour(juce::Label::textColourId, textColour.withAlpha(0.6f));
    infoLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(infoLabel);

    // Purchase link
    purchaseLink.setButtonText("Purchase License");
    purchaseLink.setURL(juce::URL("https://midixplorer.com/purchase"));
    purchaseLink.setColour(juce::HyperlinkButton::textColourId, accentColour);
    addAndMakeVisible(purchaseLink);

    // Register as listener
    LicenseManager::getInstance().addListener(this);

    // Update UI based on current status
    updateUI();
}

ActivationDialog::~ActivationDialog()
{
    LicenseManager::getInstance().removeListener(this);
}

void ActivationDialog::loadLogoImage()
{
    // Load logo from binary data
    logoImage = juce::ImageCache::getFromMemory(BinaryData::logomidi_png, BinaryData::logomidi_pngSize);
}

void ActivationDialog::paint(juce::Graphics& g)
{
    g.fillAll(backgroundColour);

    // Draw logo at top center
    if (logoImage.isValid())
    {
        // Scale logo to fit within dialog width while maintaining aspect ratio
        int logoMaxWidth = getWidth() - 60;
        int logoMaxHeight = 50;  // Max height for logo

        float scale = juce::jmin((float)logoMaxWidth / logoImage.getWidth(),
                                  (float)logoMaxHeight / logoImage.getHeight());

        int logoWidth = (int)(logoImage.getWidth() * scale);
        int logoHeight = (int)(logoImage.getHeight() * scale);
        int logoX = (getWidth() - logoWidth) / 2;
        int logoY = 15;

        g.drawImage(logoImage, logoX, logoY, logoWidth, logoHeight,
                    0, 0, logoImage.getWidth(), logoImage.getHeight());
    }

    // Border
    g.setColour(juce::Colour(0xff4a4a4a));
    g.drawRect(getLocalBounds(), 1);
}

void ActivationDialog::resized()
{
    auto bounds = getLocalBounds().reduced(30);

    // Reserve space for logo at top
    bounds.removeFromTop(55);

    titleLabel.setBounds(bounds.removeFromTop(35));
    bounds.removeFromTop(10);

    statusLabel.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(25);

    licenseKeyLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(5);

    licenseKeyEditor.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(20);

    // Buttons row - layout based on visibility
    auto buttonArea = bounds.removeFromTop(40);

    if (activateButton.isVisible()) {
        activateButton.setBounds(buttonArea.removeFromLeft(150));
        buttonArea.removeFromLeft(10);
    }
    if (deactivateButton.isVisible()) {
        deactivateButton.setBounds(buttonArea.removeFromLeft(100));
        buttonArea.removeFromLeft(10);
    }
    closeButton.setBounds(buttonArea.removeFromLeft(80));

    bounds.removeFromTop(30);

    infoLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(5);
    purchaseLink.setBounds(bounds.removeFromTop(25));
}

void ActivationDialog::buttonClicked(juce::Button* button)
{
    if (button == &activateButton)
    {
        attemptActivation();
    }
    else if (button == &deactivateButton)
    {
        attemptDeactivation();
    }
    else if (button == &closeButton)
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        {
            dw->exitModalState(LicenseManager::getInstance().isLicenseValid() ? 1 : 0);
        }
    }
}

void ActivationDialog::attemptActivation()
{
    juce::String key = licenseKeyEditor.getText().trim();

    if (key.isEmpty())
    {
        statusLabel.setText("Please enter a license key", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, errorColour);
        return;
    }

    isProcessing = true;
    activateButton.setEnabled(false);
    statusLabel.setText("Activating license...", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, textColour.withAlpha(0.7f));

    LicenseManager::getInstance().activateLicense(key,
        [this](LicenseManager::LicenseStatus status, const juce::String& message)
    {
        isProcessing = false;
        activateButton.setEnabled(true);
        statusLabel.setText(message, juce::dontSendNotification);

        if (status == LicenseManager::LicenseStatus::Valid)
        {
            statusLabel.setColour(juce::Label::textColourId, successColour);

            // Close dialog after successful activation - use SafePointer for safety
            juce::Component::SafePointer<ActivationDialog> safeThis(this);
            juce::Timer::callAfterDelay(1500, [safeThis]()
            {
                if (safeThis != nullptr)
                {
                    if (auto* dw = safeThis->findParentComponentOfClass<juce::DialogWindow>())
                    {
                        dw->exitModalState(1);
                    }
                }
            });
        }
        else
        {
            statusLabel.setColour(juce::Label::textColourId, errorColour);
        }
    });
}

void ActivationDialog::attemptDeactivation()
{
    isProcessing = true;
    deactivateButton.setEnabled(false);
    statusLabel.setText("Deactivating license...", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, textColour.withAlpha(0.7f));

    LicenseManager::getInstance().deactivateLicense(
        [this](bool success, const juce::String& message)
    {
        isProcessing = false;
        deactivateButton.setEnabled(true);
        statusLabel.setText(message, juce::dontSendNotification);

        if (success)
        {
            statusLabel.setColour(juce::Label::textColourId, successColour);
            licenseKeyEditor.clear();
            updateUI();
        }
        else
        {
            statusLabel.setColour(juce::Label::textColourId, errorColour);
        }
    });
}

void ActivationDialog::updateUI()
{
    bool isValid = LicenseManager::getInstance().isLicenseValid();
    auto info = LicenseManager::getInstance().getLicenseInfo();

    if (isValid)
    {
        // License is active - hide activate button, show deactivate
        statusLabel.setText("License is active - " + info.licenseType, juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, successColour);
        activateButton.setVisible(false);
        deactivateButton.setVisible(true);
        deactivateButton.setEnabled(true);
        licenseKeyEditor.setEnabled(false);  // Can't edit when active
        licenseKeyEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
    }
    else
    {
        // No active license - show activate button
        activateButton.setVisible(true);
        deactivateButton.setVisible(info.licenseKey.isNotEmpty());  // Only show if there's a key to deactivate
        deactivateButton.setEnabled(info.licenseKey.isNotEmpty());
        licenseKeyEditor.setEnabled(true);
        licenseKeyEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff3a3a3a));
    }

    // Trigger layout update
    resized();
}

void ActivationDialog::licenseStatusChanged(LicenseManager::LicenseStatus status,
                                            const LicenseManager::LicenseInfo& info)
{
    updateUI();
}

void ActivationDialog::showActivationDialog(juce::Component* parent,
                                            std::function<void(bool)> callback)
{
    auto* dialog = new ActivationDialog();
    dialog->completionCallback = callback;

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(dialog);
    options.dialogTitle = "License Activation";
    options.dialogBackgroundColour = juce::Colour(0xff2a2a2a);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    auto* window = options.launchAsync();

    if (parent != nullptr)
    {
        window->centreAroundComponent(parent, window->getWidth(), window->getHeight());
    }
}

void ActivationDialog::showLicenseInfoDialog(juce::Component* parent)
{
    showActivationDialog(parent, nullptr);
}

//==============================================================================
// LicenseInfoPanel
//==============================================================================

LicenseInfoPanel::LicenseInfoPanel()
{
    statusLabel.setText("License Status: Unknown", juce::dontSendNotification);
    statusLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(statusLabel);

    licenseTypeLabel.setText("", juce::dontSendNotification);
    licenseTypeLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(licenseTypeLabel);

    customerLabel.setText("", juce::dontSendNotification);
    customerLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(customerLabel);

    expiryLabel.setText("", juce::dontSendNotification);
    expiryLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(expiryLabel);

    manageButton.setButtonText("Manage License");
    manageButton.onClick = [this]()
    {
        ActivationDialog::showLicenseInfoDialog(this);
    };
    addAndMakeVisible(manageButton);

    LicenseManager::getInstance().addListener(this);

    // Get initial status
    auto status = LicenseManager::getInstance().getCurrentStatus();
    auto info = LicenseManager::getInstance().getLicenseInfo();
    licenseStatusChanged(status, info);
}

LicenseInfoPanel::~LicenseInfoPanel()
{
    LicenseManager::getInstance().removeListener(this);
}

void LicenseInfoPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));
}

void LicenseInfoPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    statusLabel.setBounds(bounds.removeFromTop(20));
    licenseTypeLabel.setBounds(bounds.removeFromTop(20));
    customerLabel.setBounds(bounds.removeFromTop(20));
    expiryLabel.setBounds(bounds.removeFromTop(20));

    bounds.removeFromTop(10);
    manageButton.setBounds(bounds.removeFromTop(30).withWidth(120));
}

void LicenseInfoPanel::licenseStatusChanged(LicenseManager::LicenseStatus status,
                                            const LicenseManager::LicenseInfo& info)
{
    juce::String statusText = "License Status: ";
    juce::Colour statusColour;

    switch (status)
    {
        case LicenseManager::LicenseStatus::Valid:
            statusText += "Active";
            statusColour = juce::Colour(0xff4aff4a);
            break;
        case LicenseManager::LicenseStatus::Invalid:
            statusText += "Invalid";
            statusColour = juce::Colour(0xffff4a4a);
            break;
        case LicenseManager::LicenseStatus::Expired:
            statusText += "Expired";
            statusColour = juce::Colour(0xffffaa4a);
            break;
        case LicenseManager::LicenseStatus::Revoked:
            statusText += "Revoked";
            statusColour = juce::Colour(0xffff4a4a);
            break;
        default:
            statusText += "Not Activated";
            statusColour = juce::Colour(0xffaaaaaa);
            break;
    }

    statusLabel.setText(statusText, juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, statusColour);

    if (info.isValid)
    {
        licenseTypeLabel.setText("Type: " + info.licenseType, juce::dontSendNotification);
        customerLabel.setText("Customer: " + info.customerName, juce::dontSendNotification);

        if (info.expiryDate.isNotEmpty())
            expiryLabel.setText("Expires: " + info.expiryDate, juce::dontSendNotification);
        else
            expiryLabel.setText("Expires: Never", juce::dontSendNotification);
    }
    else
    {
        licenseTypeLabel.setText("", juce::dontSendNotification);
        customerLabel.setText("", juce::dontSendNotification);
        expiryLabel.setText("", juce::dontSendNotification);
    }
}
