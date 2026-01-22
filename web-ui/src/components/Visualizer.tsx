/**
 * Visualizer Component - EQ curve and Compressor displays
 * Uses REAL audio data from C++ FFT analysis and compressor
 */

import { useEffect, useRef, useCallback } from 'react';

interface VisualizerProps {
  // EQ settings from amp
  bass: number;
  mid: number;
  treble: number;
  presence: number;
  gain: number;
  // Real data from C++
  inputLevel?: number;
  gainReduction?: number;  // Real gain reduction from compressor
  spectrum?: number[];     // Real FFT spectrum data
}

export function Visualizer({
  bass,
  mid,
  treble,
  presence,
  gainReduction = 0,
  spectrum
}: VisualizerProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const animationRef = useRef<number>(0);
  const spectrumRef = useRef<number[]>([]);

  // Update spectrum ref when new data arrives
  useEffect(() => {
    if (spectrum && spectrum.length > 0) {
      spectrumRef.current = spectrum;
    }
  }, [spectrum]);

  // Draw EQ curve with real spectrum background
  const draw = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const width = canvas.width;
    const height = canvas.height;
    const padding = { left: 30, right: 10, top: 10, bottom: 20 };
    const graphWidth = width - padding.left - padding.right;
    const graphHeight = height - padding.top - padding.bottom;

    // Clear
    ctx.clearRect(0, 0, width, height);

    // Draw REAL spectrum from FFT data
    const spectrumData = spectrumRef.current;
    if (spectrumData && spectrumData.length > 0) {
      // Draw spectrum as filled area
      ctx.beginPath();
      ctx.moveTo(padding.left, height - padding.bottom);

      const numBins = spectrumData.length;
      for (let i = 0; i < numBins; i++) {
        // Map bin index to x position (logarithmic frequency scale)
        const freq = (i / numBins) * 20000;
        const t = Math.log10(Math.max(20, freq) / 20) / Math.log10(20000 / 20);
        const x = padding.left + t * graphWidth;

        // Map magnitude to y position
        const magnitude = spectrumData[i] || 0;
        const y = height - padding.bottom - (magnitude * graphHeight * 0.8);

        if (i === 0) {
          ctx.moveTo(x, y);
        } else {
          ctx.lineTo(x, y);
        }
      }

      ctx.lineTo(width - padding.right, height - padding.bottom);
      ctx.closePath();

      // Gradient fill for spectrum
      const specGradient = ctx.createLinearGradient(0, padding.top, 0, height - padding.bottom);
      specGradient.addColorStop(0, 'rgba(255, 200, 100, 0.3)');
      specGradient.addColorStop(1, 'rgba(255, 200, 100, 0.05)');
      ctx.fillStyle = specGradient;
      ctx.fill();

      // Draw spectrum line
      ctx.beginPath();
      for (let i = 0; i < numBins; i++) {
        const freq = (i / numBins) * 20000;
        const t = Math.log10(Math.max(20, freq) / 20) / Math.log10(20000 / 20);
        const x = padding.left + t * graphWidth;
        const magnitude = spectrumData[i] || 0;
        const y = height - padding.bottom - (magnitude * graphHeight * 0.8);

        if (i === 0) {
          ctx.moveTo(x, y);
        } else {
          ctx.lineTo(x, y);
        }
      }
      ctx.strokeStyle = 'rgba(255, 200, 100, 0.4)';
      ctx.lineWidth = 1.5;
      ctx.stroke();
    } else {
      // No spectrum data yet - show subtle placeholder
      ctx.fillStyle = 'rgba(255, 200, 100, 0.05)';
      ctx.font = '10px DM Sans';
      ctx.textAlign = 'center';
      ctx.fillText('Waiting for audio...', width / 2, height / 2);
    }

    // Background grid
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
    ctx.lineWidth = 1;

    // Vertical lines (frequency markers)
    const freqs = [100, 500, 1000, 5000, 10000];
    freqs.forEach((freq) => {
      const x = padding.left + (Math.log10(freq / 20) / Math.log10(20000 / 20)) * graphWidth;
      ctx.beginPath();
      ctx.moveTo(x, padding.top);
      ctx.lineTo(x, height - padding.bottom);
      ctx.stroke();
    });

    // Zero line
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.12)';
    const zeroY = padding.top + graphHeight / 2;
    ctx.beginPath();
    ctx.moveTo(padding.left, zeroY);
    ctx.lineTo(width - padding.right, zeroY);
    ctx.stroke();

    // Calculate EQ curve based on knob values
    const bassDb = (bass - 0.5) * 24;
    const midDb = (mid - 0.5) * 24;
    const trebleDb = (treble - 0.5) * 24;
    const presenceDb = (presence - 0.5) * 24;

    // Generate smooth curve
    const points: { x: number; y: number }[] = [];
    const numPoints = 200;

    for (let i = 0; i <= numPoints; i++) {
      const t = i / numPoints;
      const freq = 20 * Math.pow(20000 / 20, t);

      let dbGain = 0;

      // Bass (centered at 100Hz)
      dbGain += bassDb * Math.exp(-Math.pow(Math.log10(freq / 100), 2) / (2 * 0.7 * 0.7));
      // Mid (centered at 800Hz)
      dbGain += midDb * Math.exp(-Math.pow(Math.log10(freq / 800), 2) / (2 * 0.8 * 0.8));
      // Treble (centered at 3kHz)
      dbGain += trebleDb * Math.exp(-Math.pow(Math.log10(freq / 3000), 2) / (2 * 0.9 * 0.9));
      // Presence (centered at 5kHz)
      dbGain += presenceDb * Math.exp(-Math.pow(Math.log10(freq / 5000), 2) / (2 * 1.2 * 1.2));

      dbGain = Math.max(-12, Math.min(12, dbGain));

      const x = padding.left + t * graphWidth;
      const y = padding.top + (1 - (dbGain + 12) / 24) * graphHeight;
      points.push({ x, y });
    }

    // Draw curve fill
    ctx.beginPath();
    ctx.moveTo(padding.left, zeroY);
    points.forEach((p) => ctx.lineTo(p.x, p.y));
    ctx.lineTo(width - padding.right, zeroY);
    ctx.closePath();

    const gradient = ctx.createLinearGradient(0, padding.top, 0, height - padding.bottom);
    gradient.addColorStop(0, 'rgba(255, 200, 100, 0.25)');
    gradient.addColorStop(0.5, 'rgba(255, 200, 100, 0.05)');
    gradient.addColorStop(1, 'rgba(255, 200, 100, 0.25)');
    ctx.fillStyle = gradient;
    ctx.fill();

    // Draw curve line with glow
    ctx.shadowColor = 'rgba(255, 200, 100, 0.6)';
    ctx.shadowBlur = 8;
    ctx.beginPath();
    points.forEach((p, i) => {
      if (i === 0) ctx.moveTo(p.x, p.y);
      else ctx.lineTo(p.x, p.y);
    });
    ctx.strokeStyle = '#ffd080';
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.shadowBlur = 0;

    // Draw frequency labels
    ctx.fillStyle = 'rgba(255, 255, 255, 0.35)';
    ctx.font = '9px DM Sans';
    ctx.textAlign = 'center';
    [100, 1000, 10000].forEach((freq) => {
      const x = padding.left + (Math.log10(freq / 20) / Math.log10(20000 / 20)) * graphWidth;
      const label = freq >= 1000 ? `${freq / 1000}k` : `${freq}`;
      ctx.fillText(label, x, height - 4);
    });

    // Draw dB labels
    ctx.textAlign = 'right';
    [-12, 0, 12].forEach((db) => {
      const y = padding.top + (1 - (db + 12) / 24) * graphHeight;
      ctx.fillText(`${db > 0 ? '+' : ''}${db}`, padding.left - 4, y + 3);
    });

  }, [bass, mid, treble, presence]);

  // Animation loop for continuous spectrum updates
  useEffect(() => {
    const animate = () => {
      draw();
      animationRef.current = requestAnimationFrame(animate);
    };

    animationRef.current = requestAnimationFrame(animate);

    return () => {
      cancelAnimationFrame(animationRef.current);
    };
  }, [draw]);

  // Use REAL gain reduction from C++ compressor (clamped to display range)
  const displayGainReduction = Math.min(Math.max(gainReduction, 0), 12);

  return (
    <div className="visualizer">
      {/* EQ Panel */}
      <div className="viz-panel viz-panel-eq">
        <div className="viz-panel-label">EQ Response</div>
        <div className="viz-panel-content">
          <canvas
            ref={canvasRef}
            width={400}
            height={130}
            className="eq-canvas"
          />
        </div>
      </div>

      {/* Compressor Panel */}
      <div className="viz-panel viz-panel-comp">
        <div className="viz-panel-label">Compressor</div>
        <div className="viz-panel-content">
          <div className="comp-display">
            <div className="comp-meter">
              <div className="comp-meter-track">
                <div
                  className="comp-meter-fill"
                  style={{ height: `${(displayGainReduction / 12) * 100}%` }}
                />
              </div>
              <div className="comp-meter-labels">
                <span>0</span>
                <span>-6</span>
                <span>-12</span>
              </div>
            </div>
            <div className="comp-info">
              <div className="comp-gr-value">-{displayGainReduction.toFixed(1)}</div>
              <div className="comp-gr-label">GR dB</div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
