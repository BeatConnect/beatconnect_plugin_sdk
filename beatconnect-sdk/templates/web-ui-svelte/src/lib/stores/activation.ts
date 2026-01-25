/**
 * Activation Store (Svelte)
 *
 * Manages license activation state and communication with C++ backend.
 * Use this store to integrate activation dialogs in Svelte-based plugins.
 */

import { writable, derived, get } from 'svelte/store';
import { isInJuceWebView } from '../bridge';

// Activation status codes (match C++ ActivationStatus enum)
export type ActivationStatus =
  | 'valid'
  | 'invalid'
  | 'revoked'
  | 'max_reached'
  | 'network_error'
  | 'server_error'
  | 'not_configured'
  | 'already_active'
  | 'not_activated';

export interface ActivationInfo {
  activationCode: string;
  machineId: string;
  activatedAt: string;
  expiresAt?: string;
  currentActivations: number;
  maxActivations: number;
  isValid: boolean;
}

export interface ActivationState {
  isConfigured: boolean;      // SDK configured (activation enabled in build)
  isActivated: boolean;       // Currently activated
  isLoading: boolean;         // Network operation in progress
  lastStatus?: ActivationStatus;
  lastError?: string;
  info?: ActivationInfo;
}

const initialState: ActivationState = {
  isConfigured: false,
  isActivated: false,
  isLoading: false,
};

// Main activation store
const createActivationStore = () => {
  const { subscribe, set, update } = writable<ActivationState>(initialState);

  return {
    subscribe,

    // Initialize from C++ state
    init: (state: Partial<ActivationState>) => {
      update(s => ({ ...s, ...state }));
    },

    // Set loading state
    setLoading: (loading: boolean) => {
      update(s => ({ ...s, isLoading: loading }));
    },

    // Handle activation result from C++
    handleActivationResult: (status: ActivationStatus, info?: ActivationInfo) => {
      update(s => ({
        ...s,
        isLoading: false,
        lastStatus: status,
        isActivated: status === 'valid' || status === 'already_active',
        info: info,
        lastError: status !== 'valid' && status !== 'already_active'
          ? getStatusMessage(status)
          : undefined,
      }));
    },

    // Handle deactivation result
    handleDeactivationResult: (status: ActivationStatus) => {
      update(s => ({
        ...s,
        isLoading: false,
        lastStatus: status,
        isActivated: status !== 'valid', // valid means deactivation succeeded
        info: status === 'valid' ? undefined : s.info,
        lastError: status !== 'valid' ? getStatusMessage(status) : undefined,
      }));
    },

    // Clear error
    clearError: () => {
      update(s => ({ ...s, lastError: undefined }));
    },

    // Reset store
    reset: () => set(initialState),
  };
};

export const activationStore = createActivationStore();

// Derived store for showing activation dialog
export const showActivationDialog = derived(
  activationStore,
  $state => $state.isConfigured && !$state.isActivated
);

// Status message helper
function getStatusMessage(status: ActivationStatus): string {
  switch (status) {
    case 'valid': return 'Activation successful';
    case 'invalid': return 'Invalid activation code';
    case 'revoked': return 'This license has been revoked';
    case 'max_reached': return 'Maximum activations reached for this license';
    case 'network_error': return 'Network error - please check your connection';
    case 'server_error': return 'Server error - please try again later';
    case 'not_configured': return 'Activation not configured';
    case 'already_active': return 'Already activated';
    case 'not_activated': return 'Not activated';
    default: return 'Unknown error';
  }
}

// API functions to communicate with C++
export function activate(code: string): void {
  if (!isInJuceWebView()) {
    // Mock for development
    console.log('[Activation] Mock activate:', code);
    setTimeout(() => {
      if (code === 'TEST-1234-5678-ABCD' || code.length === 36) {
        activationStore.handleActivationResult('valid', {
          activationCode: code,
          machineId: 'mock-machine-id',
          activatedAt: new Date().toISOString(),
          currentActivations: 1,
          maxActivations: 3,
          isValid: true,
        });
      } else {
        activationStore.handleActivationResult('invalid');
      }
    }, 1000);
    activationStore.setLoading(true);
    return;
  }

  activationStore.setLoading(true);
  window.__JUCE__!.backend.emitEvent('activateLicense', { code });
}

export function deactivate(): void {
  if (!isInJuceWebView()) {
    console.log('[Activation] Mock deactivate');
    setTimeout(() => {
      activationStore.handleDeactivationResult('valid');
    }, 500);
    activationStore.setLoading(true);
    return;
  }

  activationStore.setLoading(true);
  window.__JUCE__!.backend.emitEvent('deactivateLicense', {});
}

export function requestActivationStatus(): void {
  console.log('[Activation] requestActivationStatus() called');
  if (!isInJuceWebView()) {
    console.log('[Activation] Mock status request (not in JUCE WebView)');
    return;
  }

  console.log('[Activation] Emitting getActivationStatus event to C++');
  window.__JUCE__!.backend.emitEvent('getActivationStatus', {});
}

// Initialize listeners for C++ events
export function initActivationListeners(): void {
  console.log('[Activation] initActivationListeners called');
  console.log('[Activation] isInJuceWebView:', isInJuceWebView());

  if (!isInJuceWebView()) {
    console.log('[Activation] Development mode - activation dialog disabled');
    // In dev mode, skip activation (set showPreview=true to test dialog)
    const showPreview = false;
    activationStore.init({ isConfigured: showPreview, isActivated: !showPreview });
    return;
  }

  console.log('[Activation] Setting up C++ event listeners');

  // Listen for activation state updates from C++
  window.__JUCE__!.backend.addEventListener('activationState', (data: unknown) => {
    console.log('[Activation] activationState event received:', data);
    const state = data as {
      isConfigured: boolean;
      isActivated: boolean;
      info?: ActivationInfo;
    };
    console.log('[Activation] Dialog should show:', state.isConfigured && !state.isActivated);
    activationStore.init({
      isConfigured: state.isConfigured,
      isActivated: state.isActivated,
      info: state.info,
    });
  });

  // Listen for activation results
  window.__JUCE__!.backend.addEventListener('activationResult', (data: unknown) => {
    const result = data as { status: ActivationStatus; info?: ActivationInfo };
    console.log('[Activation] Result:', result);
    activationStore.handleActivationResult(result.status, result.info);
  });

  // Listen for deactivation results
  window.__JUCE__!.backend.addEventListener('deactivationResult', (data: unknown) => {
    const result = data as { status: ActivationStatus };
    console.log('[Activation] Deactivation result:', result);
    activationStore.handleDeactivationResult(result.status);
  });

  // Request initial state
  requestActivationStatus();
}
