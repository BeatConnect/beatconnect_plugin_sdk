/*
  ==============================================================================
    BeccaToneAmp - Plugin Editor Implementation
    Becca's signature guitar tone in a box

    Uses JUCE 8's native WebView relay system for reliable bidirectional
    parameter synchronization between C++ and the web UI.
  ==============================================================================
*/

#include "PluginEditor.h"
#include "ParameterIDs.h"

#if BEATCONNECT_ACTIVATION_ENABLED
#include <beatconnect/Activation.h>
#endif

static constexpr const char* DEV_SERVER_URL = "http://localhost:5173";

//==============================================================================
BeccaToneAmpEditor::BeccaToneAmpEditor(BeccaToneAmpProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p)
{
    setupWebView();
    setupRelaysAndAttachments();

    // Set plugin window size (amp-style wide format)
    setSize(900, 600);
    setResizable(false, false);

    // Start timer for sending visualizer data (30 Hz)
    startTimerHz(30);
}

BeccaToneAmpEditor::~BeccaToneAmpEditor()
{
    stopTimer();
}

//==============================================================================
void BeccaToneAmpEditor::setupWebView()
{
    // ===========================================================================
    // STEP 1: Create relays BEFORE creating WebBrowserComponent
    // ===========================================================================

    // Amp Section
    gainRelay = std::make_unique<juce::WebSliderRelay>("gain");
    bassRelay = std::make_unique<juce::WebSliderRelay>("bass");
    midRelay = std::make_unique<juce::WebSliderRelay>("mid");
    trebleRelay = std::make_unique<juce::WebSliderRelay>("treble");
    presenceRelay = std::make_unique<juce::WebSliderRelay>("presence");
    levelRelay = std::make_unique<juce::WebSliderRelay>("level");
    volumeRelay = std::make_unique<juce::WebSliderRelay>("volume");
    compressionRelay = std::make_unique<juce::WebSliderRelay>("compression");

    // Chorus
    chorusEnabledRelay = std::make_unique<juce::WebToggleButtonRelay>("chorus_enabled");
    chorusRateRelay = std::make_unique<juce::WebSliderRelay>("chorus_rate");
    chorusDepthRelay = std::make_unique<juce::WebSliderRelay>("chorus_depth");
    chorusMixRelay = std::make_unique<juce::WebSliderRelay>("chorus_mix");

    // Tremolo
    tremoloEnabledRelay = std::make_unique<juce::WebToggleButtonRelay>("tremolo_enabled");
    tremoloRateRelay = std::make_unique<juce::WebSliderRelay>("tremolo_rate");
    tremoloDepthRelay = std::make_unique<juce::WebSliderRelay>("tremolo_depth");

    // Delay
    delayEnabledRelay = std::make_unique<juce::WebToggleButtonRelay>("delay_enabled");
    delayTimeRelay = std::make_unique<juce::WebSliderRelay>("delay_time");
    delayFeedbackRelay = std::make_unique<juce::WebSliderRelay>("delay_feedback");
    delayMixRelay = std::make_unique<juce::WebSliderRelay>("delay_mix");

    // Reverb
    reverbEnabledRelay = std::make_unique<juce::WebToggleButtonRelay>("reverb_enabled");
    reverbSizeRelay = std::make_unique<juce::WebSliderRelay>("reverb_size");
    reverbDampingRelay = std::make_unique<juce::WebSliderRelay>("reverb_damping");
    reverbMixRelay = std::make_unique<juce::WebSliderRelay>("reverb_mix");

    // Distortion
    distortionEnabledRelay = std::make_unique<juce::WebToggleButtonRelay>("distortion_enabled");
    distortionDriveRelay = std::make_unique<juce::WebSliderRelay>("distortion_drive");
    distortionToneRelay = std::make_unique<juce::WebSliderRelay>("distortion_tone");
    distortionLevelRelay = std::make_unique<juce::WebSliderRelay>("distortion_level");

    // Global
    bypassRelay = std::make_unique<juce::WebToggleButtonRelay>("bypass");
    presetRelay = std::make_unique<juce::WebComboBoxRelay>("preset");

    // Pedal Slots (signal chain order)
    pedalSlot0Relay = std::make_unique<juce::WebComboBoxRelay>("pedal_slot_0");
    pedalSlot1Relay = std::make_unique<juce::WebComboBoxRelay>("pedal_slot_1");
    pedalSlot2Relay = std::make_unique<juce::WebComboBoxRelay>("pedal_slot_2");
    pedalSlot3Relay = std::make_unique<juce::WebComboBoxRelay>("pedal_slot_3");
    pedalSlot4Relay = std::make_unique<juce::WebComboBoxRelay>("pedal_slot_4");

    // Advanced
    expertModeRelay = std::make_unique<juce::WebToggleButtonRelay>("expert_mode");
    micDistanceRelay = std::make_unique<juce::WebSliderRelay>("mic_distance");
    cabStyleRelay = std::make_unique<juce::WebComboBoxRelay>("cab_style");

    // 8-band EQ
    eq60HzRelay = std::make_unique<juce::WebSliderRelay>("eq_60hz");
    eq120HzRelay = std::make_unique<juce::WebSliderRelay>("eq_120hz");
    eq250HzRelay = std::make_unique<juce::WebSliderRelay>("eq_250hz");
    eq500HzRelay = std::make_unique<juce::WebSliderRelay>("eq_500hz");
    eq1kHzRelay = std::make_unique<juce::WebSliderRelay>("eq_1khz");
    eq2kHzRelay = std::make_unique<juce::WebSliderRelay>("eq_2khz");
    eq4kHzRelay = std::make_unique<juce::WebSliderRelay>("eq_4khz");
    eq8kHzRelay = std::make_unique<juce::WebSliderRelay>("eq_8khz");

    // ===========================================================================
    // STEP 2: Get resources directory - handle both Standalone and VST3 paths
    // ===========================================================================
    auto executableFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto executableDir = executableFile.getParentDirectory();

    // Try Standalone path first: executable/../Resources/WebUI
    resourcesDir = executableDir.getChildFile("Resources").getChildFile("WebUI");

    // If that doesn't exist, try VST3 path: executable/../../Resources/WebUI
    // VST3 structure: Plugin.vst3/Contents/x86_64-win/Plugin.vst3 (DLL)
    //                 Plugin.vst3/Contents/Resources/WebUI
    if (!resourcesDir.isDirectory())
    {
        resourcesDir = executableDir.getParentDirectory()
                           .getChildFile("Resources")
                           .getChildFile("WebUI");
    }

    // ===========================================================================
    // STEP 3: Build WebBrowserComponent with JUCE 8 options
    // ===========================================================================
    auto options = juce::WebBrowserComponent::Options()
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withNativeIntegrationEnabled()
        .withResourceProvider(
            [this](const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource>
            {
                auto path = url;
                if (path.startsWith("/"))
                    path = path.substring(1);
                if (path.isEmpty())
                    path = "index.html";

                auto file = resourcesDir.getChildFile(path);
                if (!file.existsAsFile())
                    return std::nullopt;

                juce::String mimeType = "application/octet-stream";
                if (path.endsWith(".html")) mimeType = "text/html";
                else if (path.endsWith(".css")) mimeType = "text/css";
                else if (path.endsWith(".js")) mimeType = "application/javascript";
                else if (path.endsWith(".json")) mimeType = "application/json";
                else if (path.endsWith(".png")) mimeType = "image/png";
                else if (path.endsWith(".jpg") || path.endsWith(".jpeg")) mimeType = "image/jpeg";
                else if (path.endsWith(".svg")) mimeType = "image/svg+xml";
                else if (path.endsWith(".woff")) mimeType = "font/woff";
                else if (path.endsWith(".woff2")) mimeType = "font/woff2";

                juce::MemoryBlock data;
                file.loadFileAsData(data);

                return juce::WebBrowserComponent::Resource{
                    std::vector<std::byte>(
                        reinterpret_cast<const std::byte*>(data.getData()),
                        reinterpret_cast<const std::byte*>(data.getData()) + data.getSize()),
                    mimeType.toStdString()
                };
            })
        // Register all relays
        .withOptionsFrom(*gainRelay)
        .withOptionsFrom(*bassRelay)
        .withOptionsFrom(*midRelay)
        .withOptionsFrom(*trebleRelay)
        .withOptionsFrom(*presenceRelay)
        .withOptionsFrom(*levelRelay)
        .withOptionsFrom(*volumeRelay)
        .withOptionsFrom(*compressionRelay)
        .withOptionsFrom(*chorusEnabledRelay)
        .withOptionsFrom(*chorusRateRelay)
        .withOptionsFrom(*chorusDepthRelay)
        .withOptionsFrom(*chorusMixRelay)
        .withOptionsFrom(*tremoloEnabledRelay)
        .withOptionsFrom(*tremoloRateRelay)
        .withOptionsFrom(*tremoloDepthRelay)
        .withOptionsFrom(*delayEnabledRelay)
        .withOptionsFrom(*delayTimeRelay)
        .withOptionsFrom(*delayFeedbackRelay)
        .withOptionsFrom(*delayMixRelay)
        .withOptionsFrom(*reverbEnabledRelay)
        .withOptionsFrom(*reverbSizeRelay)
        .withOptionsFrom(*reverbDampingRelay)
        .withOptionsFrom(*reverbMixRelay)
        .withOptionsFrom(*distortionEnabledRelay)
        .withOptionsFrom(*distortionDriveRelay)
        .withOptionsFrom(*distortionToneRelay)
        .withOptionsFrom(*distortionLevelRelay)
        .withOptionsFrom(*bypassRelay)
        .withOptionsFrom(*presetRelay)
        .withOptionsFrom(*pedalSlot0Relay)
        .withOptionsFrom(*pedalSlot1Relay)
        .withOptionsFrom(*pedalSlot2Relay)
        .withOptionsFrom(*pedalSlot3Relay)
        .withOptionsFrom(*pedalSlot4Relay)
        .withOptionsFrom(*expertModeRelay)
        .withOptionsFrom(*micDistanceRelay)
        .withOptionsFrom(*cabStyleRelay)
        .withOptionsFrom(*eq60HzRelay)
        .withOptionsFrom(*eq120HzRelay)
        .withOptionsFrom(*eq250HzRelay)
        .withOptionsFrom(*eq500HzRelay)
        .withOptionsFrom(*eq1kHzRelay)
        .withOptionsFrom(*eq2kHzRelay)
        .withOptionsFrom(*eq4kHzRelay)
        .withOptionsFrom(*eq8kHzRelay)
        // Activation event listeners
        .withEventListener("activateLicense", [this](const juce::var& data) {
            handleActivateLicense(data);
        })
        .withEventListener("deactivateLicense", [this](const juce::var& data) {
            handleDeactivateLicense(data);
        })
        .withEventListener("getActivationStatus", [this](const juce::var&) {
            handleGetActivationStatus();
        })
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2()
                .withBackgroundColour(juce::Colour(0xff1a1a1a))
                .withStatusBarDisabled()
                .withUserDataFolder(
                    juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("BeccaToneAmp_WebView2")));

    webView = std::make_unique<juce::WebBrowserComponent>(options);
    addAndMakeVisible(*webView);

    // ===========================================================================
    // STEP 4: Load URL based on build mode
    // ===========================================================================
#if BECCA_TONE_AMP_DEV_MODE
    webView->goToURL(DEV_SERVER_URL);
#else
    webView->goToURL(webView->getResourceProviderRoot());
#endif
}

//==============================================================================
void BeccaToneAmpEditor::setupRelaysAndAttachments()
{
    auto& apvts = processorRef.getAPVTS();

    // Amp Section
    gainAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::gain), *gainRelay, nullptr);
    bassAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::bass), *bassRelay, nullptr);
    midAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::mid), *midRelay, nullptr);
    trebleAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::treble), *trebleRelay, nullptr);
    presenceAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::presence), *presenceRelay, nullptr);
    levelAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::level), *levelRelay, nullptr);
    volumeAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::volume), *volumeRelay, nullptr);
    compressionAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::compression), *compressionRelay, nullptr);

    // Chorus
    chorusEnabledAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *apvts.getParameter(ParamIDs::chorusEnabled), *chorusEnabledRelay, nullptr);
    chorusRateAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::chorusRate), *chorusRateRelay, nullptr);
    chorusDepthAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::chorusDepth), *chorusDepthRelay, nullptr);
    chorusMixAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::chorusMix), *chorusMixRelay, nullptr);

    // Tremolo
    tremoloEnabledAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *apvts.getParameter(ParamIDs::tremoloEnabled), *tremoloEnabledRelay, nullptr);
    tremoloRateAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::tremoloRate), *tremoloRateRelay, nullptr);
    tremoloDepthAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::tremoloDepth), *tremoloDepthRelay, nullptr);

    // Delay
    delayEnabledAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *apvts.getParameter(ParamIDs::delayEnabled), *delayEnabledRelay, nullptr);
    delayTimeAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::delayTime), *delayTimeRelay, nullptr);
    delayFeedbackAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::delayFeedback), *delayFeedbackRelay, nullptr);
    delayMixAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::delayMix), *delayMixRelay, nullptr);

    // Reverb
    reverbEnabledAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *apvts.getParameter(ParamIDs::reverbEnabled), *reverbEnabledRelay, nullptr);
    reverbSizeAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::reverbSize), *reverbSizeRelay, nullptr);
    reverbDampingAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::reverbDamping), *reverbDampingRelay, nullptr);
    reverbMixAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::reverbMix), *reverbMixRelay, nullptr);

    // Distortion
    distortionEnabledAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *apvts.getParameter(ParamIDs::distortionEnabled), *distortionEnabledRelay, nullptr);
    distortionDriveAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::distortionDrive), *distortionDriveRelay, nullptr);
    distortionToneAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::distortionTone), *distortionToneRelay, nullptr);
    distortionLevelAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::distortionLevel), *distortionLevelRelay, nullptr);

    // Global
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *apvts.getParameter(ParamIDs::bypass), *bypassRelay, nullptr);
    presetAttachment = std::make_unique<juce::WebComboBoxParameterAttachment>(
        *apvts.getParameter(ParamIDs::preset), *presetRelay, nullptr);

    // Pedal Slots
    pedalSlot0Attachment = std::make_unique<juce::WebComboBoxParameterAttachment>(
        *apvts.getParameter(ParamIDs::pedalSlot0), *pedalSlot0Relay, nullptr);
    pedalSlot1Attachment = std::make_unique<juce::WebComboBoxParameterAttachment>(
        *apvts.getParameter(ParamIDs::pedalSlot1), *pedalSlot1Relay, nullptr);
    pedalSlot2Attachment = std::make_unique<juce::WebComboBoxParameterAttachment>(
        *apvts.getParameter(ParamIDs::pedalSlot2), *pedalSlot2Relay, nullptr);
    pedalSlot3Attachment = std::make_unique<juce::WebComboBoxParameterAttachment>(
        *apvts.getParameter(ParamIDs::pedalSlot3), *pedalSlot3Relay, nullptr);
    pedalSlot4Attachment = std::make_unique<juce::WebComboBoxParameterAttachment>(
        *apvts.getParameter(ParamIDs::pedalSlot4), *pedalSlot4Relay, nullptr);

    // Advanced
    expertModeAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *apvts.getParameter(ParamIDs::expertMode), *expertModeRelay, nullptr);
    micDistanceAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::micDistance), *micDistanceRelay, nullptr);
    cabStyleAttachment = std::make_unique<juce::WebComboBoxParameterAttachment>(
        *apvts.getParameter(ParamIDs::cabStyle), *cabStyleRelay, nullptr);

    // 8-band EQ
    eq60HzAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::eq60Hz), *eq60HzRelay, nullptr);
    eq120HzAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::eq120Hz), *eq120HzRelay, nullptr);
    eq250HzAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::eq250Hz), *eq250HzRelay, nullptr);
    eq500HzAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::eq500Hz), *eq500HzRelay, nullptr);
    eq1kHzAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::eq1kHz), *eq1kHzRelay, nullptr);
    eq2kHzAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::eq2kHz), *eq2kHzRelay, nullptr);
    eq4kHzAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::eq4kHz), *eq4kHzRelay, nullptr);
    eq8kHzAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::eq8kHz), *eq8kHzRelay, nullptr);
}

//==============================================================================
void BeccaToneAmpEditor::timerCallback()
{
    sendVisualizerData();
}

void BeccaToneAmpEditor::sendVisualizerData()
{
    if (!webView)
        return;

    juce::DynamicObject::Ptr data = new juce::DynamicObject();

    // Send audio levels for meters
    data->setProperty("inputLevel", processorRef.getInputLevel());
    data->setProperty("outputLevel", processorRef.getOutputLevel());

    // Send compressor gain reduction
    data->setProperty("gainReduction", processorRef.getGainReduction());

    // Send spectrum data if available
    if (processorRef.isSpectrumReady())
    {
        static constexpr int SPECTRUM_BINS = 128;
        std::array<float, BeccaToneAmpProcessor::NUM_SPECTRUM_BINS> fullSpectrum;
        processorRef.getSpectrum(fullSpectrum.data(), BeccaToneAmpProcessor::NUM_SPECTRUM_BINS);

        // Downsample to fewer bins for UI
        juce::Array<juce::var> spectrumArray;
        for (int i = 0; i < SPECTRUM_BINS; ++i)
        {
            int srcIdx = (i * BeccaToneAmpProcessor::NUM_SPECTRUM_BINS) / SPECTRUM_BINS;
            spectrumArray.add(fullSpectrum[srcIdx]);
        }
        data->setProperty("spectrum", spectrumArray);
    }

    webView->emitEventIfBrowserIsVisible("visualizerData", juce::var(data.get()));
}

//==============================================================================
void BeccaToneAmpEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
}

void BeccaToneAmpEditor::resized()
{
    if (webView)
        webView->setBounds(getLocalBounds());
}

//==============================================================================
// Activation Handlers
//==============================================================================

void BeccaToneAmpEditor::sendActivationState()
{
    if (!webView)
        return;

    juce::DynamicObject::Ptr data = new juce::DynamicObject();

#if BEATCONNECT_ACTIVATION_ENABLED
    auto& activation = processorRef.getActivation();  // Use instance member, NOT singleton

    // isConfigured = true when BEATCONNECT_ACTIVATION_ENABLED=1 and SDK was configured
    bool isConfigured = true;
    bool isActivated = activation.isActivated();

    data->setProperty("isConfigured", isConfigured);
    data->setProperty("isActivated", isActivated);

    if (isActivated)
    {
        if (auto info = activation.getActivationInfo())
        {
            juce::DynamicObject::Ptr infoObj = new juce::DynamicObject();
            infoObj->setProperty("activationCode", juce::String(info->activationCode));
            infoObj->setProperty("machineId", juce::String(info->machineId));
            infoObj->setProperty("activatedAt", juce::String(info->activatedAt));
            infoObj->setProperty("currentActivations", info->currentActivations);
            infoObj->setProperty("maxActivations", info->maxActivations);
            infoObj->setProperty("isValid", info->isValid);
            data->setProperty("info", juce::var(infoObj.get()));
        }
    }
#else
    // Activation not enabled - report as not configured (no dialog shown)
    data->setProperty("isConfigured", false);
    data->setProperty("isActivated", true);  // Allow full access when activation disabled
#endif

    webView->emitEventIfBrowserIsVisible("activationState", juce::var(data.get()));
}

void BeccaToneAmpEditor::handleActivateLicense(const juce::var& data)
{
#if BEATCONNECT_ACTIVATION_ENABLED
    juce::String code = data.getProperty("code", "").toString();
    if (code.isEmpty())
        return;

    // Use weak reference for async callback safety
    juce::Component::SafePointer<BeccaToneAmpEditor> safeThis(this);

    auto& activation = processorRef.getActivation();  // Use instance member, NOT singleton

    // Perform activation asynchronously
    activation.activateAsync(code.toStdString(),
        [safeThis](beatconnect::ActivationStatus status) {
            juce::MessageManager::callAsync([safeThis, status]() {
                if (!safeThis)
                    return;

                juce::DynamicObject::Ptr result = new juce::DynamicObject();

                juce::String statusStr;
                switch (status)
                {
                    case beatconnect::ActivationStatus::Valid:         statusStr = "valid"; break;
                    case beatconnect::ActivationStatus::Invalid:       statusStr = "invalid"; break;
                    case beatconnect::ActivationStatus::Revoked:       statusStr = "revoked"; break;
                    case beatconnect::ActivationStatus::MaxReached:    statusStr = "max_reached"; break;
                    case beatconnect::ActivationStatus::NetworkError:  statusStr = "network_error"; break;
                    case beatconnect::ActivationStatus::ServerError:   statusStr = "server_error"; break;
                    case beatconnect::ActivationStatus::NotConfigured: statusStr = "not_configured"; break;
                    case beatconnect::ActivationStatus::AlreadyActive: statusStr = "already_active"; break;
                    case beatconnect::ActivationStatus::NotActivated:  statusStr = "not_activated"; break;
                    default: statusStr = "unknown"; break;
                }
                result->setProperty("status", statusStr);

                // If successful, include activation info
                if (status == beatconnect::ActivationStatus::Valid ||
                    status == beatconnect::ActivationStatus::AlreadyActive)
                {
                    auto& activation = safeThis->processorRef.getActivation();  // Use instance member, NOT singleton
                    if (auto info = activation.getActivationInfo())
                    {
                        juce::DynamicObject::Ptr infoObj = new juce::DynamicObject();
                        infoObj->setProperty("activationCode", juce::String(info->activationCode));
                        infoObj->setProperty("machineId", juce::String(info->machineId));
                        infoObj->setProperty("activatedAt", juce::String(info->activatedAt));
                        infoObj->setProperty("currentActivations", info->currentActivations);
                        infoObj->setProperty("maxActivations", info->maxActivations);
                        infoObj->setProperty("isValid", info->isValid);
                        result->setProperty("info", juce::var(infoObj.get()));
                    }
                }

                safeThis->webView->emitEventIfBrowserIsVisible("activationResult", juce::var(result.get()));
            });
        });
#else
    juce::ignoreUnused(data);
#endif
}

void BeccaToneAmpEditor::handleDeactivateLicense([[maybe_unused]] const juce::var& data)
{
#if BEATCONNECT_ACTIVATION_ENABLED
    juce::Component::SafePointer<BeccaToneAmpEditor> safeThis(this);

    // Perform deactivation in background thread - capture processor ref for thread safety
    auto& activation = processorRef.getActivation();
    std::thread([safeThis, &activation]() {
        auto status = activation.deactivate();  // Use instance member, NOT singleton

        juce::MessageManager::callAsync([safeThis, status]() {
            if (!safeThis)
                return;

            juce::DynamicObject::Ptr result = new juce::DynamicObject();
            juce::String statusStr;
            switch (status)
            {
                case beatconnect::ActivationStatus::Valid:         statusStr = "valid"; break;
                case beatconnect::ActivationStatus::NetworkError:  statusStr = "network_error"; break;
                case beatconnect::ActivationStatus::ServerError:   statusStr = "server_error"; break;
                case beatconnect::ActivationStatus::NotActivated:  statusStr = "not_activated"; break;
                default: statusStr = "unknown"; break;
            }
            result->setProperty("status", statusStr);

            safeThis->webView->emitEventIfBrowserIsVisible("deactivationResult", juce::var(result.get()));
        });
    }).detach();
#endif
}

void BeccaToneAmpEditor::handleGetActivationStatus()
{
    sendActivationState();
}
