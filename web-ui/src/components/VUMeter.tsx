/**
 * VU Meter Component - Vintage analog-style meter
 */

import { useEffect, useRef, useState } from 'react';

interface VUMeterProps {
  level: number;
  label: string;
}

export function VUMeter({ level, label }: VUMeterProps) {
  const [smoothedLevel, setSmoothedLevel] = useState(0);
  const [peakLevel, setPeakLevel] = useState(0);
  const peakHoldTimer = useRef<number | null>(null);

  // Smooth the level changes for a more analog feel
  useEffect(() => {
    const targetLevel = Math.max(0, Math.min(1, level));

    // Smooth attack/release
    setSmoothedLevel(prev => {
      const diff = targetLevel - prev;
      // Fast attack, slow release (like real VU meters)
      const speed = diff > 0 ? 0.3 : 0.08;
      return prev + diff * speed;
    });

    // Peak hold
    if (targetLevel > peakLevel) {
      setPeakLevel(targetLevel);
      if (peakHoldTimer.current) {
        clearTimeout(peakHoldTimer.current);
      }
      peakHoldTimer.current = window.setTimeout(() => {
        setPeakLevel(0);
      }, 1500);
    }

    return () => {
      if (peakHoldTimer.current) {
        clearTimeout(peakHoldTimer.current);
      }
    };
  }, [level, peakLevel]);

  // Convert to dB for display (-40 to +3)
  const dbLevel = smoothedLevel > 0.001 ? 20 * Math.log10(smoothedLevel) : -60;
  const normalizedDb = Math.max(0, Math.min(100, ((dbLevel + 40) / 43) * 100));

  // Needle rotation (-45 to +45 degrees)
  const needleRotation = -45 + (normalizedDb / 100) * 90;

  // Generate tick marks
  const ticks = [-40, -20, -10, -7, -5, -3, 0, 3];

  return (
    <div className="vu-meter">
      <div className="vu-meter-face">
        {/* Tick marks and labels */}
        <div className="vu-ticks">
          {ticks.map((db) => {
            const normalized = ((db + 40) / 43) * 100;
            const angle = -45 + (normalized / 100) * 90;
            const isRed = db >= 0;
            return (
              <div
                key={db}
                className={`vu-tick ${isRed ? 'red' : ''}`}
                style={{
                  transform: `rotate(${angle}deg)`,
                }}
              >
                <span className="vu-tick-label">{db > 0 ? `+${db}` : db}</span>
              </div>
            );
          })}
        </div>

        {/* Arc scale */}
        <div className="vu-arc">
          <svg viewBox="0 0 120 70" className="vu-arc-svg">
            {/* Background arc */}
            <path
              d="M 10 60 A 50 50 0 0 1 110 60"
              fill="none"
              stroke="rgba(255,255,255,0.1)"
              strokeWidth="2"
            />
            {/* Green zone (-40 to 0) */}
            <path
              d="M 10 60 A 50 50 0 0 1 85 15"
              fill="none"
              stroke="#4a8b4a"
              strokeWidth="3"
              strokeLinecap="round"
            />
            {/* Red zone (0 to +3) */}
            <path
              d="M 85 15 A 50 50 0 0 1 110 60"
              fill="none"
              stroke="#c44"
              strokeWidth="3"
              strokeLinecap="round"
            />
          </svg>
        </div>

        {/* Needle */}
        <div
          className="vu-needle"
          style={{
            transform: `rotate(${needleRotation}deg)`,
          }}
        >
          <div className="vu-needle-arm" />
          <div className="vu-needle-pivot" />
        </div>

        {/* Peak indicator */}
        {peakLevel > 0.7 && (
          <div className="vu-peak-indicator" />
        )}

        {/* VU label */}
        <div className="vu-label">VU</div>
      </div>

      {/* Channel label */}
      <div className="vu-channel-label">{label}</div>
    </div>
  );
}
