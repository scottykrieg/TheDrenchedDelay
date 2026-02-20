import React from 'react';
import { usePluginBridge, normLin, denormLin, normLog, denormLog } from './hooks/usePluginBridge';
import { ParamID, NOTE_DIVISIONS, MODIFIERS, FILTER_TYPES, HARM_MODES, HARM_INTERVALS } from './types/plugin';
import { Knob } from './components/Knob';
import { Toggle, LedButton, Selector, Section } from './components/Controls';

// ============================================================
// Colour palette
// ============================================================
const GOLD   = '#c8a84b';
const TEAL   = '#4bc8a8';
const ROSE   = '#c84b6e';
const VIOLET = '#8b6bc8';
const BLUE   = '#4b7ec8';
const GREEN  = '#6bc84b';
const ORANGE = '#c88b4b';

// ============================================================
// App
// ============================================================
export default function App() {
  const { params, setParam, tapTempo } = usePluginBridge();

  // Helper: send normalised value
  function p(id: string, rawValue: number, min: number, max: number, log = false) {
    const norm = log ? normLog(rawValue, min, max) : normLin(rawValue, min, max);
    setParam(id, norm);
  }

  // Helper: read normalised → raw
  function raw(id: string, min: number, max: number, log = false): number {
    const norm = params[id as keyof typeof params] as number;
    return log ? denormLog(norm, min, max) : denormLin(norm, min, max);
  }

  // For bool params stored as 0/1:
  function bool(id: string): boolean {
    return (params[id as keyof typeof params] as number) > 0.5;
  }
  function setBool(id: string, v: boolean) {
    setParam(id, v ? 1 : 0);
  }

  // Choice params stored as float index:
  function choice(id: string): number {
    return Math.round(params[id as keyof typeof params] as number);
  }
  function setChoice(id: string, idx: number) {
    setParam(id, idx);
  }

  const syncOn = bool(ParamID.syncEnabled);
  const bpm = params._bpm.toFixed(1);

  return (
    <div style={{
      width: '100%', height: '100%',
      background: '#0e0e12',
      color: '#c0b090',
      fontFamily: '"SF Mono", "Fira Code", monospace',
      display: 'flex',
      flexDirection: 'column',
      overflow: 'auto',
      padding: 16,
      gap: 12,
    }}>
      {/* =============================== HEADER =============================== */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        borderBottom: `1px solid ${GOLD}33`,
        paddingBottom: 10,
      }}>
        <div>
          <span style={{
            fontSize: 22, fontWeight: 900,
            color: GOLD, letterSpacing: '0.22em',
          }}>CHROMA</span>
          <span style={{
            fontSize: 22, fontWeight: 300,
            color: '#7a7a9a', letterSpacing: '0.22em',
          }}> DELAY</span>
        </div>
        <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
          <span style={{ fontSize: 11, color: '#5a5a7a' }}>♩ {bpm} BPM</span>
          <LedButton label="TAP" onPress={tapTempo} color={GOLD} wide subtitle="TEMPO" />
        </div>
      </div>

      {/* =============================== MAIN ROW =============================== */}
      <div style={{ display: 'flex', gap: 12, flexWrap: 'wrap' }}>

        {/* --- CORE DELAY --- */}
        <Section title="Delay" accentColor={GOLD}>
          <Knob
            label="Time"
            value={raw(ParamID.delayTimeMs, 1, 2000, true)}
            min={1} max={2000}
            color={GOLD} size={68}
            normalize={v => normLog(v, 1, 2000)}
            denormalize={n => denormLog(n, 1, 2000)}
            displayValue={v => `${v.toFixed(0)} ms`}
            onChange={v => p(ParamID.delayTimeMs, v, 1, 2000, true)}
            disabled={syncOn}
          />
          <Knob
            label="Feedback"
            value={raw(ParamID.feedback, 0, 0.98)}
            min={0} max={0.98}
            color={ROSE} size={68}
            displayValue={v => `${(v * 100).toFixed(0)}%`}
            onChange={v => p(ParamID.feedback, v, 0, 0.98)}
          />
          <Knob
            label="Mix"
            value={raw(ParamID.mix, 0, 1)}
            min={0} max={1}
            color={GOLD} size={68}
            displayValue={v => `${(v * 100).toFixed(0)}%`}
            onChange={v => p(ParamID.mix, v, 0, 1)}
          />
          <Knob
            label="In Gain"
            value={raw(ParamID.inputGain, -24, 24)}
            min={-24} max={24}
            color={TEAL} size={56}
            unit=" dB" decimals={1}
            onChange={v => p(ParamID.inputGain, v, -24, 24)}
          />
          <Knob
            label="Output"
            value={raw(ParamID.outputLevel, -24, 12)}
            min={-24} max={12}
            color={TEAL} size={56}
            unit=" dB" decimals={1}
            onChange={v => p(ParamID.outputLevel, v, -24, 12)}
          />
          <Knob
            label="Slop"
            value={raw(ParamID.gridOffset, -20, 20)}
            min={-20} max={20}
            color={ORANGE} size={56}
            unit=" ms" decimals={1}
            onChange={v => p(ParamID.gridOffset, v, -20, 20)}
          />
          <div style={{ display: 'flex', flexDirection: 'column', gap: 8, alignSelf: 'center' }}>
            <Toggle label="Phase ⊘" value={bool(ParamID.phaseInvert)} onChange={v => setBool(ParamID.phaseInvert, v)} color={ROSE} size="sm" />
            <Toggle label="🔒 Hold" value={bool(ParamID.lockHold)} onChange={v => setBool(ParamID.lockHold, v)} color={ROSE} size="sm" />
          </div>
        </Section>

        {/* --- SYNC / RHYTHM --- */}
        <Section title="Sync" accentColor={BLUE}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
            <Toggle label="BPM Sync" value={syncOn} onChange={v => setBool(ParamID.syncEnabled, v)} color={BLUE} />
            <Selector
              label="Division"
              options={NOTE_DIVISIONS}
              value={choice(ParamID.noteDivision)}
              onChange={i => setChoice(ParamID.noteDivision, i)}
              color={BLUE}
            />
            <Selector
              label="Modifier"
              options={MODIFIERS}
              value={choice(ParamID.modifier)}
              onChange={i => setChoice(ParamID.modifier, i)}
              color={BLUE}
            />
          </div>
        </Section>
      </div>

      {/* =============================== EFFECTS ROW =============================== */}
      <div style={{ display: 'flex', gap: 10, flexWrap: 'wrap' }}>

        {/* --- BITCRUSHER --- */}
        <Section title="Bitcrusher" enabled={bool(ParamID.bcEnabled)}
          onToggle={v => setBool(ParamID.bcEnabled, v)} accentColor={ORANGE}>
          <Knob
            label="Bits"
            value={raw(ParamID.bcBits, 2, 16)}
            min={2} max={16} step={0.5}
            color={ORANGE} size={56}
            displayValue={v => `${v.toFixed(1)} bit`}
            onChange={v => p(ParamID.bcBits, v, 2, 16)}
            disabled={!bool(ParamID.bcEnabled)}
          />
          <Knob
            label="Rate ÷"
            value={raw(ParamID.bcSampleRateDiv, 1, 32)}
            min={1} max={32} step={0.5}
            color={ORANGE} size={56}
            displayValue={v => `÷${v.toFixed(1)}`}
            onChange={v => p(ParamID.bcSampleRateDiv, v, 1, 32)}
            disabled={!bool(ParamID.bcEnabled)}
          />
        </Section>

        {/* --- SATURATION --- */}
        <Section title="Saturation" enabled={bool(ParamID.satEnabled)}
          onToggle={v => setBool(ParamID.satEnabled, v)} accentColor={ROSE}>
          <Knob
            label="Drive"
            value={raw(ParamID.satDrive, 0, 1)}
            min={0} max={1}
            color={ROSE} size={64}
            displayValue={v => `${(v * 100).toFixed(0)}%`}
            onChange={v => p(ParamID.satDrive, v, 0, 1)}
            disabled={!bool(ParamID.satEnabled)}
          />
        </Section>

        {/* --- FILTER --- */}
        <Section title="Resonant Filter" enabled={bool(ParamID.filtEnabled)}
          onToggle={v => setBool(ParamID.filtEnabled, v)} accentColor={TEAL}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
            <Selector
              label="Type"
              options={FILTER_TYPES}
              value={choice(ParamID.filtType)}
              onChange={i => setChoice(ParamID.filtType, i)}
              color={TEAL}
            />
            <div style={{ display: 'flex', gap: 8 }}>
              <Knob
                label="Cutoff"
                value={raw(ParamID.filtCutoff, 20, 20000, true)}
                min={20} max={20000}
                color={TEAL} size={56}
                normalize={v => normLog(v, 20, 20000)}
                denormalize={n => denormLog(n, 20, 20000)}
                displayValue={v => v >= 1000 ? `${(v/1000).toFixed(1)}k` : `${v.toFixed(0)}`}
                onChange={v => p(ParamID.filtCutoff, v, 20, 20000, true)}
                disabled={!bool(ParamID.filtEnabled)}
              />
              <Knob
                label="Resonance"
                value={raw(ParamID.filtResonance, 0.5, 10)}
                min={0.5} max={10}
                color={TEAL} size={56}
                displayValue={v => `Q ${v.toFixed(2)}`}
                onChange={v => p(ParamID.filtResonance, v, 0.5, 10)}
                disabled={!bool(ParamID.filtEnabled)}
              />
            </div>
          </div>
        </Section>

        {/* --- CHORUS --- */}
        <Section title="Chorus" enabled={bool(ParamID.chorusEnabled)}
          onToggle={v => setBool(ParamID.chorusEnabled, v)} accentColor={VIOLET}>
          <Knob
            label="Rate"
            value={raw(ParamID.chorusRate, 0.01, 10, true)}
            min={0.01} max={10}
            color={VIOLET} size={52}
            normalize={v => normLog(v, 0.01, 10)}
            denormalize={n => denormLog(n, 0.01, 10)}
            displayValue={v => `${v.toFixed(2)} Hz`}
            onChange={v => p(ParamID.chorusRate, v, 0.01, 10, true)}
            disabled={!bool(ParamID.chorusEnabled)}
          />
          <Knob
            label="Depth"
            value={raw(ParamID.chorusDepth, 0, 1)}
            min={0} max={1}
            color={VIOLET} size={52}
            displayValue={v => `${(v*100).toFixed(0)}%`}
            onChange={v => p(ParamID.chorusDepth, v, 0, 1)}
            disabled={!bool(ParamID.chorusEnabled)}
          />
          <Knob
            label="Mix"
            value={raw(ParamID.chorusMix, 0, 1)}
            min={0} max={1}
            color={VIOLET} size={52}
            displayValue={v => `${(v*100).toFixed(0)}%`}
            onChange={v => p(ParamID.chorusMix, v, 0, 1)}
            disabled={!bool(ParamID.chorusEnabled)}
          />
        </Section>

        {/* --- PHASER --- */}
        <Section title="Phaser" enabled={bool(ParamID.phaserEnabled)}
          onToggle={v => setBool(ParamID.phaserEnabled, v)} accentColor={BLUE}>
          <Knob
            label="Rate"
            value={raw(ParamID.phaserRate, 0.01, 5, true)}
            min={0.01} max={5}
            color={BLUE} size={52}
            normalize={v => normLog(v, 0.01, 5)}
            denormalize={n => denormLog(n, 0.01, 5)}
            displayValue={v => `${v.toFixed(2)} Hz`}
            onChange={v => p(ParamID.phaserRate, v, 0.01, 5, true)}
            disabled={!bool(ParamID.phaserEnabled)}
          />
          <Knob
            label="Depth"
            value={raw(ParamID.phaserDepth, 0, 1)}
            min={0} max={1}
            color={BLUE} size={52}
            displayValue={v => `${(v*100).toFixed(0)}%`}
            onChange={v => p(ParamID.phaserDepth, v, 0, 1)}
            disabled={!bool(ParamID.phaserEnabled)}
          />
          <Knob
            label="FB"
            value={raw(ParamID.phaserFeedback, 0, 0.95)}
            min={0} max={0.95}
            color={BLUE} size={52}
            displayValue={v => `${(v*100).toFixed(0)}%`}
            onChange={v => p(ParamID.phaserFeedback, v, 0, 0.95)}
            disabled={!bool(ParamID.phaserEnabled)}
          />
        </Section>

        {/* --- TREMOLO --- */}
        <Section title="Tremolo" enabled={bool(ParamID.tremEnabled)}
          onToggle={v => setBool(ParamID.tremEnabled, v)} accentColor={GREEN}>
          <Knob
            label="Rate"
            value={raw(ParamID.tremRate, 0.1, 20, true)}
            min={0.1} max={20}
            color={GREEN} size={52}
            normalize={v => normLog(v, 0.1, 20)}
            denormalize={n => denormLog(n, 0.1, 20)}
            displayValue={v => `${v.toFixed(2)} Hz`}
            onChange={v => p(ParamID.tremRate, v, 0.1, 20, true)}
            disabled={!bool(ParamID.tremEnabled)}
          />
          <Knob
            label="Depth"
            value={raw(ParamID.tremDepth, 0, 1)}
            min={0} max={1}
            color={GREEN} size={52}
            displayValue={v => `${(v*100).toFixed(0)}%`}
            onChange={v => p(ParamID.tremDepth, v, 0, 1)}
            disabled={!bool(ParamID.tremEnabled)}
          />
          <Toggle
            label="Sync"
            value={bool(ParamID.tremSync)}
            onChange={v => setBool(ParamID.tremSync, v)}
            color={GREEN} size="sm"
          />
        </Section>
      </div>

      {/* =============================== UTILITY ROW =============================== */}
      <div style={{ display: 'flex', gap: 10, flexWrap: 'wrap' }}>

        {/* --- HARMONIZER --- */}
        <Section title="Harmonizer" enabled={bool(ParamID.harmEnabled)}
          onToggle={v => setBool(ParamID.harmEnabled, v)} accentColor={GOLD}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
            <Selector
              label="Mode"
              options={HARM_MODES}
              value={choice(ParamID.harmMode)}
              onChange={i => setChoice(ParamID.harmMode, i)}
              color={GOLD}
            />
            <div style={{ display: 'flex', gap: 8, alignItems: 'flex-end' }}>
              <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                <select
                  value={Math.round(raw(ParamID.harmInterval, -24, 24))}
                  onChange={e => p(ParamID.harmInterval, parseInt(e.target.value), -24, 24)}
                  disabled={!bool(ParamID.harmEnabled)}
                  style={{
                    background: '#1a1a24', color: '#c0b090',
                    border: `1px solid ${GOLD}55`,
                    borderRadius: 4, padding: '4px 8px',
                    fontSize: 10, letterSpacing: '0.05em',
                    outline: 'none', cursor: 'pointer',
                    maxWidth: 150,
                  }}
                >
                  {HARM_INTERVALS.map((label, i) => (
                    <option key={i} value={i - 24}>{label}</option>
                  ))}
                </select>
                <span style={{ fontSize: 9, color: '#5a5a7a', textAlign: 'center' }}>INTERVAL</span>
              </div>
              <Knob
                label="Mix"
                value={raw(ParamID.harmMix, 0, 1)}
                min={0} max={1}
                color={GOLD} size={52}
                displayValue={v => `${(v*100).toFixed(0)}%`}
                onChange={v => p(ParamID.harmMix, v, 0, 1)}
                disabled={!bool(ParamID.harmEnabled)}
              />
            </div>
          </div>
        </Section>

        {/* --- GHOST DELAY --- */}
        <Section title="Ghost Delay" enabled={bool(ParamID.ghostEnabled)}
          onToggle={v => setBool(ParamID.ghostEnabled, v)} accentColor={VIOLET}>
          <Knob
            label="Time"
            value={raw(ParamID.ghostTime, 1, 1000, true)}
            min={1} max={1000}
            color={VIOLET} size={56}
            normalize={v => normLog(v, 1, 1000)}
            denormalize={n => denormLog(n, 1, 1000)}
            displayValue={v => `${v.toFixed(0)} ms`}
            onChange={v => p(ParamID.ghostTime, v, 1, 1000, true)}
            disabled={!bool(ParamID.ghostEnabled)}
          />
          <Knob
            label="Feedback"
            value={raw(ParamID.ghostFeedback, 0, 0.95)}
            min={0} max={0.95}
            color={VIOLET} size={56}
            displayValue={v => `${(v*100).toFixed(0)}%`}
            onChange={v => p(ParamID.ghostFeedback, v, 0, 0.95)}
            disabled={!bool(ParamID.ghostEnabled)}
          />
        </Section>

        {/* --- REVERB --- */}
        <Section title="Reverb" enabled={bool(ParamID.reverbEnabled)}
          onToggle={v => setBool(ParamID.reverbEnabled, v)} accentColor={TEAL}>
          <Knob
            label="Size"
            value={raw(ParamID.reverbSize, 0, 1)}
            min={0} max={1}
            color={TEAL} size={52}
            displayValue={v => `${(v*100).toFixed(0)}%`}
            onChange={v => p(ParamID.reverbSize, v, 0, 1)}
            disabled={!bool(ParamID.reverbEnabled)}
          />
          <Knob
            label="Damp"
            value={raw(ParamID.reverbDamping, 0, 1)}
            min={0} max={1}
            color={TEAL} size={52}
            displayValue={v => `${(v*100).toFixed(0)}%`}
            onChange={v => p(ParamID.reverbDamping, v, 0, 1)}
            disabled={!bool(ParamID.reverbEnabled)}
          />
          <Knob
            label="Mix"
            value={raw(ParamID.reverbMix, 0, 1)}
            min={0} max={1}
            color={TEAL} size={52}
            displayValue={v => `${(v*100).toFixed(0)}%`}
            onChange={v => p(ParamID.reverbMix, v, 0, 1)}
            disabled={!bool(ParamID.reverbEnabled)}
          />
        </Section>

        {/* --- REVERSE --- */}
        <Section title="Reverse" enabled={bool(ParamID.reverseEnabled)}
          onToggle={v => setBool(ParamID.reverseEnabled, v)} accentColor={ROSE}>
          <div style={{
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            padding: '8px 16px', color: '#5a5a7a', fontSize: 10,
            textAlign: 'center', maxWidth: 120,
          }}>
            Flips repeats backwards for ghostly swells
          </div>
        </Section>

      </div>

      {/* =============================== FOOTER =============================== */}
      <div style={{
        marginTop: 'auto',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        borderTop: `1px solid #2a2a3a`,
        paddingTop: 8,
        fontSize: 9,
        color: '#3a3a5a',
        letterSpacing: '0.1em',
      }}>
        <span>CHROMA DELAY v1.0.0</span>
        <span>Shift+drag = fine control · Scroll = adjust</span>
      </div>
    </div>
  );
}
