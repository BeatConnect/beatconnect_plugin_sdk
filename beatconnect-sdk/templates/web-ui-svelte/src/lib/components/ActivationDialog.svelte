<script lang="ts">
  /**
   * ActivationDialog - License activation screen for Svelte plugins
   *
   * Clean modal overlay for entering activation codes.
   * Customize the styling to match your plugin's aesthetic.
   */
  import { activationStore, activate, deactivate } from '../stores/activation';
  import { fade, scale } from 'svelte/transition';
  import { cubicOut } from 'svelte/easing';
  import { onMount } from 'svelte';

  let activationKey = '';
  let inputRef: HTMLInputElement;
  let shake = false;
  let unlocked = false;

  $: isLoading = $activationStore.isLoading;
  $: lastError = $activationStore.lastError;
  $: isActivated = $activationStore.isActivated;
  $: info = $activationStore.info;

  // UUID format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx (36 chars with dashes)
  // Legacy format: XXXX-XXXX-XXXX-XXXX (19 chars with dashes)
  const uuidRegex = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
  const legacyRegex = /^[A-Z0-9]{4}-[A-Z0-9]{4}-[A-Z0-9]{4}-[A-Z0-9]{4}$/;

  $: isCodeComplete = uuidRegex.test(activationKey) || legacyRegex.test(activationKey);

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

  function handleInput(event: Event) {
    const input = event.target as HTMLInputElement;
    activationKey = formatKey(input.value);
    activationStore.clearError();
  }

  function handleKeyDown(event: KeyboardEvent) {
    if (event.key === 'Enter' && isCodeComplete && !isLoading) {
      handleActivate();
    }
  }

  function triggerShake() {
    shake = true;
    setTimeout(() => shake = false, 500);
  }

  // Watch for errors and trigger shake
  $: if (lastError) triggerShake();

  function handleActivate() {
    if (!isCodeComplete || isLoading) return;
    activate(activationKey);
  }

  function handleDeactivate() {
    if (isLoading) return;
    deactivate();
  }

  // Watch for successful activation
  $: if (isActivated && info) {
    unlocked = true;
  }

  onMount(() => {
    setTimeout(() => inputRef?.focus(), 100);
  });
</script>

<div class="overlay" transition:fade={{ duration: 200 }}>
  <div class="container" class:shake>
    <!-- Lock Icon -->
    <div class="lock-container">
      <div class="lock-icon" class:unlocked>
        {#if unlocked}
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
            <path d="M8 11V7a4 4 0 018 0" />
            <rect x="3" y="11" width="18" height="11" rx="2" />
            <circle cx="12" cy="16" r="1" />
          </svg>
        {:else}
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
            <rect x="3" y="11" width="18" height="11" rx="2" />
            <path d="M7 11V7a5 5 0 0110 0v4" />
            <circle cx="12" cy="16" r="1" />
          </svg>
        {/if}
      </div>
    </div>

    <!-- Header -->
    <div class="header">
      <h1>{unlocked ? 'Activated' : 'Activate Plugin'}</h1>
      <p class="subtitle">
        {#if unlocked}
          Your license is active
        {:else if isActivated && info}
          License already activated
        {:else}
          Enter your activation key to unlock
        {/if}
      </p>
    </div>

    {#if isActivated && info}
      <!-- Activated State -->
      <div class="activated-state" transition:fade={{ duration: 150 }}>
        <div class="license-info">
          <div class="info-row">
            <span class="label">License</span>
            <span class="value mono">{info.activationCode.slice(0, 8)}...</span>
          </div>
          <div class="info-row">
            <span class="label">Activations</span>
            <span class="value">{info.currentActivations} / {info.maxActivations}</span>
          </div>
          <div class="info-row">
            <span class="label">Since</span>
            <span class="value">{new Date(info.activatedAt).toLocaleDateString()}</span>
          </div>
        </div>

        <button class="btn btn-ghost" on:click={handleDeactivate} disabled={isLoading}>
          {#if isLoading}
            <span class="spinner"></span>
          {/if}
          Deactivate This Machine
        </button>
        <p class="hint">Free up this activation slot for another computer</p>
      </div>
    {:else}
      <!-- Activation Form -->
      <div class="form" transition:fade={{ duration: 150 }}>
        <div class="input-group">
          <label for="activation-key">Activation Key</label>
          <input
            id="activation-key"
            type="text"
            placeholder="xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
            value={activationKey}
            on:input={handleInput}
            on:keydown={handleKeyDown}
            bind:this={inputRef}
            disabled={isLoading}
            class:error={lastError}
          />
        </div>

        {#if lastError}
          <p class="error-text" transition:fade={{ duration: 150 }}>
            {lastError}
          </p>
        {/if}

        <button
          class="btn btn-primary"
          on:click={handleActivate}
          disabled={!isCodeComplete || isLoading}
        >
          {#if isLoading}
            <span class="spinner"></span>
            Validating...
          {:else}
            Activate
          {/if}
        </button>
      </div>
    {/if}
  </div>
</div>

<style>
  .overlay {
    position: fixed;
    inset: 0;
    background: #09090b;
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 1000;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  }

  .container {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 24px;
    padding: 48px;
    max-width: 400px;
    width: 100%;
  }

  .container.shake {
    animation: shake 0.5s ease-in-out;
  }

  @keyframes shake {
    0%, 100% { transform: translateX(0); }
    10%, 30%, 50%, 70%, 90% { transform: translateX(-4px); }
    20%, 40%, 60%, 80% { transform: translateX(4px); }
  }

  /* Lock Icon */
  .lock-container {
    width: 80px;
    height: 80px;
    display: flex;
    align-items: center;
    justify-content: center;
  }

  .lock-icon {
    width: 64px;
    height: 64px;
    color: #52525b;
    transition: all 0.5s ease;
  }

  .lock-icon.unlocked {
    color: #22c55e;
    filter: drop-shadow(0 0 12px rgba(34, 197, 94, 0.4));
  }

  .lock-icon svg {
    width: 100%;
    height: 100%;
  }

  /* Header */
  .header {
    text-align: center;
  }

  h1 {
    color: #fafafa;
    font-size: 24px;
    font-weight: 600;
    margin: 0 0 8px;
    letter-spacing: -0.025em;
  }

  .subtitle {
    color: #71717a;
    font-size: 14px;
    margin: 0;
    line-height: 1.5;
  }

  /* Form */
  .form {
    width: 100%;
    display: flex;
    flex-direction: column;
    gap: 16px;
  }

  .input-group {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }

  label {
    color: #fafafa;
    font-size: 14px;
    font-weight: 500;
  }

  input {
    width: 100%;
    padding: 10px 12px;
    font-size: 14px;
    font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
    letter-spacing: 0.5px;
    background: #09090b;
    border: 1px solid #27272a;
    border-radius: 6px;
    color: #fafafa;
    outline: none;
    transition: border-color 0.15s, box-shadow 0.15s;
    box-sizing: border-box;
  }

  input:focus {
    border-color: #3f3f46;
    box-shadow: 0 0 0 2px rgba(63, 63, 70, 0.3);
  }

  input.error {
    border-color: #ef4444;
  }

  input::placeholder {
    color: #52525b;
  }

  input:disabled {
    opacity: 0.5;
  }

  .error-text {
    color: #ef4444;
    font-size: 13px;
    margin: 0;
  }

  /* Buttons */
  .btn {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    width: 100%;
    padding: 10px 16px;
    font-size: 14px;
    font-weight: 500;
    border: none;
    border-radius: 6px;
    cursor: pointer;
    transition: all 0.15s;
  }

  .btn-primary {
    background: #fafafa;
    color: #09090b;
  }

  .btn-primary:hover:not(:disabled) {
    background: #e4e4e7;
  }

  .btn-primary:disabled {
    background: #27272a;
    color: #71717a;
    cursor: not-allowed;
  }

  .btn-ghost {
    background: transparent;
    border: 1px solid #27272a;
    color: #a1a1aa;
  }

  .btn-ghost:hover:not(:disabled) {
    border-color: #3f3f46;
    color: #fafafa;
  }

  .btn-ghost:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  .spinner {
    width: 14px;
    height: 14px;
    border: 2px solid currentColor;
    border-top-color: transparent;
    border-radius: 50%;
    animation: spin 0.6s linear infinite;
  }

  @keyframes spin {
    to { transform: rotate(360deg); }
  }

  /* Activated State */
  .activated-state {
    width: 100%;
    display: flex;
    flex-direction: column;
    gap: 16px;
  }

  .license-info {
    background: #18181b;
    border: 1px solid #27272a;
    border-radius: 8px;
    padding: 16px;
  }

  .info-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 8px 0;
  }

  .info-row:not(:last-child) {
    border-bottom: 1px solid #27272a;
  }

  .info-row .label {
    color: #71717a;
    font-size: 12px;
    text-transform: uppercase;
    letter-spacing: 0.5px;
  }

  .info-row .value {
    color: #fafafa;
    font-size: 13px;
  }

  .info-row .value.mono {
    font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
    color: #22c55e;
  }

  .hint {
    color: #52525b;
    font-size: 12px;
    text-align: center;
    margin: 0;
  }
</style>
