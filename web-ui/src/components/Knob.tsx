/**
 * Knob Component - Vintage-style rotary control with tooltip
 * Shows value while dragging, double-click to reset to default
 */

import React, { useRef, useCallback, useState } from 'react';
import { Tooltip } from './Tooltip';

interface KnobProps {
  value: number;
  onChange: (value: number) => void;
  onDragStart?: () => void;
  onDragEnd?: () => void;
  label: string;
  min?: number;
  max?: number;
  defaultValue?: number;
  size?: 'tiny' | 'small' | 'medium' | 'large';
  color?: string;
  tooltipTitle?: string;
  tooltipDescription?: string;
}

export function Knob({
  value,
  onChange,
  onDragStart,
  onDragEnd,
  label,
  min = 0,
  max = 1,
  defaultValue,
  size = 'medium',
  color = '#d4a574',
  tooltipTitle,
  tooltipDescription,
}: KnobProps) {
  const [showValue, setShowValue] = useState(false);
  const isDragging = useRef(false);
  const startY = useRef(0);
  const startValue = useRef(0);

  const sizeMap = {
    tiny: 28,
    small: 44,
    medium: 56,
    large: 68,
  };
  const knobSize = sizeMap[size];

  const normalizedValue = (value - min) / (max - min);
  const rotation = -135 + normalizedValue * 270;

  // Format value for display (0-100 percentage or appropriate format)
  const formatValue = (val: number) => {
    const percentage = Math.round(((val - min) / (max - min)) * 100);
    return `${percentage}`;
  };

  // Handle pointer down - prevents framer-motion Reorder from capturing the drag
  const handlePointerDown = useCallback(
    (e: React.PointerEvent) => {
      // CRITICAL: Stop propagation for both pointer and mouse events
      // to prevent framer-motion Reorder from dragging the pedal
      e.stopPropagation();
      e.preventDefault();

      isDragging.current = true;
      startY.current = e.clientY;
      startValue.current = value;
      setShowValue(true);
      onDragStart?.();

      const handlePointerMove = (e: PointerEvent) => {
        if (!isDragging.current) return;
        const deltaY = startY.current - e.clientY;
        const sensitivity = 0.005;
        const newValue = Math.max(min, Math.min(max, startValue.current + deltaY * sensitivity * (max - min)));
        onChange(newValue);
      };

      const handlePointerUp = () => {
        isDragging.current = false;
        setShowValue(false);
        onDragEnd?.();
        document.removeEventListener('pointermove', handlePointerMove);
        document.removeEventListener('pointerup', handlePointerUp);
      };

      document.addEventListener('pointermove', handlePointerMove);
      document.addEventListener('pointerup', handlePointerUp);
    },
    [value, min, max, onChange, onDragStart, onDragEnd]
  );

  // Double-click to reset to default
  const handleDoubleClick = useCallback(
    (e: React.MouseEvent) => {
      e.preventDefault();
      if (defaultValue !== undefined) {
        onChange(defaultValue);
      }
    },
    [defaultValue, onChange]
  );

  const handlePointerLeave = useCallback(() => {
    if (!isDragging.current) {
      setShowValue(false);
    }
  }, []);

  const displayText = showValue ? formatValue(value) : label;

  const knobContent = (
    <div
      className={`knob-container knob-${size}`}
      style={{ width: knobSize + 16 }}
      onPointerLeave={handlePointerLeave}
    >
      <div
        className="knob"
        style={{
          width: knobSize,
          height: knobSize,
          transform: `rotate(${rotation}deg)`,
          '--knob-color': color,
        } as React.CSSProperties}
        onPointerDownCapture={handlePointerDown}
        onPointerDown={handlePointerDown}
        onDoubleClick={handleDoubleClick}
      >
        <div className="knob-indicator" />
      </div>
      <div className={`knob-label ${showValue ? 'knob-label-value' : ''}`}>{displayText}</div>
    </div>
  );

  // Wrap in tooltip if tooltip info provided
  if (tooltipTitle && tooltipDescription) {
    return (
      <Tooltip title={tooltipTitle} description={tooltipDescription}>
        {knobContent}
      </Tooltip>
    );
  }

  return knobContent;
}
