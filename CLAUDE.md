# CLAUDE.md - BeatConnect Plugin SDK

This file provides instructions for Claude Code when creating or working with VST3/AU plugins that integrate with the BeatConnect platform.

## Architecture Overview

BeatConnect plugins use a **Web/JUCE 8 Hybrid Architecture**:
- **C++ (JUCE 8)** - Audio processing, parameter management, state persistence
- **React/TypeScript** - User interface rendered in WebView2 (Windows) / WKWebView (macOS)
- **JUCE 8 Relay System** - Native bidirectional parameter sync (NOT postMessage!)

This enables:
- Hot reload during development (edit React, see changes instantly)
- Modern UI frameworks (React, Tailwind, etc.)
- Cross-platform consistency
- Proper undo/redo, automation, preset support

## Critical: JUCE 8 Relay System

**DO NOT use the old postMessage/evaluateJavascript approach.** JUCE 8 provides native relay classes that handle parameter sync properly:

| Relay Type | Parameter Type | C++ Class | TypeScript |
|------------|---------------|-----------|------------|
| Slider | Float | `WebSliderRelay` | `getSliderState()` |
| Toggle | Bool | `WebToggleButtonRelay` | `getToggleState()` |
| ComboBox | Choice | `WebComboBoxRelay` | `getComboBoxState()` |

### How It Works

1. **C++ creates relays** before WebBrowserComponent
2. **Relays are registered** with WebView options
3. **Attachments connect** relays to APVTS parameters
4. **TypeScript accesses** `window.__JUCE__.backend` for sync
5. **Events flow bidirectionally** - changes from either side sync automatically

## Project Structure

```
my-plugin/
├── CMakeLists.txt                 # Build config with NEEDS_WEBVIEW2
├── src/
│   ├── PluginProcessor.cpp        # Audio processing + APVTS
│   ├── PluginProcessor.h
│   ├── PluginEditor.cpp           # WebView + Relay setup
│   ├── PluginEditor.h
│   └── ParameterIDs.h             # Parameter ID constants
├── web/
│   ├── package.json
│   ├── vite.config.ts
│   ├── index.html
│   └── src/
│       ├── main.tsx
│       ├── App.tsx
│       ├── index.css
│       ├── lib/
│       │   └── juce-bridge.ts     # JUCE 8 frontend library
│       └── hooks/
│           └── useJuceParam.ts    # React parameter hooks
└── .github/workflows/build.yml
```

## Creating a New Plugin

### Step 1: Copy Templates

Copy all files from `templates/` to your new plugin directory.

### Step 2: Replace Placeholders

Replace these in ALL files:
- `{{PLUGIN_NAME}}` → PascalCase name (e.g., "SuperDelay")
- `{{PLUGIN_NAME_UPPER}}` → SCREAMING_SNAKE (e.g., "SUPER_DELAY")
- `{{COMPANY_NAME}}` → Company name (e.g., "BeatConnect")
- `{{COMPANY_NAME_LOWER}}` → lowercase (e.g., "beatconnect")
- `{{PLUGIN_CODE}}` → 4 chars (e.g., "Sdly")
- `{{MANUFACTURER_CODE}}` → 4 chars (e.g., "Beat")

### Step 3: Define Parameters

**ParameterIDs.h:**
```cpp
namespace ParamIDs {
    inline constexpr const char* gain = "gain";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* bypass = "bypass";
    inline constexpr const char* mode = "mode";
}
```

**PluginProcessor.cpp (createParameterLayout):**
```cpp
params.push_back(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID { ParamIDs::gain, 1 },
    "Gain",
    juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
    0.5f
));

params.push_back(std::make_unique<juce::AudioParameterBool>(
    juce::ParameterID { ParamIDs::bypass, 1 },
    "Bypass",
    false
));

params.push_back(std::make_unique<juce::AudioParameterChoice>(
    juce::ParameterID { ParamIDs::mode, 1 },
    "Mode",
    juce::StringArray { "Clean", "Warm", "Hot" },
    0
));
```

### Step 4: Create Relays (PluginEditor.cpp)

```cpp
void MyPluginEditor::setupWebView()
{
    // 1. Create relays FIRST (before WebBrowserComponent)
    gainRelay = std::make_unique<juce::WebSliderRelay>("gain");
    mixRelay = std::make_unique<juce::WebSliderRelay>("mix");
    bypassRelay = std::make_unique<juce::WebToggleButtonRelay>("bypass");
    modeRelay = std::make_unique<juce::WebComboBoxRelay>("mode");

    // 2. Build WebView options with relays
    auto options = juce::WebBrowserComponent::Options()
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withNativeIntegrationEnabled()
        .withResourceProvider([this](const juce::String& url) {
            // Serve bundled web files
            auto path = url.startsWith("/") ? url.substring(1) : url;
            if (path.isEmpty()) path = "index.html";
            auto file = resourcesDir.getChildFile(path);
            if (!file.existsAsFile()) return std::nullopt;
            // ... return file contents with MIME type
        })
        .withOptionsFrom(*gainRelay)
        .withOptionsFrom(*mixRelay)
        .withOptionsFrom(*bypassRelay)
        .withOptionsFrom(*modeRelay)
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2()
                .withBackgroundColour(juce::Colour(0xff1a1a1a))
                .withUserDataFolder(/* temp folder */));

    webView = std::make_unique<juce::WebBrowserComponent>(options);

    // 3. Load URL (dev server or bundled)
#if MYPLUGIN_DEV_MODE
    webView->goToURL("http://localhost:5173");
#else
    webView->goToURL(webView->getResourceProviderRoot());
#endif
}

void MyPluginEditor::setupRelaysAndAttachments()
{
    auto& apvts = processorRef.getAPVTS();

    gainAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::gain), *gainRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::mix), *mixRelay, nullptr);
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *apvts.getParameter(ParamIDs::bypass), *bypassRelay, nullptr);
    modeAttachment = std::make_unique<juce::WebComboBoxParameterAttachment>(
        *apvts.getParameter(ParamIDs::mode), *modeRelay, nullptr);
}
```

### Step 5: Use Parameters in React

**App.tsx:**
```tsx
import { useSliderParam, useToggleParam, useComboParam } from './hooks/useJuceParam';

function App() {
    // Hooks automatically sync with C++ relays
    const gain = useSliderParam('gain', { defaultValue: 0.5 });
    const bypass = useToggleParam('bypass');
    const mode = useComboParam('mode');

    return (
        <div>
            <input
                type="range"
                value={gain.value}
                onMouseDown={gain.onDragStart}
                onMouseUp={gain.onDragEnd}
                onChange={(e) => gain.setValue(parseFloat(e.target.value))}
            />
            <button onClick={bypass.toggle}>
                {bypass.value ? 'BYPASSED' : 'ACTIVE'}
            </button>
            <select
                value={mode.index}
                onChange={(e) => mode.setIndex(parseInt(e.target.value))}
            >
                {mode.choices.map((c, i) => <option key={i} value={i}>{c}</option>)}
            </select>
        </div>
    );
}
```

## CMakeLists.txt Requirements

```cmake
juce_add_plugin(${PROJECT_NAME}
    # ...
    NEEDS_WEBVIEW2 TRUE              # Critical for Windows
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
)

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1      # Windows only
        # Dev mode toggle
        $<IF:$<BOOL:${MYPLUGIN_DEV_MODE}>,MYPLUGIN_DEV_MODE=1,MYPLUGIN_DEV_MODE=0>
)
```

## Development Workflow

1. **Start web dev server:**
   ```bash
   cd web && npm install && npm run dev
   ```

2. **Build plugin in dev mode:**
   ```bash
   cmake -B build -DMYPLUGIN_DEV_MODE=ON
   cmake --build build
   ```

3. **Run Standalone** - it connects to localhost:5173 for hot reload

4. **Production build:**
   ```bash
   cd web && npm run build
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```

## Sending Non-Parameter Data (Visualizers)

For level meters, waveforms, and other real-time data:

**C++ (in timer callback):**
```cpp
void sendVisualizerData()
{
    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    data->setProperty("inputLevel", getInputLevel());
    data->setProperty("outputLevel", getOutputLevel());
    webView->emitEventIfBrowserIsVisible("visualizerData", juce::var(data.get()));
}
```

**TypeScript:**
```tsx
const viz = useVisualizerData<{ inputLevel: number }>('visualizerData', { inputLevel: 0 });
// Use viz.inputLevel in your meter component
```

## Common Mistakes to Avoid

1. **Creating WebBrowserComponent before relays** - Relays MUST exist first
2. **Mismatched identifiers** - "gain" in C++ must match "gain" in TypeScript exactly
3. **Using postMessage for parameters** - Use the relay system instead
4. **Forgetting NEEDS_WEBVIEW2** - Required for Windows WebView2 support
5. **Not calling dragStart/dragEnd** - Needed for proper undo/redo grouping

## BeatConnect Integration

See `docs/integration.md` for:
- Build pipeline setup
- Activation SDK (coming soon)
- Distribution integration

## File Templates

All templates are in `templates/`:
- `CMakeLists.txt` - Full build configuration
- `src/` - C++ source with relay setup
- `web/` - React UI with JUCE 8 hooks
- `.github/workflows/build.yml` - CI/CD workflow
