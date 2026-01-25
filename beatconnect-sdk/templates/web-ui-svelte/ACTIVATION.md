# Activation UI Integration (Svelte)

This directory contains Svelte templates for integrating BeatConnect license activation into your plugin.

## Files

- `src/lib/stores/activation.ts` - Svelte store for activation state management
- `src/lib/components/ActivationDialog.svelte` - Modal dialog for activation UI
- `src/lib/bridge.ts` - JUCE 8 WebView bridge utilities

## Integration Steps

### 1. Copy Files to Your Plugin

Copy the following to your `web-ui/src/lib/` directory:

```
lib/
├── stores/
│   └── activation.ts
├── components/
│   └── ActivationDialog.svelte
└── bridge.ts  (if not already present)
```

### 2. Initialize Activation Listeners

In your `main.ts`:

```typescript
import { initActivationListeners } from './lib/stores/activation';

// Call early in app initialization
initActivationListeners();
```

### 3. Add Dialog to App.svelte

```svelte
<script>
  import { showActivationDialog } from './lib/stores/activation';
  import ActivationDialog from './lib/components/ActivationDialog.svelte';
</script>

<!-- Your main app content -->
<main>
  <!-- ... -->
</main>

<!-- Activation dialog (shown when needed) -->
{#if $showActivationDialog}
  <ActivationDialog />
{/if}
```

### 4. Add C++ Event Handlers

In your `PluginEditor.cpp`, add event listeners:

```cpp
auto options = juce::WebBrowserComponent::Options()
    // ... other options ...
    .withEventListener("activateLicense", [this](const juce::var& data) {
        handleActivateLicense(data);
    })
    .withEventListener("deactivateLicense", [this](const juce::var& data) {
        handleDeactivateLicense(data);
    })
    .withEventListener("getActivationStatus", [this](const juce::var&) {
        handleGetActivationStatus();
    });
```

See the SDK's C++ templates for full handler implementations.

## Customization

### Styling

The `ActivationDialog.svelte` component includes scoped styles. To match your plugin's aesthetic:

1. Modify the CSS variables in the `<style>` section
2. Or override styles with a parent class

### Development Mode

In `initActivationListeners()`, set `showPreview = true` to test the dialog locally:

```typescript
const showPreview = true; // Set to true to preview activation dialog
activationStore.init({ isConfigured: showPreview, isActivated: !showPreview });
```

### Test Activation Code

In development mode, use `TEST-1234-5678-ABCD` or any 36-character UUID to simulate successful activation.

## C++ Requirements

Ensure your `CMakeLists.txt` includes:

```cmake
option(BEATCONNECT_ENABLE_ACTIVATION "Enable BeatConnect activation key support" OFF)

# When enabled, sets BEATCONNECT_ACTIVATION_ENABLED=1
if(BEATCONNECT_ENABLE_ACTIVATION)
    target_compile_definitions(${PROJECT_NAME} PUBLIC BEATCONNECT_ACTIVATION_ENABLED=1)
endif()
```

And your `PluginProcessor.cpp` configures the SDK:

```cpp
#if BEATCONNECT_ACTIVATION_ENABLED
#include <beatconnect/Activation.h>
#endif

void MyPluginProcessor::loadProjectData()
{
#if HAS_PROJECT_DATA && BEATCONNECT_ACTIVATION_ENABLED
    // Load project_data.json and configure SDK
    beatconnect::ActivationConfig config;
    config.apiBaseUrl = apiBaseUrl_.toStdString();
    config.pluginId = pluginId_.toStdString();
    config.supabaseKey = supabasePublishableKey_.toStdString();
    beatconnect::Activation::getInstance().configure(config);
#endif
}
```
