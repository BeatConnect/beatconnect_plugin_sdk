/**
 * usePedalOrder - Hook for managing pedal signal chain order
 *
 * Syncs the visual pedal order with C++ parameters for DSP processing.
 * Each slot stores which pedal is in that position.
 */

import { useState, useEffect, useCallback, useRef } from 'react';
import { getComboBoxState, isInJuceWebView } from '../lib/juce-bridge';

// Pedal IDs must match C++ PedalSlots::pedalIds order
export const PEDAL_IDS = ['distortion', 'chorus', 'tremolo', 'delay', 'reverb'] as const;
export type PedalId = typeof PEDAL_IDS[number];

// Slot parameter IDs
const SLOT_PARAM_IDS = [
  'pedal_slot_0',
  'pedal_slot_1',
  'pedal_slot_2',
  'pedal_slot_3',
  'pedal_slot_4',
] as const;

// Default order: distortion -> chorus -> tremolo -> delay -> reverb
const DEFAULT_ORDER: PedalId[] = ['distortion', 'chorus', 'tremolo', 'delay', 'reverb'];

interface PedalOrderReturn {
  /** Current pedal order (array of pedal IDs) */
  order: PedalId[];
  /** Set new pedal order */
  setOrder: (newOrder: PedalId[]) => void;
  /** Move a pedal from one index to another */
  movePedal: (fromIndex: number, toIndex: number) => void;
  /** Whether running in JUCE WebView */
  isConnected: boolean;
}

/**
 * Hook for managing pedal order with C++ sync
 */
export function usePedalOrder(): PedalOrderReturn {
  const [order, setOrderState] = useState<PedalId[]>(DEFAULT_ORDER);
  const isConnected = isInJuceWebView();

  // Keep refs to slot states
  const slotStatesRef = useRef(
    SLOT_PARAM_IDS.map(paramId => getComboBoxState(paramId))
  );

  // Convert pedal ID to index (matches C++ order)
  const pedalIdToIndex = (id: PedalId): number => PEDAL_IDS.indexOf(id);

  // Convert index to pedal ID
  const indexToPedalId = (index: number): PedalId => PEDAL_IDS[index] || 'distortion';

  // Read order from C++ parameters
  const readOrderFromCpp = useCallback((): PedalId[] => {
    return slotStatesRef.current.map(state => {
      const index = state.getChoiceIndex();
      return indexToPedalId(index);
    });
  }, []);

  // Write order to C++ parameters
  const writeOrderToCpp = useCallback((newOrder: PedalId[]) => {
    newOrder.forEach((pedalId, slotIndex) => {
      const pedalIndex = pedalIdToIndex(pedalId);
      slotStatesRef.current[slotIndex].setChoiceIndex(pedalIndex);
    });
  }, []);

  // Initialize from C++ on mount and listen for changes
  useEffect(() => {
    if (isConnected) {
      // Read initial order from C++
      const initialOrder = readOrderFromCpp();
      setOrderState(initialOrder);

      // Listen for changes from C++ (preset loads, etc.)
      const listeners = slotStatesRef.current.map((state) => {
        return state.valueChangedEvent.addListener(() => {
          // Re-read entire order when any slot changes
          const newOrder = readOrderFromCpp();
          setOrderState(newOrder);
        });
      });

      return () => {
        listeners.forEach((listenerId, i) => {
          slotStatesRef.current[i].valueChangedEvent.removeListener(listenerId);
        });
      };
    }
  }, [isConnected, readOrderFromCpp]);

  // Set complete new order
  const setOrder = useCallback((newOrder: PedalId[]) => {
    setOrderState(newOrder);
    if (isConnected) {
      writeOrderToCpp(newOrder);
    }
  }, [isConnected, writeOrderToCpp]);

  // Move a pedal from one slot to another (for drag-and-drop)
  const movePedal = useCallback((fromIndex: number, toIndex: number) => {
    setOrderState(currentOrder => {
      const newOrder = [...currentOrder];
      const [removed] = newOrder.splice(fromIndex, 1);
      newOrder.splice(toIndex, 0, removed);

      if (isConnected) {
        writeOrderToCpp(newOrder);
      }

      return newOrder;
    });
  }, [isConnected, writeOrderToCpp]);

  return { order, setOrder, movePedal, isConnected };
}

/**
 * Convert UI PedalConfig array to PedalId array
 */
export function pedalConfigsToIds(configs: { id: string }[]): PedalId[] {
  return configs.map(c => c.id as PedalId);
}

/**
 * Reorder PedalConfig array based on PedalId order
 */
export function reorderPedalConfigs<T extends { id: string }>(
  configs: T[],
  order: PedalId[]
): T[] {
  return order.map(id => configs.find(c => c.id === id)!).filter(Boolean);
}
