/**
 * PresetSelector - Animated preset menu with descriptions
 */

import { useState, useRef, useEffect } from 'react';
import { motion, AnimatePresence } from 'framer-motion';

interface PresetInfo {
  name: string;
  description: string;
  tags: string[];
}

const PRESET_DATA: PresetInfo[] = [
  {
    name: 'Becca Signature Tone',
    description: 'The quintessential Becca sound. Warm, buttery clean with subtle compression and a touch of reverb.',
    tags: ['Soul', 'Jazz', 'R&B'],
  },
  {
    name: 'Pristine Clean',
    description: 'Crystal clear, studio-quality clean tone. No coloration, just pure guitar signal.',
    tags: ['Studio', 'Clean', 'Hi-Fi'],
  },
  {
    name: 'Super Clean',
    description: 'Sparkly clean with enhanced presence and subtle compression. Great for chord work.',
    tags: ['Funk', 'Pop', 'Clean'],
  },
  {
    name: 'Overdriven / Fuzzy',
    description: 'Warm tube overdrive meets vintage fuzz. Fat, saturated tones with tons of sustain.',
    tags: ['Blues', 'Rock', 'Vintage'],
  },
  {
    name: '80s Dream',
    description: 'Lush chorus and shimmering reverb create a dreamy, atmospheric soundscape.',
    tags: ['Retro', 'Atmospheric', 'Dreamy'],
  },
  {
    name: 'Funk Groove',
    description: 'Tight, punchy clean with fast compression. Designed for percussive strumming.',
    tags: ['Funk', 'Rhythm', 'Tight'],
  },
  {
    name: 'Indie Grit',
    description: 'Lo-fi warmth with subtle grit and tremolo. Perfect for indie rock and shoegaze.',
    tags: ['Indie', 'Lo-Fi', 'Experimental'],
  },
];

interface PresetSelectorProps {
  currentIndex: number;
  onSelect: (index: number) => void;
  onInitialize?: () => void;
  choices?: string[];
}

export function PresetSelector({ currentIndex, onSelect, onInitialize, choices }: PresetSelectorProps) {
  const [isOpen, setIsOpen] = useState(false);
  const containerRef = useRef<HTMLDivElement>(null);

  // Use JUCE choices if available, fallback to our preset data
  const presetNames = choices && choices.length > 0 ? choices : PRESET_DATA.map(p => p.name);

  // Close menu when clicking outside
  useEffect(() => {
    const handleClickOutside = (e: MouseEvent) => {
      if (containerRef.current && !containerRef.current.contains(e.target as Node)) {
        setIsOpen(false);
      }
    };

    if (isOpen) {
      document.addEventListener('mousedown', handleClickOutside);
      return () => document.removeEventListener('mousedown', handleClickOutside);
    }
  }, [isOpen]);

  const handleSelect = (index: number) => {
    onSelect(index);
    setIsOpen(false);
  };

  const handleInitialize = () => {
    onInitialize?.();
    setIsOpen(false);
  };

  const currentPreset = PRESET_DATA[currentIndex] || {
    name: presetNames[currentIndex] || 'Unknown',
    description: '',
    tags: [],
  };

  return (
    <div className="preset-selector" ref={containerRef}>
      {/* Current Preset Button */}
      <motion.button
        className="preset-button"
        onClick={() => setIsOpen(!isOpen)}
        whileHover={{ scale: 1.01 }}
        whileTap={{ scale: 0.99 }}
      >
        <span className="preset-button-name">{currentPreset.name}</span>
        <motion.span
          className="preset-button-arrow"
          animate={{ rotate: isOpen ? 180 : 0 }}
          transition={{ duration: 0.2 }}
        >
          ▼
        </motion.span>
      </motion.button>

      {/* Dropdown Menu */}
      <AnimatePresence>
        {isOpen && (
          <motion.div
            className="preset-menu"
            initial={{ opacity: 0, y: -10, scale: 0.95 }}
            animate={{ opacity: 1, y: 0, scale: 1 }}
            exit={{ opacity: 0, y: -10, scale: 0.95 }}
            transition={{
              type: 'spring',
              stiffness: 400,
              damping: 25,
              mass: 0.8
            }}
          >
            <div className="preset-menu-header">
              <span className="preset-menu-title">Select Preset</span>
              {onInitialize && (
                <button className="preset-initialize-btn" onClick={handleInitialize}>
                  Reset
                </button>
              )}
            </div>

            <div className="preset-menu-list">
              {PRESET_DATA.map((preset, index) => (
                <button
                  key={index}
                  className={`preset-item ${index === currentIndex ? 'active' : ''}`}
                  onClick={() => handleSelect(index)}
                >
                  <div className="preset-item-content">
                    <div className="preset-item-header">
                      <span className="preset-item-name">{preset.name}</span>
                      {index === currentIndex && <span className="preset-item-check">Active</span>}
                    </div>
                    <div className="preset-item-description">{preset.description}</div>
                    <div className="preset-item-tags">
                      {preset.tags.map((tag, i) => (
                        <span key={i} className="preset-tag">{tag}</span>
                      ))}
                    </div>
                  </div>
                </button>
              ))}
            </div>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
}
