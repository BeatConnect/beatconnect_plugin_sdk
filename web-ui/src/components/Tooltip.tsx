/**
 * Tooltip Component - Beautiful branded tooltips with smart positioning
 * Uses Portal to render outside parent DOM hierarchy
 */

import { useState, useRef, useEffect, ReactNode } from 'react';
import { createPortal } from 'react-dom';
import { motion, AnimatePresence } from 'framer-motion';

interface TooltipProps {
  title: string;
  description: string;
  children: ReactNode;
  delay?: number;
}

export function Tooltip({
  title,
  description,
  children,
  delay = 600,
}: TooltipProps) {
  const [isVisible, setIsVisible] = useState(false);
  const [tooltipStyle, setTooltipStyle] = useState<React.CSSProperties>({});
  const timeoutRef = useRef<number | null>(null);
  const wrapperRef = useRef<HTMLDivElement>(null);

  const calculatePosition = () => {
    if (!wrapperRef.current) return;

    const rect = wrapperRef.current.getBoundingClientRect();
    const tooltipWidth = 240;
    const tooltipHeight = 80;
    const padding = 8;

    // Default: position above the element, centered
    let top = rect.top - tooltipHeight - padding;
    let left = rect.left + rect.width / 2 - tooltipWidth / 2;

    // If tooltip would go above viewport, position below
    if (top < padding) {
      top = rect.bottom + padding;
    }

    // If tooltip would go off left edge, align to left
    if (left < padding) {
      left = padding;
    }

    // If tooltip would go off right edge, align to right
    if (left + tooltipWidth > window.innerWidth - padding) {
      left = window.innerWidth - tooltipWidth - padding;
    }

    setTooltipStyle({
      top: `${top}px`,
      left: `${left}px`,
    });
  };

  const handleMouseEnter = () => {
    timeoutRef.current = window.setTimeout(() => {
      calculatePosition();
      setIsVisible(true);
    }, delay);
  };

  const handleMouseLeave = () => {
    if (timeoutRef.current) {
      clearTimeout(timeoutRef.current);
      timeoutRef.current = null;
    }
    setIsVisible(false);
  };

  useEffect(() => {
    return () => {
      if (timeoutRef.current) {
        clearTimeout(timeoutRef.current);
      }
    };
  }, []);

  // Render tooltip via portal to document.body
  const tooltipElement = (
    <AnimatePresence>
      {isVisible && (
        <motion.div
          className="tooltip"
          style={tooltipStyle}
          initial={{ opacity: 0, scale: 0.95 }}
          animate={{ opacity: 1, scale: 1 }}
          exit={{ opacity: 0, scale: 0.95 }}
          transition={{ duration: 0.15 }}
        >
          <span className="tooltip-title">{title}</span>
          <span className="tooltip-description">{description}</span>
        </motion.div>
      )}
    </AnimatePresence>
  );

  return (
    <div
      className="tooltip-wrapper"
      ref={wrapperRef}
      onMouseEnter={handleMouseEnter}
      onMouseLeave={handleMouseLeave}
    >
      {children}
      {createPortal(tooltipElement, document.body)}
    </div>
  );
}

// Tooltip descriptions for all controls
export const CONTROL_TOOLTIPS = {
  // Amp controls
  gain: {
    title: 'Gain',
    description: 'Controls the preamp drive. Lower values give clean tones, higher values add warm tube saturation and harmonic richness.',
  },
  bass: {
    title: 'Bass',
    description: 'Adjusts low frequencies. Boost for thick, warm tones or cut for tighter, more defined sound.',
  },
  mid: {
    title: 'Mid',
    description: 'Controls midrange frequencies. Essential for cutting through a mix and adding body to your tone.',
  },
  treble: {
    title: 'Treble',
    description: 'Shapes high frequencies. Increase for sparkle and clarity, decrease for warmer, darker tones.',
  },
  presence: {
    title: 'Presence',
    description: 'Adds high-mid clarity and bite. Great for articulation without harshness.',
  },
  compression: {
    title: 'Compression',
    description: 'Evens out dynamics and adds sustain. Higher values give more squash and consistency.',
  },
  level: {
    title: 'Level',
    description: 'Pre-volume output level. Adjusts signal strength before the master volume.',
  },
  volume: {
    title: 'Volume',
    description: 'Master output volume. Controls the final output level of the amp.',
  },

  // Distortion/Fuzz pedal
  distortion: {
    title: 'Fuzz',
    description: 'Classic fuzz distortion with warm, fuzzy saturation. Think vintage blues and psychedelic rock.',
  },
  distortionDrive: {
    title: 'Drive',
    description: 'Controls the amount of fuzz saturation. From subtle grit to full-on fuzzy mayhem.',
  },
  distortionTone: {
    title: 'Tone',
    description: 'Shapes the brightness of the fuzz. Roll back for thick, woolly tones or boost for cutting presence.',
  },

  // Chorus pedal
  chorus: {
    title: 'Chorus',
    description: 'Lush modulation effect that adds shimmer and depth. Perfect for clean tones and dreamy textures.',
  },
  chorusRate: {
    title: 'Rate',
    description: 'Speed of the chorus modulation. Slower for subtle movement, faster for more obvious wobble.',
  },
  chorusDepth: {
    title: 'Depth',
    description: 'Intensity of the chorus effect. Higher values create more dramatic pitch variation.',
  },

  // Tremolo pedal
  tremolo: {
    title: 'Tremolo',
    description: 'Classic amplitude modulation. Creates a pulsing, wavering effect from subtle to dramatic.',
  },
  tremoloRate: {
    title: 'Rate',
    description: 'Speed of the tremolo pulse. From slow, hypnotic waves to rapid stuttering.',
  },
  tremoloDepth: {
    title: 'Depth',
    description: 'Intensity of the volume modulation. Subtle shimmer to dramatic choppy effects.',
  },

  // Delay pedal
  delay: {
    title: 'Delay',
    description: 'Echo effect that repeats your signal. Great for adding space and rhythmic interest.',
  },
  delayTime: {
    title: 'Time',
    description: 'Delay time between repeats. Shorter for slapback, longer for atmospheric echoes.',
  },
  delayMix: {
    title: 'Mix',
    description: 'Blend between dry signal and delayed signal. Higher values make echoes more prominent.',
  },

  // Reverb pedal
  reverb: {
    title: 'Reverb',
    description: 'Adds space and ambience. From intimate room sounds to vast, ethereal halls.',
  },
  reverbSize: {
    title: 'Size',
    description: 'Size of the virtual room. Small for tight spaces, large for cavernous halls.',
  },
  reverbMix: {
    title: 'Mix',
    description: 'Amount of reverb in the signal. Subtle for natural space, heavy for ambient textures.',
  },
} as const;
