/*
  ==============================================================================
    {{PLUGIN_NAME}} - Plugin Editor Implementation
    BeatConnect Plugin Template

    Uses JUCE 8's native WebView relay system for reliable bidirectional
    parameter synchronization between C++ and the web UI.
  ==============================================================================
*/

#include "PluginEditor.h"
#include "ParameterIDs.h"

// Development server URL - Vite default port
// Set {{PLUGIN_NAME_UPPER}}_DEV_MODE=1 in CMakeLists.txt to use dev server
static constexpr const char* DEV_SERVER_URL = "http://localhost:5173";

//==============================================================================
{{PLUGIN_NAME}}Editor::{{PLUGIN_NAME}}Editor({{PLUGIN_NAME}}Processor& p)
    : AudioProcessorEditor(&p),
      processorRef(p)
{
    setupWebView();
    setupRelaysAndAttachments();

    // Set plugin window size
    setSize(800, 500);
    setResizable(false, false);

    // Start timer for sending visualizer/meter data to web UI (30 Hz)
    startTimerHz(30);
}

{{PLUGIN_NAME}}Editor::~{{PLUGIN_NAME}}Editor()
{
    stopTimer();
}

//==============================================================================
void {{PLUGIN_NAME}}Editor::setupWebView()
{
    // ===========================================================================
    // STEP 1: Create relays BEFORE creating WebBrowserComponent
    // ===========================================================================
    // Relays must exist before building WebView options.
    // The relay identifier must match the identifier used in your web code's
    // getSliderState(), getToggleState(), or getComboBoxState() calls.

    // Slider relays (continuous float parameters)
    gainRelay = std::make_unique<juce::WebSliderRelay>("gain");
    mixRelay = std::make_unique<juce::WebSliderRelay>("mix");

    // Toggle relays (boolean parameters)
    bypassRelay = std::make_unique<juce::WebToggleButtonRelay>("bypass");

    // ComboBox relays (choice parameters) - uncomment if needed:
    // modeRelay = std::make_unique<juce::WebComboBoxRelay>("mode");

    // ===========================================================================
    // STEP 2: Get resources directory for production builds
    // ===========================================================================
    resourcesDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                       .getParentDirectory()
                       .getChildFile("Resources")
                       .getChildFile("WebUI");

    // ===========================================================================
    // STEP 3: Build WebBrowserComponent with JUCE 8 options
    // ===========================================================================
    auto options = juce::WebBrowserComponent::Options()
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withNativeIntegrationEnabled()
        // Resource provider serves bundled web files in production
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

                // Determine MIME type
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
        // Register relays with WebView
        .withOptionsFrom(*gainRelay)
        .withOptionsFrom(*mixRelay)
        .withOptionsFrom(*bypassRelay)
        // .withOptionsFrom(*modeRelay)  // Uncomment if using
        // Windows-specific WebView2 options
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2()
                .withBackgroundColour(juce::Colour(0xff1a1a1a))
                .withStatusBarDisabled()
                .withUserDataFolder(
                    juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("{{PLUGIN_NAME}}_WebView2")));

    // Create the WebBrowserComponent
    webView = std::make_unique<juce::WebBrowserComponent>(options);
    addAndMakeVisible(*webView);

    // ===========================================================================
    // STEP 4: Load URL based on build mode
    // ===========================================================================
#if {{PLUGIN_NAME_UPPER}}_DEV_MODE
    // Development: connect to Vite dev server for hot reload
    webView->goToURL(DEV_SERVER_URL);
#else
    // Production: load from bundled resources via resource provider
    webView->goToURL(webView->getResourceProviderRoot());
#endif
}

//==============================================================================
void {{PLUGIN_NAME}}Editor::setupRelaysAndAttachments()
{
    // ===========================================================================
    // Connect relays to APVTS parameters
    // ===========================================================================
    // WebXxxParameterAttachment automatically:
    // - Syncs initial value from APVTS to relay (and thus to web)
    // - Updates APVTS when web changes the control
    // - Updates web when APVTS changes (e.g., automation)
    // - Handles drag start/end for proper undo/redo grouping

    auto& apvts = processorRef.getAPVTS();

    // Slider attachments
    gainAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::gain), *gainRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::mix), *mixRelay, nullptr);

    // Toggle attachments
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *apvts.getParameter(ParamIDs::bypass), *bypassRelay, nullptr);

    // ComboBox attachments - uncomment if using:
    // modeAttachment = std::make_unique<juce::WebComboBoxParameterAttachment>(
    //     *apvts.getParameter(ParamIDs::mode), *modeRelay, nullptr);
}

//==============================================================================
void {{PLUGIN_NAME}}Editor::timerCallback()
{
    sendVisualizerData();
}

void {{PLUGIN_NAME}}Editor::sendVisualizerData()
{
    // ===========================================================================
    // Send custom data to web (meters, visualizers, status)
    // ===========================================================================
    // This is for non-parameter data like level meters, waveforms, etc.
    // Use emitEventIfBrowserIsVisible() to avoid queueing events when hidden.

    if (!webView)
        return;

    // Build visualizer data object
    juce::DynamicObject::Ptr data = new juce::DynamicObject();

    // Example: send audio levels
    // data->setProperty("inputLevel", processorRef.getInputLevel());
    // data->setProperty("outputLevel", processorRef.getOutputLevel());

    // Example: send playback state
    // data->setProperty("isPlaying", processorRef.isHostPlaying());

    // Send to web UI
    webView->emitEventIfBrowserIsVisible("visualizerData", juce::var(data.get()));
}

//==============================================================================
void {{PLUGIN_NAME}}Editor::paint(juce::Graphics& g)
{
    // Background color (matches WebView2 background)
    g.fillAll(juce::Colour(0xff1a1a1a));
}

void {{PLUGIN_NAME}}Editor::resized()
{
    if (webView)
        webView->setBounds(getLocalBounds());
}
