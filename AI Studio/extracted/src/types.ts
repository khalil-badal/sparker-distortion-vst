export type CircuitType = 'EMBER' | 'BURNER' | 'BLAZER';
export type EmberMode = 'WARM' | 'CLASSIC' | 'HOT';
export type AggressionMode = 'BLUE' | 'RED';
export type OversamplingOption = '1x' | '2x' | '4x' | '8x' | '16x';

export interface PluginState {
  // Common state
  engaged: boolean;
  circuit: CircuitType;
  inputDb: number; // -12 to +12 dB
  outputDb: number; // -12 to +12 dB
  
  // EMBER state
  drive: number; // 0 - 10
  level: number; // 0 - 10
  tone: number; // 0 - 10
  tight: number; // 0 - 10
  gate: number; // 0 - 10
  mode: EmberMode;
  oversampling: OversamplingOption;

  // BURNER state
  burnerGain: number; // 0 - 10
  burnerVolume: number; // 0 - 10
  bass: number; // 0 - 10
  mid: number; // 0 - 10
  treble: number; // 0 - 10
  aggression: AggressionMode;

  // BLAZER state
  blazerGain: number; // 0 - 10
  blazerVolume: number; // 0 - 10
  blazerBass: number; // 0 - 10
  blazerTreble: number; // 0 - 10
  presence: number; // 0 - 10
  blazerTight: number; // 0 - 10
  trim: number; // 0 - 10
}

export interface Preset {
  id: string;
  name: string;
  description: string;
  state: Partial<PluginState>;
}

export interface ThemeColors {
  primary: string; // hex
  primaryGlow: string; // rgba or hex
  accent: string;
  border: string;
  text: string;
  ringArc: string;
  ledColor: string;
  ledShadow: string;
}
