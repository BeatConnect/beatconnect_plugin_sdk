/**
 * Level Meter Component - VU-style meter
 */

interface LevelMeterProps {
  level: number;
  label: string;
}

export function LevelMeter({ level, label }: LevelMeterProps) {
  // Convert level to dB display (-60 to 0)
  const dbLevel = level > 0 ? Math.max(-60, 20 * Math.log10(level)) : -60;
  const percentage = ((dbLevel + 60) / 60) * 100;

  // Color based on level
  const getColor = () => {
    if (percentage > 90) return '#ff4444';
    if (percentage > 75) return '#ffaa44';
    return '#44aa77';
  };

  return (
    <div className="level-meter">
      <div className="meter-label">{label}</div>
      <div className="meter-track">
        <div
          className="meter-fill"
          style={{
            height: `${percentage}%`,
            backgroundColor: getColor(),
          }}
        />
      </div>
    </div>
  );
}
