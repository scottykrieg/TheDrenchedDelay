import React from 'react';

// ============================================================
// LED Toggle — on/off switch with illuminated LED dot
// ============================================================
interface ToggleProps {
  label: string;
  value: boolean;
  onChange: (v: boolean) => void;
  color?: string;
  size?: 'sm' | 'md';
}

export const Toggle: React.FC<ToggleProps> = ({
  label, value, onChange, color = '#c8a84b', size = 'md',
}) => {
  const h = size === 'sm' ? 22 : 28;
  const fontSize = size === 'sm' ? 9 : 10;

  return (
    <button
      onClick={() => onChange(!value)}
      style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: 4,
        background: 'none',
        border: 'none',
        cursor: 'pointer',
        padding: 0,
        userSelect: 'none',
      }}
    >
      <div style={{
        width: h * 1.8, height: h,
        background: value ? color + '33' : '#1a1a22',
        border: `1.5px solid ${value ? color : '#3a3a4a'}`,
        borderRadius: h / 2,
        display: 'flex',
        alignItems: 'center',
        padding: 3,
        transition: 'all 0.15s',
        justifyContent: value ? 'flex-end' : 'flex-start',
      }}>
        <div style={{
          width: h - 8, height: h - 8,
          borderRadius: '50%',
          background: value ? color : '#3a3a4a',
          boxShadow: value ? `0 0 6px ${color}88` : 'none',
          transition: 'all 0.15s',
        }} />
      </div>
      <span style={{
        fontSize,
        color: value ? '#c0b090' : '#5a5a7a',
        textTransform: 'uppercase',
        letterSpacing: '0.1em',
        transition: 'color 0.15s',
      }}>
        {label}
      </span>
    </button>
  );
};

// ============================================================
// LED Button — momentary press button (e.g. Tap Tempo)
// ============================================================
interface ButtonProps {
  label: string;
  onPress: () => void;
  color?: string;
  wide?: boolean;
  subtitle?: string;
}

export const LedButton: React.FC<ButtonProps> = ({
  label, onPress, color = '#c8a84b', wide = false, subtitle,
}) => {
  const [pressed, setPressed] = React.useState(false);

  const handleDown = () => {
    setPressed(true);
    onPress();
  };
  const handleUp = () => setPressed(false);

  return (
    <button
      onPointerDown={handleDown}
      onPointerUp={handleUp}
      onPointerLeave={handleUp}
      style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: 4,
        background: pressed ? color + '22' : '#1a1a22',
        border: `1.5px solid ${pressed ? color : '#3a3a4a'}`,
        borderRadius: 8,
        padding: '8px 16px',
        cursor: 'pointer',
        userSelect: 'none',
        minWidth: wide ? 90 : 60,
        transition: 'all 0.08s',
        boxShadow: pressed ? `0 0 12px ${color}66` : 'none',
      }}
    >
      <span style={{
        fontSize: 12,
        fontWeight: 700,
        color: pressed ? color : '#c0b090',
        textTransform: 'uppercase',
        letterSpacing: '0.15em',
        transition: 'color 0.08s',
      }}>
        {label}
      </span>
      {subtitle && (
        <span style={{ fontSize: 9, color: '#6a6a8a', letterSpacing: '0.1em' }}>
          {subtitle}
        </span>
      )}
    </button>
  );
};

// ============================================================
// Selector — stepped options (note divisions, filter type, etc.)
// ============================================================
interface SelectorProps {
  label: string;
  options: string[];
  value: number;  // index
  onChange: (index: number) => void;
  color?: string;
}

export const Selector: React.FC<SelectorProps> = ({
  label, options, value, onChange, color = '#c8a84b',
}) => {
  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      alignItems: 'center',
      gap: 6,
    }}>
      <div style={{ display: 'flex', gap: 2, flexWrap: 'wrap', justifyContent: 'center' }}>
        {options.map((opt, i) => (
          <button
            key={i}
            onClick={() => onChange(i)}
            style={{
              background: value === i ? color + '22' : '#12121a',
              border: `1px solid ${value === i ? color : '#2a2a3a'}`,
              borderRadius: 4,
              padding: '3px 6px',
              cursor: 'pointer',
              fontSize: 9,
              color: value === i ? color : '#6a6a8a',
              letterSpacing: '0.08em',
              fontWeight: value === i ? 700 : 400,
              transition: 'all 0.1s',
            }}
          >
            {opt}
          </button>
        ))}
      </div>
      <span style={{
        fontSize: 9, color: '#5a5a7a',
        textTransform: 'uppercase', letterSpacing: '0.12em',
      }}>
        {label}
      </span>
    </div>
  );
};

// ============================================================
// Section panel container
// ============================================================
interface SectionProps {
  title: string;
  enabled?: boolean;
  onToggle?: (v: boolean) => void;
  color?: string;
  children: React.ReactNode;
  accentColor?: string;
}

export const Section: React.FC<SectionProps> = ({
  title, enabled, onToggle, children, accentColor = '#c8a84b',
}) => {
  const isToggleable = onToggle !== undefined;

  return (
    <div style={{
      background: '#14141e',
      border: `1px solid ${enabled !== false ? accentColor + '44' : '#2a2a3a'}`,
      borderRadius: 10,
      padding: '12px 14px',
      display: 'flex',
      flexDirection: 'column',
      gap: 12,
      opacity: isToggleable && !enabled ? 0.6 : 1,
      transition: 'opacity 0.2s, border-color 0.2s',
      minWidth: 0,
    }}>
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        gap: 8,
      }}>
        <span style={{
          fontSize: 10,
          fontWeight: 700,
          color: enabled !== false ? accentColor : '#4a4a6a',
          textTransform: 'uppercase',
          letterSpacing: '0.18em',
          transition: 'color 0.2s',
        }}>
          {title}
        </span>
        {isToggleable && (
          <button
            onClick={() => onToggle(!enabled)}
            style={{
              width: 32, height: 16,
              background: enabled ? accentColor + '33' : '#1e1e28',
              border: `1px solid ${enabled ? accentColor : '#3a3a4a'}`,
              borderRadius: 8,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              padding: 2,
              justifyContent: enabled ? 'flex-end' : 'flex-start',
              transition: 'all 0.15s',
            }}
          >
            <div style={{
              width: 10, height: 10,
              borderRadius: '50%',
              background: enabled ? accentColor : '#3a3a4a',
              boxShadow: enabled ? `0 0 4px ${accentColor}` : 'none',
            }} />
          </button>
        )}
      </div>
      <div style={{
        pointerEvents: isToggleable && !enabled ? 'none' : 'auto',
        display: 'flex',
        gap: 12,
        flexWrap: 'wrap',
        alignItems: 'flex-start',
      }}>
        {children}
      </div>
    </div>
  );
};
