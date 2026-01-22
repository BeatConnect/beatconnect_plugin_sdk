/**
 * TubeGlow Component - Realistic vacuum tube with audio-reactive glow
 * Larger, more detailed design inspired by real 12AX7/EL34 tubes
 */

import { useEffect, useState } from 'react';

interface TubeGlowProps {
  level: number;
  label?: string;
}

export function TubeGlow({ level, label }: TubeGlowProps) {
  const [smoothedLevel, setSmoothedLevel] = useState(0);

  useEffect(() => {
    const targetLevel = Math.max(0, Math.min(1, level));

    setSmoothedLevel(prev => {
      const diff = targetLevel - prev;
      const speed = diff > 0 ? 0.35 : 0.05;
      return prev + diff * speed;
    });
  }, [level]);

  // Base glow even when idle (tubes are always warm)
  const baseGlow = 0.25;
  const glowIntensity = baseGlow + smoothedLevel * 0.75;

  return (
    <div className="tube-container">
      <div
        className="tube"
        style={{
          '--glow-intensity': glowIntensity,
          '--glow-opacity': 0.2 + smoothedLevel * 0.6,
        } as React.CSSProperties}
      >
        {/* Outer glass envelope */}
        <div className="tube-envelope">
          {/* Glass reflection highlights */}
          <div className="tube-reflection" />
          <div className="tube-reflection-2" />

          {/* Internal getter (silver flash at top) */}
          <div className="tube-getter" />

          {/* Plate structure */}
          <div className="tube-plate-assembly">
            <div className="tube-plate tube-plate-left" />
            <div className="tube-plate tube-plate-right" />

            {/* Grid wires */}
            <div className="tube-grid">
              {[...Array(5)].map((_, i) => (
                <div key={i} className="tube-grid-wire" />
              ))}
            </div>

            {/* Cathode/heater - the glowing center */}
            <div className="tube-cathode">
              <div className="tube-heater" />
              <div className="tube-heater-glow" />
            </div>
          </div>

          {/* Mica spacers */}
          <div className="tube-mica tube-mica-top" />
          <div className="tube-mica tube-mica-bottom" />

          {/* Internal glow */}
          <div className="tube-internal-glow" />
        </div>

        {/* Base */}
        <div className="tube-base">
          <div className="tube-base-ring" />
          <div className="tube-pins">
            {[...Array(8)].map((_, i) => (
              <div
                key={i}
                className="tube-pin"
                style={{
                  transform: `rotate(${i * 45}deg) translateY(-12px)`,
                }}
              />
            ))}
          </div>
          <div className="tube-base-center" />
        </div>

        {/* Ambient glow around tube */}
        <div className="tube-ambient-glow" />
      </div>

      {label && <span className="tube-label">{label}</span>}
    </div>
  );
}
