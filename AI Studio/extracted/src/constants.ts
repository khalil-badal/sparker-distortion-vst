import { PluginState, ThemeColors, CircuitType, Preset } from './types';

export const DEFAULT_PLUGIN_STATE: PluginState = {
  engaged: true,
  circuit: 'EMBER',
  inputDb: 0.0,
  outputDb: 0.0,
  
  // EMBER
  drive: 6.5,
  level: 7.0,
  tone: 5.0,
  tight: 3.0,
  gate: 2.0,
  mode: 'WARM',
  oversampling: '8x',

  // BURNER
  burnerGain: 7.5,
  burnerVolume: 7.0,
  bass: 6.0,
  mid: 5.0,
  treble: 6.5,
  aggression: 'BLUE',

  // BLAZER
  blazerGain: 8.0,
  blazerVolume: 6.8,
  blazerBass: 5.5,
  blazerTreble: 6.0,
  presence: 5.0,
  blazerTight: 4.0,
  trim: 3.5,
};

export const CIRCUIT_THEMES: Record<CircuitType, ThemeColors> = {
  EMBER: {
    primary: '#d946ef',
    primaryGlow: 'rgba(217, 70, 239, 0.65)',
    accent: '#f0abfc',
    border: '#a21caf',
    text: '#f5d0fe',
    ringArc: '#d946ef',
    ledColor: '#f0abfc',
    ledShadow: '0 0 16px #d946ef, 0 0 32px rgba(217,70,239,0.85)',
  },
  BURNER: {
    primary: '#38bdf8',
    primaryGlow: 'rgba(56, 189, 248, 0.6)',
    accent: '#7dd3fc',
    border: '#0284c7',
    text: '#bae6fd',
    ringArc: '#38bdf8',
    ledColor: '#38bdf8',
    ledShadow: '0 0 14px #38bdf8, 0 0 28px rgba(56,189,248,0.8)',
  },
  BLAZER: {
    primary: '#f59e0b',
    primaryGlow: 'rgba(245, 158, 11, 0.6)',
    accent: '#fbbf24',
    border: '#b45309',
    text: '#fef08a',
    ringArc: '#f59e0b',
    ledColor: '#f59e0b',
    ledShadow: '0 0 14px #f59e0b, 0 0 28px rgba(245,158,11,0.8)',
  },
};

export const RED_BURNER_THEME: ThemeColors = {
  primary: '#f43f5e',
  primaryGlow: 'rgba(244, 63, 94, 0.6)',
  accent: '#fb7185',
  border: '#9f1239',
  text: '#fecdd3',
  ringArc: '#f43f5e',
  ledColor: '#f43f5e',
  ledShadow: '0 0 14px #f43f5e, 0 0 28px rgba(244,63,94,0.8)',
};

export function getCircuitTheme(state: PluginState): ThemeColors {
  if (state.circuit === 'BURNER' && state.aggression === 'RED') {
    return RED_BURNER_THEME;
  }
  return CIRCUIT_THEMES[state.circuit];
}

export const PRESETS: Preset[] = [
  {
    id: 'ember-warm-lead',
    name: 'Ember - Warm Tube Lead',
    description: 'Smooth liquid lead tone with rich mid harmonic saturation & warm tube response',
    state: {
      circuit: 'EMBER',
      drive: 7.2,
      level: 6.8,
      tone: 5.8,
      tight: 2.5,
      gate: 1.5,
      mode: 'WARM',
      oversampling: '8x',
    },
  },
  {
    id: 'ember-hot-fuzz',
    name: 'Ember - Hot Germanium Fuzz',
    description: 'Screaming high-gain fuzzy crunch with aggressive upper bite and sustain',
    state: {
      circuit: 'EMBER',
      drive: 9.0,
      level: 5.5,
      tone: 7.2,
      tight: 1.0,
      gate: 3.0,
      mode: 'HOT',
      oversampling: '4x',
    },
  },
  {
    id: 'burner-high-gain-rhythm',
    name: 'Burner - Modern Rhythm',
    description: 'Aggressive scoop with punchy low end and roaring dual gain stage',
    state: {
      circuit: 'BURNER',
      burnerGain: 8.5,
      burnerVolume: 6.5,
      bass: 7.0,
      mid: 4.2,
      treble: 7.5,
      aggression: 'BLUE',
      gate: 3.5,
      oversampling: '4x',
    },
  },
  {
    id: 'burner-red-thrash',
    name: 'Burner - Red Thrash Metal',
    description: 'High-speed palm-mute attack with tight noise gate and extreme aggression',
    state: {
      circuit: 'BURNER',
      burnerGain: 9.5,
      burnerVolume: 6.0,
      bass: 8.2,
      mid: 3.0,
      treble: 8.8,
      aggression: 'RED',
      gate: 6.5,
      oversampling: '8x',
    },
  },
  {
    id: 'blazer-searing-solo',
    name: 'Blazer - Searing Crunch',
    description: 'Tight focus with maximum presence, zero mud, and razor sharp clarity',
    state: {
      circuit: 'BLAZER',
      blazerGain: 8.0,
      blazerVolume: 7.2,
      blazerBass: 5.0,
      blazerTreble: 6.8,
      presence: 7.5,
      blazerTight: 5.0,
      trim: 4.0,
      oversampling: '4x',
    },
  },
  {
    id: 'blazer-djent-edge',
    name: 'Blazer - Djent Edge Cut',
    description: 'Ultra-tight low-cut boost designed to push high-gain tube amps into modern djent precision',
    state: {
      circuit: 'BLAZER',
      blazerGain: 6.5,
      blazerVolume: 8.5,
      blazerBass: 3.5,
      blazerTreble: 8.0,
      presence: 8.8,
      blazerTight: 8.5,
      trim: 5.5,
      oversampling: '16x',
    },
  },
];
