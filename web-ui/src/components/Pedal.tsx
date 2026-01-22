/**
 * Pedal Component - Realistic guitar effects stompbox
 * Each pedal has its own distinct embossed font style
 */

import { Knob } from './Knob';
import { Tooltip } from './Tooltip';

interface PedalKnob {
  id: string;
  label: string;
  value: number;
  onChange: (value: number) => void;
  onDragStart?: () => void;
  onDragEnd?: () => void;
  min?: number;
  max?: number;
  defaultValue?: number;
  tooltipTitle?: string;
  tooltipDescription?: string;
}

interface PedalProps {
  name: string;
  color: string;
  enabled: boolean;
  onToggle: () => void;
  knobs: PedalKnob[];
  fontClass?: string;
  tooltipTitle?: string;
  tooltipDescription?: string;
}

export function Pedal({ name, color, enabled, onToggle, knobs, fontClass = '', tooltipTitle, tooltipDescription }: PedalProps) {
  const nameplateContent = (
    <div className={`pedal-nameplate ${fontClass}`}>
      <span className="pedal-name">{name}</span>
    </div>
  );

  return (
    <div
      className={`pedal ${enabled ? 'enabled' : ''}`}
      style={{ '--pedal-color': color } as React.CSSProperties}
    >
      {/* Top screws */}
      <div className="pedal-screw pedal-screw-tl" />
      <div className="pedal-screw pedal-screw-tr" />

      {/* LED indicator at top */}
      <div className={`pedal-led ${enabled ? 'on' : ''}`} />

      {/* Knobs - 2 small knobs */}
      <div className="pedal-knobs-container">
        {knobs.slice(0, 2).map((knob) => (
          <div key={knob.id} className="pedal-knob-wrapper">
            <Knob
              value={knob.value}
              onChange={knob.onChange}
              onDragStart={knob.onDragStart}
              onDragEnd={knob.onDragEnd}
              label={knob.label}
              min={knob.min}
              max={knob.max}
              defaultValue={knob.defaultValue}
              size="tiny"
              color="#1a1a1a"
              tooltipTitle={knob.tooltipTitle}
              tooltipDescription={knob.tooltipDescription}
            />
          </div>
        ))}
      </div>

      {/* Pedal name - larger, below knobs, with tooltip */}
      {tooltipTitle && tooltipDescription ? (
        <Tooltip title={tooltipTitle} description={tooltipDescription}>
          {nameplateContent}
        </Tooltip>
      ) : (
        nameplateContent
      )}

      {/* Foot pad / stomp switch - realistic with ridges */}
      <button className="pedal-footpad" onClick={onToggle}>
        <div className="footpad-surface">
          <div className="footpad-ridges" />
        </div>
      </button>

      {/* Bottom screws */}
      <div className="pedal-screw pedal-screw-bl" />
      <div className="pedal-screw pedal-screw-br" />
    </div>
  );
}
