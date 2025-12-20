#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LicenseManager.h"

/**
 * License Activation Dialog
 * UI for entering and managing license keys
 */
class ActivationDialog : public juce::Component,
                         public juce::Button::Listener,
                         public LicenseManager::Listener
{
public:
    ActivationDialog();
    ~ActivationDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Button callback
    void buttonClicked(juce::Button* button) override;

    // License status callback
    void licenseStatusChanged(LicenseManager::LicenseStatus status,
                              const LicenseManager::LicenseInfo& info) override;

    // Show as modal dialog
    static void showActivationDialog(juce::Component* parent,
                                     std::function<void(bool)> callback);

    // Show license info dialog
    static void showLicenseInfoDialog(juce::Component* parent);

private:
    void updateUI();
    void attemptActivation();
    void attemptDeactivation();

    // UI Components
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label licenseKeyLabel;
    juce::TextEditor licenseKeyEditor;
    juce::TextButton activateButton;
    juce::TextButton deactivateButton;
    juce::TextButton closeButton;
    juce::Label infoLabel;
    juce::HyperlinkButton purchaseLink;

    // Progress indicator
    bool isProcessing = false;

    // Callback for modal result
    std::function<void(bool)> completionCallback;

    // Colors
    juce::Colour backgroundColour = juce::Colour(0xff2a2a2a);
    juce::Colour textColour = juce::Colour(0xffffffff);
    juce::Colour accentColour = juce::Colour(0xff4a9eff);
    juce::Colour errorColour = juce::Colour(0xffff4a4a);
    juce::Colour successColour = juce::Colour(0xff4aff4a);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ActivationDialog)
};

/**
 * License Info Panel - shows current license status
 */
class LicenseInfoPanel : public juce::Component,
                         public LicenseManager::Listener
{
public:
    LicenseInfoPanel();
    ~LicenseInfoPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void licenseStatusChanged(LicenseManager::LicenseStatus status,
                              const LicenseManager::LicenseInfo& info) override;

private:
    juce::Label statusLabel;
    juce::Label licenseTypeLabel;
    juce::Label customerLabel;
    juce::Label expiryLabel;
    juce::TextButton manageButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseInfoPanel)
};
