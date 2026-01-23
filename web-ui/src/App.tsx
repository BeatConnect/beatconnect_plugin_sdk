/**
 * Becca Tone Amp - Web UI
 *
 * Becca's signature guitar tone in a box.
 * Warm, buttery amp with pedals for soulful fusion, funk, R&B, and blues.
 */

import { useState, memo, useCallback } from 'react';
import { Reorder, motion, AnimatePresence, useDragControls } from 'framer-motion';
import { useSliderParam, useToggleParam, useComboParam, useVisualizerData } from './hooks/useJuceParam';
import { Knob } from './components/Knob';
import { Pedal } from './components/Pedal';
import { TubeGlow } from './components/TubeGlow';
import { PresetSelector } from './components/PresetSelector';
import { Visualizer } from './components/Visualizer';
import { CONTROL_TOOLTIPS } from './components/Tooltip';
import { LogoText } from './components/LogoText';
import { DragProvider, useDragContext } from './contexts/DragContext';
import { usePedalOrder, reorderPedalConfigs, pedalConfigsToIds, PedalId } from './hooks/usePedalOrder';
import { useSparks } from './components/Sparks';

// ==============================================================================
// Parameter IDs (must match C++ ParameterIDs.h)
// ==============================================================================
const ParamIDs = {
  gain: 'gain',
  bass: 'bass',
  mid: 'mid',
  treble: 'treble',
  presence: 'presence',
  level: 'level',
  volume: 'volume',
  compression: 'compression',
  chorusEnabled: 'chorus_enabled',
  chorusRate: 'chorus_rate',
  chorusDepth: 'chorus_depth',
  tremoloEnabled: 'tremolo_enabled',
  tremoloRate: 'tremolo_rate',
  tremoloDepth: 'tremolo_depth',
  delayEnabled: 'delay_enabled',
  delayTime: 'delay_time',
  delayMix: 'delay_mix',
  reverbEnabled: 'reverb_enabled',
  reverbSize: 'reverb_size',
  reverbMix: 'reverb_mix',
  distortionEnabled: 'distortion_enabled',
  distortionDrive: 'distortion_drive',
  distortionTone: 'distortion_tone',
  bypass: 'bypass',
  preset: 'preset',
} as const;

interface VisualizerData {
  inputLevel?: number;
  outputLevel?: number;
  gainReduction?: number;
  spectrum?: number[];
}

interface PedalConfig {
  id: string;
  name: string;
  color: string;
  fontClass: string;
}

const INITIAL_PEDAL_ORDER: PedalConfig[] = [
  { id: 'distortion', name: 'FUZZ', color: '#4a3a2a', fontClass: 'font-fuzz' },
  { id: 'chorus', name: 'CHORUS', color: '#3a4545', fontClass: 'font-chorus' },
  { id: 'tremolo', name: 'TREMOLO', color: '#3a453a', fontClass: 'font-tremolo' },
  { id: 'delay', name: 'DELAY', color: '#453a4a', fontClass: 'font-delay' },
  { id: 'reverb', name: 'REVERB', color: '#3a4550', fontClass: 'font-reverb' },
];

// Default values for all parameters (used for double-click reset)
const DEFAULT_VALUES = {
  gain: 0.35,
  bass: 0.6,
  mid: 0.55,
  treble: 0.4,
  presence: 0.5,
  level: 0.7,
  volume: 0.75,
  compression: 0.2,
  chorusRate: 0.18,
  chorusDepth: 0.3,
  tremoloRate: 0.37,
  tremoloDepth: 0.5,
  delayTime: 0.26,
  delayMix: 0.3,
  reverbSize: 0.4,
  reverbMix: 0.25,
  distortionDrive: 0.5,
  distortionTone: 0.5,
};

// Memoized pedal item to prevent re-renders during drag
interface PedalItemProps {
  pedal: PedalConfig;
  enabled: boolean;
  onToggle: () => void;
  knobs: Array<{
    id: string;
    label: string;
    value: number;
    onChange: (value: number) => void;
    onDragStart?: () => void;
    onDragEnd?: () => void;
    defaultValue?: number;
    tooltipTitle?: string;
    tooltipDescription?: string;
  }>;
  tooltipTitle?: string;
  tooltipDescription?: string;
}

const PedalItem = memo(function PedalItem({ pedal, enabled, onToggle, knobs, tooltipTitle, tooltipDescription }: PedalItemProps) {
  // Use drag controls to only allow dragging from specific areas (not knobs)
  const dragControls = useDragControls();

  // Only start drag if not clicking on a knob or interactive element
  const handleDragStart = (e: React.PointerEvent) => {
    const target = e.target as HTMLElement;
    // Check if we clicked on a knob or any interactive element
    const isKnob = target.closest('.knob, .knob-container, .pedal-knobs, .footswitch, .footswitch-button');
    if (!isKnob) {
      dragControls.start(e);
    }
  };

  return (
    <Reorder.Item
      value={pedal}
      className="pedal-wrapper"
      whileDrag={{ scale: 0.95, opacity: 0.8 }}
      transition={{ type: "spring", stiffness: 400, damping: 30 }}
      layout
      dragListener={false}
      dragControls={dragControls}
    >
      {/* Drag handle area - the pedal body (not the knobs) */}
      <div
        className="pedal-drag-handle"
        onPointerDown={handleDragStart}
      >
        <Pedal
          name={pedal.name}
          color={pedal.color}
          enabled={enabled}
          onToggle={onToggle}
          knobs={knobs}
          fontClass={pedal.fontClass}
          tooltipTitle={tooltipTitle}
          tooltipDescription={tooltipDescription}
        />
      </div>
    </Reorder.Item>
  );
});

function AppContent() {
  const [showVisualizer, setShowVisualizer] = useState(false);
  const { isDragging, setDragging } = useDragContext();

  // Pedal order synced with C++ DSP
  const { order: pedalOrderIds, setOrder: setPedalOrderIds } = usePedalOrder();

  // Convert PedalId order to PedalConfig order for Reorder.Group
  const pedalOrder = reorderPedalConfigs(INITIAL_PEDAL_ORDER, pedalOrderIds);

  // Handle reorder from UI - sync to C++
  const handlePedalReorder = useCallback((newConfigs: PedalConfig[]) => {
    const newIds = pedalConfigsToIds(newConfigs);
    setPedalOrderIds(newIds as PedalId[]);
  }, [setPedalOrderIds]);

  // Wrap drag callbacks to update context
  const createDragCallbacks = useCallback((onDragStart?: () => void, onDragEnd?: () => void) => ({
    onDragStart: () => {
      setDragging(true);
      onDragStart?.();
    },
    onDragEnd: () => {
      setDragging(false);
      onDragEnd?.();
    },
  }), [setDragging]);

  // Spark effects
  const metalSparks = useSparks('metal', 12);
  const orangeSparks = useSparks('orange', 10);
  const goldSparks = useSparks('gold', 8);

  // Amp parameters
  const gain = useSliderParam(ParamIDs.gain, { defaultValue: 0.35 });
  const bass = useSliderParam(ParamIDs.bass, { defaultValue: 0.6 });
  const mid = useSliderParam(ParamIDs.mid, { defaultValue: 0.55 });
  const treble = useSliderParam(ParamIDs.treble, { defaultValue: 0.4 });
  const presence = useSliderParam(ParamIDs.presence, { defaultValue: 0.5 });
  const level = useSliderParam(ParamIDs.level, { defaultValue: 0.7 });
  const volume = useSliderParam(ParamIDs.volume, { defaultValue: 0.75 });
  const compression = useSliderParam(ParamIDs.compression, { defaultValue: 0.2 });

  // Effects
  const chorusEnabled = useToggleParam(ParamIDs.chorusEnabled, { defaultValue: false });
  const chorusRate = useSliderParam(ParamIDs.chorusRate, { defaultValue: 0.18 });
  const chorusDepth = useSliderParam(ParamIDs.chorusDepth, { defaultValue: 0.3 });
  const tremoloEnabled = useToggleParam(ParamIDs.tremoloEnabled, { defaultValue: false });
  const tremoloRate = useSliderParam(ParamIDs.tremoloRate, { defaultValue: 0.37 });
  const tremoloDepth = useSliderParam(ParamIDs.tremoloDepth, { defaultValue: 0.5 });
  const delayEnabled = useToggleParam(ParamIDs.delayEnabled, { defaultValue: false });
  const delayTime = useSliderParam(ParamIDs.delayTime, { defaultValue: 0.26 });
  const delayMix = useSliderParam(ParamIDs.delayMix, { defaultValue: 0.3 });
  const reverbEnabled = useToggleParam(ParamIDs.reverbEnabled, { defaultValue: true });
  const reverbSize = useSliderParam(ParamIDs.reverbSize, { defaultValue: 0.4 });
  const reverbMix = useSliderParam(ParamIDs.reverbMix, { defaultValue: 0.25 });
  const distortionEnabled = useToggleParam(ParamIDs.distortionEnabled, { defaultValue: false });
  const distortionDrive = useSliderParam(ParamIDs.distortionDrive, { defaultValue: 0.5 });
  const distortionTone = useSliderParam(ParamIDs.distortionTone, { defaultValue: 0.5 });

  // Global
  const bypass = useToggleParam(ParamIDs.bypass, { defaultValue: false });
  const preset = useComboParam(ParamIDs.preset, { defaultIndex: 0 });

  // Visualizer data
  const vizData = useVisualizerData<VisualizerData>('visualizerData', {});

  // Calculate tube glow intensity for lighting effects
  const tubeIntensity = Math.max(vizData.inputLevel ?? 0, vizData.outputLevel ?? 0);

  const getPedalProps = (pedalId: string) => {
    switch (pedalId) {
      case 'distortion':
        return {
          enabled: distortionEnabled.value,
          onToggle: distortionEnabled.toggle,
          tooltipTitle: CONTROL_TOOLTIPS.distortion.title,
          tooltipDescription: CONTROL_TOOLTIPS.distortion.description,
          knobs: [
            { id: 'drive', label: 'Drive', value: distortionDrive.value, onChange: distortionDrive.setValue, onDragStart: distortionDrive.onDragStart, onDragEnd: distortionDrive.onDragEnd, defaultValue: DEFAULT_VALUES.distortionDrive, tooltipTitle: CONTROL_TOOLTIPS.distortionDrive.title, tooltipDescription: CONTROL_TOOLTIPS.distortionDrive.description },
            { id: 'tone', label: 'Tone', value: distortionTone.value, onChange: distortionTone.setValue, onDragStart: distortionTone.onDragStart, onDragEnd: distortionTone.onDragEnd, defaultValue: DEFAULT_VALUES.distortionTone, tooltipTitle: CONTROL_TOOLTIPS.distortionTone.title, tooltipDescription: CONTROL_TOOLTIPS.distortionTone.description },
          ],
        };
      case 'chorus':
        return {
          enabled: chorusEnabled.value,
          onToggle: chorusEnabled.toggle,
          tooltipTitle: CONTROL_TOOLTIPS.chorus.title,
          tooltipDescription: CONTROL_TOOLTIPS.chorus.description,
          knobs: [
            { id: 'rate', label: 'Rate', value: chorusRate.value, onChange: chorusRate.setValue, onDragStart: chorusRate.onDragStart, onDragEnd: chorusRate.onDragEnd, defaultValue: DEFAULT_VALUES.chorusRate, tooltipTitle: CONTROL_TOOLTIPS.chorusRate.title, tooltipDescription: CONTROL_TOOLTIPS.chorusRate.description },
            { id: 'depth', label: 'Depth', value: chorusDepth.value, onChange: chorusDepth.setValue, onDragStart: chorusDepth.onDragStart, onDragEnd: chorusDepth.onDragEnd, defaultValue: DEFAULT_VALUES.chorusDepth, tooltipTitle: CONTROL_TOOLTIPS.chorusDepth.title, tooltipDescription: CONTROL_TOOLTIPS.chorusDepth.description },
          ],
        };
      case 'tremolo':
        return {
          enabled: tremoloEnabled.value,
          onToggle: tremoloEnabled.toggle,
          tooltipTitle: CONTROL_TOOLTIPS.tremolo.title,
          tooltipDescription: CONTROL_TOOLTIPS.tremolo.description,
          knobs: [
            { id: 'rate', label: 'Rate', value: tremoloRate.value, onChange: tremoloRate.setValue, onDragStart: tremoloRate.onDragStart, onDragEnd: tremoloRate.onDragEnd, defaultValue: DEFAULT_VALUES.tremoloRate, tooltipTitle: CONTROL_TOOLTIPS.tremoloRate.title, tooltipDescription: CONTROL_TOOLTIPS.tremoloRate.description },
            { id: 'depth', label: 'Depth', value: tremoloDepth.value, onChange: tremoloDepth.setValue, onDragStart: tremoloDepth.onDragStart, onDragEnd: tremoloDepth.onDragEnd, defaultValue: DEFAULT_VALUES.tremoloDepth, tooltipTitle: CONTROL_TOOLTIPS.tremoloDepth.title, tooltipDescription: CONTROL_TOOLTIPS.tremoloDepth.description },
          ],
        };
      case 'delay':
        return {
          enabled: delayEnabled.value,
          onToggle: delayEnabled.toggle,
          tooltipTitle: CONTROL_TOOLTIPS.delay.title,
          tooltipDescription: CONTROL_TOOLTIPS.delay.description,
          knobs: [
            { id: 'time', label: 'Time', value: delayTime.value, onChange: delayTime.setValue, onDragStart: delayTime.onDragStart, onDragEnd: delayTime.onDragEnd, defaultValue: DEFAULT_VALUES.delayTime, tooltipTitle: CONTROL_TOOLTIPS.delayTime.title, tooltipDescription: CONTROL_TOOLTIPS.delayTime.description },
            { id: 'mix', label: 'Mix', value: delayMix.value, onChange: delayMix.setValue, onDragStart: delayMix.onDragStart, onDragEnd: delayMix.onDragEnd, defaultValue: DEFAULT_VALUES.delayMix, tooltipTitle: CONTROL_TOOLTIPS.delayMix.title, tooltipDescription: CONTROL_TOOLTIPS.delayMix.description },
          ],
        };
      case 'reverb':
        return {
          enabled: reverbEnabled.value,
          onToggle: reverbEnabled.toggle,
          tooltipTitle: CONTROL_TOOLTIPS.reverb.title,
          tooltipDescription: CONTROL_TOOLTIPS.reverb.description,
          knobs: [
            { id: 'size', label: 'Size', value: reverbSize.value, onChange: reverbSize.setValue, onDragStart: reverbSize.onDragStart, onDragEnd: reverbSize.onDragEnd, defaultValue: DEFAULT_VALUES.reverbSize, tooltipTitle: CONTROL_TOOLTIPS.reverbSize.title, tooltipDescription: CONTROL_TOOLTIPS.reverbSize.description },
            { id: 'mix', label: 'Mix', value: reverbMix.value, onChange: reverbMix.setValue, onDragStart: reverbMix.onDragStart, onDragEnd: reverbMix.onDragEnd, defaultValue: DEFAULT_VALUES.reverbMix, tooltipTitle: CONTROL_TOOLTIPS.reverbMix.title, tooltipDescription: CONTROL_TOOLTIPS.reverbMix.description },
          ],
        };
      default:
        return { enabled: false, onToggle: () => {}, knobs: [], tooltipTitle: '', tooltipDescription: '' };
    }
  };

  // Initialize/reset current preset
  const handleInitializePreset = () => {
    // Re-select the current preset to re-apply its values
    preset.setIndex(preset.index);
  };

  const isOn = !bypass.value;

  return (
    <div
      className={`plugin-container ${bypass.value ? 'bypassed' : ''}`}
      style={{ '--tube-intensity': tubeIntensity } as React.CSSProperties}
    >
      {/* Compact Header */}
      <header className="header">
        <div className="header-left">
          <PresetSelector
            currentIndex={preset.index}
            onSelect={preset.setIndex}
            onInitialize={handleInitializePreset}
            choices={preset.choices}
          />
        </div>
      </header>

      {/* Main Amp Unit */}
      <main className="main-content">
        <section
          className="amp-cabinet"
          onClick={(e) => {
            const target = e.target as HTMLElement;
            // Only trigger metal sparks on backplate elements (grill-controls background, amp-cabinet)
            // Exclude knobs, tubes, logo, pedals, and other interactive elements
            const isBackplate =
              target.classList.contains('amp-cabinet') ||
              target.classList.contains('grill-controls') ||
              target.classList.contains('amp-grill') ||
              target.classList.contains('grill-cloth') ||
              target.classList.contains('grill-tube-glow');

            if (isBackplate) {
              const rect = e.currentTarget.getBoundingClientRect();
              metalSparks.triggerSparks(e.clientX - rect.left, e.clientY - rect.top);
            }
          }}
        >
          {metalSparks.SparkElements}

          {/* Grill Section - Logo at top, controls mounted on grill */}
          <div className="amp-grill">
            <div className="grill-cloth" />

            {/* Tube glow reflection on grill */}
            <div className="grill-tube-glow" />

            {/* Logo at top of grill */}
            <div
              className="grill-logo"
              onClick={(e) => {
                // Only trigger gold sparks when clicking the logo text itself
                const target = e.target as HTMLElement;
                const isLogoText = target.classList.contains('logo-text-container') ||
                                   target.classList.contains('logo-text-base') ||
                                   target.classList.contains('logo-text-highlight') ||
                                   target.closest('.logo-text-container');
                if (isLogoText) {
                  e.stopPropagation();
                  const rect = e.currentTarget.getBoundingClientRect();
                  goldSparks.triggerSparks(e.clientX - rect.left, e.clientY - rect.top);
                }
              }}
            >
              {goldSparks.SparkElements}
              <LogoText text="Becca Tone Amp" disabled={isDragging} />
            </div>

            {/* Controls mounted on the grill - tubes attached on left */}
            <div className="grill-controls">
              {/* Tube Bay - Attached to left side, sticking out top */}
              <div
                className="tube-bay"
                onClick={(e) => {
                  e.stopPropagation();
                  const rect = e.currentTarget.getBoundingClientRect();
                  orangeSparks.triggerSparks(e.clientX - rect.left, e.clientY - rect.top);
                }}
              >
                {orangeSparks.SparkElements}
                <TubeGlow level={vizData.inputLevel ?? 0} label="IN" />
                <TubeGlow level={vizData.outputLevel ?? 0} label="OUT" />
              </div>

              {/* Knob Row - All knobs in one centered row */}
              <div className="amp-knobs">
                <Knob value={gain.value} onChange={gain.setValue} {...createDragCallbacks(gain.onDragStart, gain.onDragEnd)} label="Gain" defaultValue={DEFAULT_VALUES.gain} color="#8a7a6a" tooltipTitle={CONTROL_TOOLTIPS.gain.title} tooltipDescription={CONTROL_TOOLTIPS.gain.description} />
                <Knob value={bass.value} onChange={bass.setValue} {...createDragCallbacks(bass.onDragStart, bass.onDragEnd)} label="Bass" defaultValue={DEFAULT_VALUES.bass} color="#8a7a6a" tooltipTitle={CONTROL_TOOLTIPS.bass.title} tooltipDescription={CONTROL_TOOLTIPS.bass.description} />
                <Knob value={mid.value} onChange={mid.setValue} {...createDragCallbacks(mid.onDragStart, mid.onDragEnd)} label="Mid" defaultValue={DEFAULT_VALUES.mid} color="#8a7a6a" tooltipTitle={CONTROL_TOOLTIPS.mid.title} tooltipDescription={CONTROL_TOOLTIPS.mid.description} />
                <Knob value={treble.value} onChange={treble.setValue} {...createDragCallbacks(treble.onDragStart, treble.onDragEnd)} label="Treble" defaultValue={DEFAULT_VALUES.treble} color="#8a7a6a" tooltipTitle={CONTROL_TOOLTIPS.treble.title} tooltipDescription={CONTROL_TOOLTIPS.treble.description} />
                <Knob value={presence.value} onChange={presence.setValue} {...createDragCallbacks(presence.onDragStart, presence.onDragEnd)} label="Presence" defaultValue={DEFAULT_VALUES.presence} color="#8a7a6a" tooltipTitle={CONTROL_TOOLTIPS.presence.title} tooltipDescription={CONTROL_TOOLTIPS.presence.description} />
                <Knob value={compression.value} onChange={compression.setValue} {...createDragCallbacks(compression.onDragStart, compression.onDragEnd)} label="Comp" defaultValue={DEFAULT_VALUES.compression} color="#8a7a6a" tooltipTitle={CONTROL_TOOLTIPS.compression.title} tooltipDescription={CONTROL_TOOLTIPS.compression.description} />
                <Knob value={level.value} onChange={level.setValue} {...createDragCallbacks(level.onDragStart, level.onDragEnd)} label="Level" defaultValue={DEFAULT_VALUES.level} size="large" color="#8a7a6a" tooltipTitle={CONTROL_TOOLTIPS.level.title} tooltipDescription={CONTROL_TOOLTIPS.level.description} />
                <Knob value={volume.value} onChange={volume.setValue} {...createDragCallbacks(volume.onDragStart, volume.onDragEnd)} label="Volume" defaultValue={DEFAULT_VALUES.volume} size="large" color="#8a7a6a" tooltipTitle={CONTROL_TOOLTIPS.volume.title} tooltipDescription={CONTROL_TOOLTIPS.volume.description} />
              </div>

              {/* Power Switch */}
              <div className="power-section">
                <div className={`power-switch ${isOn ? 'on' : ''}`} onClick={bypass.toggle}>
                  <div className="power-jewel-container">
                    <div className={`power-jewel ${isOn ? 'on' : ''}`} />
                  </div>
                  <div className="power-switch-plate">
                    <span className="power-label-top">I</span>
                    <div className="power-switch-rocker">
                      <div className="power-switch-toggle" />
                    </div>
                    <span className="power-label-bottom">O</span>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </section>

        {/* Pedals / Visualizer Section */}
        <section className="pedals-section">
          {/* Back arrow (left side, only when visualizer shown) */}
          {showVisualizer && (
            <button
              className="view-toggle view-toggle-left"
              onClick={() => setShowVisualizer(false)}
              title="Show Pedals"
            >
              <span className="view-toggle-icon">◀</span>
            </button>
          )}

          <AnimatePresence mode="wait">
            {!showVisualizer ? (
              <motion.div
                key="pedals"
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                exit={{ opacity: 0 }}
                transition={{ duration: 0.2 }}
                style={{ display: 'flex', justifyContent: 'center' }}
              >
                <Reorder.Group
                  axis="x"
                  values={pedalOrder}
                  onReorder={handlePedalReorder}
                  className="pedals-row"
                  layoutScroll
                >
                  {pedalOrder.map((pedal) => {
                    const props = getPedalProps(pedal.id);
                    return (
                      <PedalItem
                        key={pedal.id}
                        pedal={pedal}
                        enabled={props.enabled}
                        onToggle={props.onToggle}
                        knobs={props.knobs}
                        tooltipTitle={props.tooltipTitle}
                        tooltipDescription={props.tooltipDescription}
                      />
                    );
                  })}
                </Reorder.Group>
              </motion.div>
            ) : (
              <motion.div
                key="visualizer"
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                exit={{ opacity: 0 }}
                transition={{ duration: 0.2 }}
                className="visualizer-wrapper"
              >
                <Visualizer
                  bass={bass.value}
                  mid={mid.value}
                  treble={treble.value}
                  presence={presence.value}
                  gain={gain.value}
                  inputLevel={vizData.inputLevel}
                  gainReduction={vizData.gainReduction}
                  spectrum={vizData.spectrum}
                />
              </motion.div>
            )}
          </AnimatePresence>

          {/* Forward arrow (right side, only when pedals shown) */}
          {!showVisualizer && (
            <button
              className="view-toggle view-toggle-right"
              onClick={() => setShowVisualizer(true)}
              title="Show Visualizer"
            >
              <span className="view-toggle-icon">▶</span>
            </button>
          )}
        </section>
      </main>
    </div>
  );
}

function App() {
  return (
    <DragProvider>
      <AppContent />
    </DragProvider>
  );
}

export default App;
