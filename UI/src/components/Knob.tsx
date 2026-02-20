import React, { useRef, useCallback, useId } from 'react';

interface KnobProps {
  label: string;
  value: number;          // raw value in user-space
  min: number;
  max: number;
  default?: number;
  step?: number;
  unit?: string;
  decimals?: number;
  size?: number;
  color?: string;
  /** Provide for log-scale knobs */
  normalize?: (v: number) => number;
  denormalize?: (n: number) => number;
  onChange: (rawValue: number) => void;
  displayValue?: (v: number) => string;
  disabled?: boolean;
}

const MIN_ANGLE = -145;
const MAX_ANGLE =  145;

function toAngle(norm: number): number {
  return MIN_ANGLE + norm * (MAX_ANGLE - MIN_ANGLE);
}

function linNorm(v: number, min: number, max: number): number {
  return Math.max(0, Math.min(1, (v - min) / (max - min)));
}
function linDenorm(n: number, min: number, max: number): number {
  return min + n * (max - min);
}

export const Knob: React.FC<KnobProps> = ({
  label, value, min, max, step, unit, decimals = 2,
  size = 64, color = '#c8a84b', onChange, displayValue, disabled = false,
  normalize, denormalize,
}) => {
  const id = useId();
  const svgRef = useRef<SVGSVGElement>(null);
  const dragRef = useRef<{ startY: number; startNorm: number } | null>(null);

  const norm = normalize ? normalize(value) : linNorm(value, min, max);
  const angle = toAngle(norm);

  const dispStr = displayValue
    ? displayValue(value)
    : `${value.toFixed(decimals)}${unit ?? ''}`;

  // ---- Pointer drag ----
  const handlePointerDown = useCallback((e: React.PointerEvent) => {
    if (disabled) return;
    e.currentTarget.setPointerCapture(e.pointerId);
    dragRef.current = { startY: e.clientY, startNorm: norm };
  }, [norm, disabled]);

  const handlePointerMove = useCallback((e: React.PointerEvent) => {
    if (!dragRef.current) return;
    const sensitivity = e.shiftKey ? 0.002 : 0.01;
    const delta = (dragRef.current.startY - e.clientY) * sensitivity;
    const newNorm = Math.max(0, Math.min(1, dragRef.current.startNorm + delta));
    const raw = denormalize ? denormalize(newNorm) : linDenorm(newNorm, min, max);
    const snapped = step ? Math.round(raw / step) * step : raw;
    onChange(Math.max(min, Math.min(max, snapped)));
  }, [min, max, step, onChange, denormalize]);

  const handlePointerUp = useCallback((e: React.PointerEvent) => {
    e.currentTarget.releasePointerCapture(e.pointerId);
    dragRef.current = null;
  }, []);

  const handleDoubleClick = useCallback(() => {
    // No-op: could reset to default
  }, []);

  const handleWheel = useCallback((e: React.WheelEvent) => {
    if (disabled) return;
    e.preventDefault();
    const delta = -e.deltaY * (e.shiftKey ? 0.0002 : 0.002);
    const newNorm = Math.max(0, Math.min(1, norm + delta));
    const raw = denormalize ? denormalize(newNorm) : linDenorm(newNorm, min, max);
    const snapped = step ? Math.round(raw / step) * step : raw;
    onChange(Math.max(min, Math.min(max, snapped)));
  }, [norm, min, max, step, onChange, denormalize, disabled]);

  // SVG arc path for the value indicator
  const r = size * 0.36;
  const cx = size / 2;
  const cy = size / 2;

  function polarXY(angleDeg: number): [number, number] {
    const rad = (angleDeg - 90) * (Math.PI / 180);
    return [cx + r * Math.cos(rad), cy + r * Math.sin(rad)];
  }

  // Track arc
  const [startX, startY] = polarXY(MIN_ANGLE);
  const [endX, endY]     = polarXY(MAX_ANGLE);
  const trackPath = `M ${startX} ${startY} A ${r} ${r} 0 1 1 ${endX} ${endY}`;

  // Value arc
  const [valX, valY] = polarXY(angle);
  const totalAngle = MAX_ANGLE - MIN_ANGLE;
  const valAngle   = norm * totalAngle;
  const largeArc   = valAngle > 180 ? 1 : 0;
  const valuePath  = `M ${startX} ${startY} A ${r} ${r} 0 ${largeArc} 1 ${valX} ${valY}`;

  // Pointer line
  const innerR  = r * 0.45;
  const [px, py] = polarXY(angle);
  const pInnerX  = cx + innerR * Math.cos((angle - 90) * Math.PI / 180);
  const pInnerY  = cy + innerR * Math.sin((angle - 90) * Math.PI / 180);

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: 4,
        opacity: disabled ? 0.4 : 1,
        userSelect: 'none',
      }}
    >
      <svg
        ref={svgRef}
        width={size}
        height={size}
        viewBox={`0 0 ${size} ${size}`}
        style={{ cursor: disabled ? 'default' : 'ns-resize', touchAction: 'none' }}
        onPointerDown={handlePointerDown}
        onPointerMove={handlePointerMove}
        onPointerUp={handlePointerUp}
        onDoubleClick={handleDoubleClick}
        onWheel={handleWheel}
      >
        {/* Background circle */}
        <circle cx={cx} cy={cy} r={r * 1.22} fill="#1a1a22" stroke="#2a2a35" strokeWidth={1} />

        {/* Track */}
        <path d={trackPath} fill="none" stroke="#2a2a35" strokeWidth={3} strokeLinecap="round" />

        {/* Value arc */}
        <path d={valuePath} fill="none" stroke={color} strokeWidth={3} strokeLinecap="round" />

        {/* Pointer */}
        <line x1={pInnerX} y1={pInnerY} x2={px} y2={py}
              stroke={color} strokeWidth={2} strokeLinecap="round" />

        {/* Center dot */}
        <circle cx={cx} cy={cy} r={3} fill={color} opacity={0.6} />
      </svg>

      {/* Value readout */}
      <span style={{
        fontSize: 10, color: '#c0b090', fontFamily: 'monospace',
        letterSpacing: '0.05em', minWidth: 48, textAlign: 'center',
      }}>
        {dispStr}
      </span>

      {/* Label */}
      <span style={{
        fontSize: 10, color: '#7a7a9a', textTransform: 'uppercase',
        letterSpacing: '0.12em', textAlign: 'center', maxWidth: size + 16,
      }}>
        {label}
      </span>
    </div>
  );
};
