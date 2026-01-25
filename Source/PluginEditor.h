/*
  ==============================================================================
    BeccaToneAmp - Plugin Editor (JUCE 8 WebView UI)
    Becca's signature guitar tone in a box

    Uses JUCE 8's native WebView relay system for bidirectional parameter sync.
  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

//==============================================================================
class BeccaToneAmpEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit BeccaToneAmpEditor(BeccaToneAmpProcessor&);
    ~BeccaToneAmpEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    //==============================================================================
    BeccaToneAmpProcessor& processorRef;

    //==============================================================================
    // WebView component
    std::unique_ptr<juce::WebBrowserComponent> webView;
    juce::File resourcesDir;

    //==============================================================================
    // JUCE 8 Parameter Relays - Amp Section
    std::unique_ptr<juce::WebSliderRelay> gainRelay;
    std::unique_ptr<juce::WebSliderRelay> bassRelay;
    std::unique_ptr<juce::WebSliderRelay> midRelay;
    std::unique_ptr<juce::WebSliderRelay> trebleRelay;
    std::unique_ptr<juce::WebSliderRelay> presenceRelay;
    std::unique_ptr<juce::WebSliderRelay> levelRelay;
    std::unique_ptr<juce::WebSliderRelay> volumeRelay;
    std::unique_ptr<juce::WebSliderRelay> compressionRelay;

    // Chorus pedal
    std::unique_ptr<juce::WebToggleButtonRelay> chorusEnabledRelay;
    std::unique_ptr<juce::WebSliderRelay> chorusRateRelay;
    std::unique_ptr<juce::WebSliderRelay> chorusDepthRelay;
    std::unique_ptr<juce::WebSliderRelay> chorusMixRelay;

    // Tremolo pedal
    std::unique_ptr<juce::WebToggleButtonRelay> tremoloEnabledRelay;
    std::unique_ptr<juce::WebSliderRelay> tremoloRateRelay;
    std::unique_ptr<juce::WebSliderRelay> tremoloDepthRelay;

    // Delay pedal
    std::unique_ptr<juce::WebToggleButtonRelay> delayEnabledRelay;
    std::unique_ptr<juce::WebSliderRelay> delayTimeRelay;
    std::unique_ptr<juce::WebSliderRelay> delayFeedbackRelay;
    std::unique_ptr<juce::WebSliderRelay> delayMixRelay;

    // Reverb pedal
    std::unique_ptr<juce::WebToggleButtonRelay> reverbEnabledRelay;
    std::unique_ptr<juce::WebSliderRelay> reverbSizeRelay;
    std::unique_ptr<juce::WebSliderRelay> reverbDampingRelay;
    std::unique_ptr<juce::WebSliderRelay> reverbMixRelay;

    // Distortion pedal
    std::unique_ptr<juce::WebToggleButtonRelay> distortionEnabledRelay;
    std::unique_ptr<juce::WebSliderRelay> distortionDriveRelay;
    std::unique_ptr<juce::WebSliderRelay> distortionToneRelay;
    std::unique_ptr<juce::WebSliderRelay> distortionLevelRelay;

    // Global
    std::unique_ptr<juce::WebToggleButtonRelay> bypassRelay;
    std::unique_ptr<juce::WebComboBoxRelay> presetRelay;

    // Pedal Slots (signal chain order)
    std::unique_ptr<juce::WebComboBoxRelay> pedalSlot0Relay;
    std::unique_ptr<juce::WebComboBoxRelay> pedalSlot1Relay;
    std::unique_ptr<juce::WebComboBoxRelay> pedalSlot2Relay;
    std::unique_ptr<juce::WebComboBoxRelay> pedalSlot3Relay;
    std::unique_ptr<juce::WebComboBoxRelay> pedalSlot4Relay;

    // Advanced/Expert Mode
    std::unique_ptr<juce::WebToggleButtonRelay> expertModeRelay;
    std::unique_ptr<juce::WebSliderRelay> micDistanceRelay;
    std::unique_ptr<juce::WebComboBoxRelay> cabStyleRelay;

    // 8-band EQ
    std::unique_ptr<juce::WebSliderRelay> eq60HzRelay;
    std::unique_ptr<juce::WebSliderRelay> eq120HzRelay;
    std::unique_ptr<juce::WebSliderRelay> eq250HzRelay;
    std::unique_ptr<juce::WebSliderRelay> eq500HzRelay;
    std::unique_ptr<juce::WebSliderRelay> eq1kHzRelay;
    std::unique_ptr<juce::WebSliderRelay> eq2kHzRelay;
    std::unique_ptr<juce::WebSliderRelay> eq4kHzRelay;
    std::unique_ptr<juce::WebSliderRelay> eq8kHzRelay;

    //==============================================================================
    // Parameter Attachments - Amp Section
    std::unique_ptr<juce::WebSliderParameterAttachment> gainAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> bassAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> midAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> trebleAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> presenceAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> levelAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> volumeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> compressionAttachment;

    // Chorus
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> chorusEnabledAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> chorusRateAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> chorusDepthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> chorusMixAttachment;

    // Tremolo
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> tremoloEnabledAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> tremoloRateAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> tremoloDepthAttachment;

    // Delay
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> delayEnabledAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> delayTimeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> delayFeedbackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> delayMixAttachment;

    // Reverb
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> reverbEnabledAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> reverbSizeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> reverbDampingAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> reverbMixAttachment;

    // Distortion
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> distortionEnabledAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> distortionDriveAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> distortionToneAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> distortionLevelAttachment;

    // Global
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> presetAttachment;

    // Pedal Slots
    std::unique_ptr<juce::WebComboBoxParameterAttachment> pedalSlot0Attachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> pedalSlot1Attachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> pedalSlot2Attachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> pedalSlot3Attachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> pedalSlot4Attachment;

    // Advanced
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> expertModeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> micDistanceAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> cabStyleAttachment;

    // 8-band EQ
    std::unique_ptr<juce::WebSliderParameterAttachment> eq60HzAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> eq120HzAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> eq250HzAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> eq500HzAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> eq1kHzAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> eq2kHzAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> eq4kHzAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> eq8kHzAttachment;

    //==============================================================================
    void setupWebView();
    void setupRelaysAndAttachments();
    void sendVisualizerData();

    //==============================================================================
    // Activation handlers
    void sendActivationState();
    void handleActivateLicense(const juce::var& data);
    void handleDeactivateLicense(const juce::var& data);
    void handleGetActivationStatus();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeccaToneAmpEditor)
};
