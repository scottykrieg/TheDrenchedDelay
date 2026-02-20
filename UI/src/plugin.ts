// ============================================================
// Parameter IDs — must match ParamID:: namespace in C++
// ============================================================
export const ParamID = {
  // Core
  delayTimeMs:    'delayTimeMs',
  feedback:       'feedback',
  mix:            'mix',
  inputGain:      'inputGain',
  outputLevel:    'outputLevel',
  phaseInvert:    'phaseInvert',
  // Rhythm
  syncEnabled:    'syncEnabled',
  noteDivision:   'noteDivision',
  modifier:       'modifier',
  gridOffset:     'gridOffset',
  lockHold:       'lockHold',
  // Bitcrusher
  bcEnabled:      'bcEnabled',
  bcBits:         'bcBits',
  bcSampleRateDiv:'bcSampleRateDiv',
  // Saturation
  satEnabled:     'satEnabled',
  satDrive:       'satDrive',
  // Filter
  filtEnabled:    'filtEnabled',
  filtType:       'filtType',
  filtCutoff:     'filtCutoff',
  filtResonance:  'filtResonance',
  // Chorus
  chorusEnabled:  'chorusEnabled',
  chorusRate:     'chorusRate',
  chorusDepth:    'chorusDepth',
  chorusMix:      'chorusMix',
  // Phaser
  phaserEnabled:  'phaserEnabled',
  phaserRate:     'phaserRate',
  phaserDepth:    'phaserDepth',
  phaserFeedback: 'phaserFeedback',
  // Tremolo
  tremEnabled:    'tremEnabled',
  tremRate:       'tremRate',
  tremDepth:      'tremDepth',
  tremSync:       'tremSync',
  // Harmonizer
  harmEnabled:    'harmEnabled',
  harmInterval:   'harmInterval',
  harmMode:       'harmMode',
  harmMix:        'harmMix',
  // Ghost Delay
  ghostEnabled:   'ghostEnabled',
  ghostTime:      'ghostTime',
  ghostFeedback:  'ghostFeedback',
  // Reverb
  reverbEnabled:  'reverbEnabled',
  reverbSize:     'reverbSize',
  reverbDamping:  'reverbDamping',
  reverbMix:      'reverbMix',
  // Reverse
  reverseEnabled: 'reverseEnabled',
} as const;

export type ParamIDType = typeof ParamID[keyof typeof ParamID];

// ============================================================
// Parameter definitions for display/range/defaults
// ============================================================
export interface ParamDef {
  id: string;
  label: string;
  min: number;
  max: number;
  default: number;
  step?: number;
  unit?: string;
  decimals?: number;
  /** Maps raw value to display string */
  display?: (v: number) => string;
  /** Raw value is normalised 0..1 for sending to JUCE */
  normalize: (v: number) => number;
  /** 0..1 → raw value */
  denormalize: (n: number) => number;
}

function linMap(min: number, max: number) {
  return {
    normalize:   (v: number) => (v - min) / (max - min),
    denormalize: (n: number) => min + n * (max - min),
  };
}

function logMap(min: number, max: number) {
  const logMin = Math.log(min), logMax = Math.log(max);
  return {
    normalize:   (v: number) => (Math.log(v) - logMin) / (logMax - logMin),
    denormalize: (n: number) => Math.exp(logMin + n * (logMax - logMin)),
  };
}

// Note division labels
export const NOTE_DIVISIONS = ['1/1','1/2','1/4','1/8','1/16','1/32'];
export const MODIFIERS       = ['Normal','Dotted','Triplet'];
export const FILTER_TYPES    = ['Low Pass','High Pass','Band Pass'];
export const HARM_MODES      = ['Static','Cascade'];

// Harmonizer interval labels (−24 to +24 semitones, stored as int)
export const HARM_INTERVALS: string[] = [];
const INTERVAL_NAMES = ['P1','m2','M2','m3','M3','P4','TT','P5','m6','M6','m7','M7','P8',
                         'm9','M9','m10','M10','P11','TT+','P12','m13','M13','m14','M14','P15'];
for (let i = -24; i <= 24; i++) {
  const abs = Math.abs(i);
  const name = INTERVAL_NAMES[abs] ?? `${abs}st`;
  HARM_INTERVALS.push(i === 0 ? 'Unison' : `${i > 0 ? '+' : '−'}${name} (${i > 0 ? '+' : ''}${i})`);
}

// ============================================================
// Plugin state — flat record matching all APVTS parameter IDs
// plus internal helpers
// ============================================================
export interface PluginState {
  // Core
  delayTimeMs:    number;
  feedback:       number;
  mix:            number;
  inputGain:      number;
  outputLevel:    number;
  phaseInvert:    number;
  syncEnabled:    number;
  noteDivision:   number;
  modifier:       number;
  gridOffset:     number;
  lockHold:       number;
  // Bitcrusher
  bcEnabled:      number;
  bcBits:         number;
  bcSampleRateDiv:number;
  // Saturation
  satEnabled:     number;
  satDrive:       number;
  // Filter
  filtEnabled:    number;
  filtType:       number;
  filtCutoff:     number;
  filtResonance:  number;
  // Chorus
  chorusEnabled:  number;
  chorusRate:     number;
  chorusDepth:    number;
  chorusMix:      number;
  // Phaser
  phaserEnabled:  number;
  phaserRate:     number;
  phaserDepth:    number;
  phaserFeedback: number;
  // Tremolo
  tremEnabled:    number;
  tremRate:       number;
  tremDepth:      number;
  tremSync:       number;
  // Harmonizer
  harmEnabled:    number;
  harmInterval:   number;
  harmMode:       number;
  harmMix:        number;
  // Ghost Delay
  ghostEnabled:   number;
  ghostTime:      number;
  ghostFeedback:  number;
  // Reverb
  reverbEnabled:  number;
  reverbSize:     number;
  reverbDamping:  number;
  reverbMix:      number;
  // Reverse
  reverseEnabled: number;
  // Internal (not APVTS params)
  _bpm:           number;
}

export const defaultState: PluginState = {
  delayTimeMs:    250,
  feedback:       0.4,
  mix:            0.5,
  inputGain:      0,
  outputLevel:    0,
  phaseInvert:    0,
  syncEnabled:    0,
  noteDivision:   2,
  modifier:       0,
  gridOffset:     0,
  lockHold:       0,
  bcEnabled:      0, bcBits: 8, bcSampleRateDiv: 1,
  satEnabled:     0, satDrive: 0.3,
  filtEnabled:    0, filtType: 0, filtCutoff: 2000, filtResonance: 0.707,
  chorusEnabled:  0, chorusRate: 0.5, chorusDepth: 0.3, chorusMix: 0.5,
  phaserEnabled:  0, phaserRate: 0.2, phaserDepth: 0.5, phaserFeedback: 0.5,
  tremEnabled:    0, tremRate: 4, tremDepth: 0.7, tremSync: 0,
  harmEnabled:    0, harmInterval: 7, harmMode: 0, harmMix: 0.5,
  ghostEnabled:   0, ghostTime: 125, ghostFeedback: 0.3,
  reverbEnabled:  0, reverbSize: 0.5, reverbDamping: 0.5, reverbMix: 0.3,
  reverseEnabled: 0,
  _bpm:           120,
};
