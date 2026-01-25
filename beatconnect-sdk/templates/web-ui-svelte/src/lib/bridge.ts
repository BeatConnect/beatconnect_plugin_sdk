/**
 * JUCE 8 Frontend Bridge Library (Svelte version)
 *
 * This module provides TypeScript wrappers for JUCE 8's native WebView integration.
 * It interfaces with window.__JUCE__ which is injected by the WebBrowserComponent.
 */

// ==============================================================================
// Type Definitions
// ==============================================================================

interface JuceBackend {
  addEventListener(eventId: string, fn: (payload: unknown) => void): [string, number];
  removeEventListener(token: [string, number]): void;
  emitEvent(eventId: string, payload: unknown): void;
}

interface JuceInitialisationData {
  __juce__platform: string[];
  __juce__functions: string[];
  __juce__registeredGlobalEventIds: string[];
  __juce__sliders: string[];
  __juce__toggles: string[];
  __juce__comboBoxes: string[];
}

declare global {
  interface Window {
    __JUCE__?: {
      backend: JuceBackend;
      initialisationData: JuceInitialisationData;
    };
  }
}

/**
 * Check if running inside JUCE WebView (vs browser development)
 */
export function isInJuceWebView(): boolean {
  return typeof window.__JUCE__ !== 'undefined' &&
         typeof window.__JUCE__.backend !== 'undefined';
}

/**
 * Listen for custom events from C++ (visualizers, activation, etc.)
 */
export function addCustomEventListener(
  eventId: string,
  callback: (data: unknown) => void
): () => void {
  if (!isInJuceWebView()) {
    console.log('[JUCE Bridge] Not in WebView, event listener ignored:', eventId);
    return () => {};
  }

  const token = window.__JUCE__!.backend.addEventListener(eventId, callback);

  return () => {
    window.__JUCE__!.backend.removeEventListener(token);
  };
}

/**
 * Emit event to C++ backend
 */
export function emitEvent(eventId: string, payload: unknown): void {
  if (!isInJuceWebView()) {
    console.log('[JUCE Bridge] Not in WebView, event not sent:', eventId, payload);
    return;
  }

  window.__JUCE__!.backend.emitEvent(eventId, payload);
}

export { };
