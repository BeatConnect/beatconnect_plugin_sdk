/**
 * ActivationDialog - License activation screen (React)
 *
 * Clean modal overlay for entering activation codes.
 * Customize the styling to match your plugin's aesthetic.
 */

import { useState, useEffect, useRef } from 'react';
import { useActivation } from '../hooks/useActivation';

// UUID format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx (36 chars with dashes)
// Legacy format: XXXX-XXXX-XXXX-XXXX (19 chars with dashes)
const uuidRegex = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const legacyRegex = /^[A-Z0-9]{4}-[A-Z0-9]{4}-[A-Z0-9]{4}-[A-Z0-9]{4}$/;

// Format the key as user types
function formatKey(value: string): string {
  const trimmed = value.trim();

  // If it looks like a UUID (has enough hex chars), preserve UUID format
  const hexOnly = trimmed.replace(/[^a-fA-F0-9]/g, '');
  if (hexOnly.length >= 20) {
    const cleaned = trimmed.replace(/[^a-fA-F0-9-]/g, '').toLowerCase();
    if (uuidRegex.test(cleaned)) return cleaned;
    if (hexOnly.length >= 32) {
      return `${hexOnly.slice(0,8)}-${hexOnly.slice(8,12)}-${hexOnly.slice(12,16)}-${hexOnly.slice(16,20)}-${hexOnly.slice(20,32)}`.toLowerCase();
    }
    return cleaned;
  }

  // Legacy 16-char format - uppercase and format as XXXX-XXXX-XXXX-XXXX
  const cleaned = trimmed.replace(/[^a-zA-Z0-9]/g, '').toUpperCase();
  const parts = cleaned.match(/.{1,4}/g) || [];
  return parts.slice(0, 4).join('-');
}

export function ActivationDialog() {
  const {
    isActivated, isLoading, lastError, info,
    activate, deactivate, clearError
  } = useActivation();

  const [activationKey, setActivationKey] = useState('');
  const [shake, setShake] = useState(false);
  const [unlocked, setUnlocked] = useState(false);
  const inputRef = useRef<HTMLInputElement>(null);

  const isCodeComplete = uuidRegex.test(activationKey) || legacyRegex.test(activationKey);

  // Focus input on mount
  useEffect(() => {
    setTimeout(() => inputRef.current?.focus(), 100);
  }, []);

  // Watch for errors and trigger shake
  useEffect(() => {
    if (lastError) {
      setShake(true);
      setTimeout(() => setShake(false), 500);
    }
  }, [lastError]);

  // Watch for successful activation
  useEffect(() => {
    if (isActivated && info) {
      setUnlocked(true);
    }
  }, [isActivated, info]);

  function handleInput(event: React.ChangeEvent<HTMLInputElement>) {
    setActivationKey(formatKey(event.target.value));
    clearError();
  }

  function handleKeyDown(event: React.KeyboardEvent) {
    if (event.key === 'Enter' && isCodeComplete && !isLoading) {
      handleActivate();
    }
  }

  function handleActivate() {
    if (!isCodeComplete || isLoading) return;
    activate(activationKey);
  }

  function handleDeactivate() {
    if (isLoading) return;
    deactivate();
  }

  return (
    <div className="activation-overlay">
      <div className={`activation-container ${shake ? 'shake' : ''}`}>
        {/* Lock Icon */}
        <div className="activation-lock-container">
          <div className={`activation-lock-icon ${unlocked ? 'unlocked' : ''}`}>
            {unlocked ? (
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5">
                <path d="M8 11V7a4 4 0 018 0" />
                <rect x="3" y="11" width="18" height="11" rx="2" />
                <circle cx="12" cy="16" r="1" />
              </svg>
            ) : (
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5">
                <rect x="3" y="11" width="18" height="11" rx="2" />
                <path d="M7 11V7a5 5 0 0110 0v4" />
                <circle cx="12" cy="16" r="1" />
              </svg>
            )}
          </div>
        </div>

        {/* Header */}
        <div className="activation-header">
          <h1>{unlocked ? 'Activated' : 'Activate Plugin'}</h1>
          <p className="activation-subtitle">
            {unlocked
              ? 'Your license is active'
              : isActivated && info
              ? 'License already activated'
              : 'Enter your activation key to unlock'}
          </p>
        </div>

        {isActivated && info ? (
          /* Activated State */
          <div className="activation-activated-state">
            <div className="activation-license-info">
              <div className="activation-info-row">
                <span className="activation-label">License</span>
                <span className="activation-value mono">{info.activationCode.slice(0, 8)}...</span>
              </div>
              <div className="activation-info-row">
                <span className="activation-label">Activations</span>
                <span className="activation-value">{info.currentActivations} / {info.maxActivations}</span>
              </div>
              <div className="activation-info-row">
                <span className="activation-label">Since</span>
                <span className="activation-value">{new Date(info.activatedAt).toLocaleDateString()}</span>
              </div>
            </div>

            <button
              className="activation-btn activation-btn-ghost"
              onClick={handleDeactivate}
              disabled={isLoading}
            >
              {isLoading && <span className="activation-spinner" />}
              Deactivate This Machine
            </button>
            <p className="activation-hint">Free up this activation slot for another computer</p>
          </div>
        ) : (
          /* Activation Form */
          <div className="activation-form">
            <div className="activation-input-group">
              <label htmlFor="activation-key">Activation Key</label>
              <input
                id="activation-key"
                type="text"
                placeholder="xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
                value={activationKey}
                onChange={handleInput}
                onKeyDown={handleKeyDown}
                ref={inputRef}
                disabled={isLoading}
                className={lastError ? 'error' : ''}
              />
            </div>

            {lastError && (
              <p className="activation-error-text">{lastError}</p>
            )}

            <button
              className="activation-btn activation-btn-primary"
              onClick={handleActivate}
              disabled={!isCodeComplete || isLoading}
            >
              {isLoading ? (
                <>
                  <span className="activation-spinner" />
                  Validating...
                </>
              ) : (
                'Activate'
              )}
            </button>
          </div>
        )}
      </div>
    </div>
  );
}
