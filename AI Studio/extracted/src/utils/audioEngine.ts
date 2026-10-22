import { PluginState, OversamplingOption } from '../types';

export class SparkerAudioEngine {
  private ctx: AudioContext | null = null;
  private inputGainNode: GainNode | null = null;
  private outputGainNode: GainNode | null = null;
  private waveShaperNode: WaveShaperNode | null = null;
  private tightFilterNode: BiquadFilterNode | null = null;
  private toneFilterNode: BiquadFilterNode | null = null;
  private bassFilterNode: BiquadFilterNode | null = null;
  private midFilterNode: BiquadFilterNode | null = null;
  private trebleFilterNode: BiquadFilterNode | null = null;
  private presenceFilterNode: BiquadFilterNode | null = null;
  private bypassGainNode: GainNode | null = null;
  private wetGainNode: GainNode | null = null;
  private inputAnalyser: AnalyserNode | null = null;
  private outputAnalyser: AnalyserNode | null = null;
  private sourceNode: AudioBufferSourceNode | OscillatorNode | null = null;
  private isPlayingDemo: boolean = false;
  private meterDataArray: Uint8Array = new Uint8Array(16);

  public init() {
    if (this.ctx) return;
    const AudioCtxClass = window.AudioContext || (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
    this.ctx = new AudioCtxClass();

    this.inputGainNode = this.ctx.createGain();
    this.outputGainNode = this.ctx.createGain();
    this.waveShaperNode = this.ctx.createWaveShaper();
    this.tightFilterNode = this.ctx.createBiquadFilter();
    this.toneFilterNode = this.ctx.createBiquadFilter();
    this.bassFilterNode = this.ctx.createBiquadFilter();
    this.midFilterNode = this.ctx.createBiquadFilter();
    this.trebleFilterNode = this.ctx.createBiquadFilter();
    this.presenceFilterNode = this.ctx.createBiquadFilter();

    this.bypassGainNode = this.ctx.createGain();
    this.wetGainNode = this.ctx.createGain();

    this.inputAnalyser = this.ctx.createAnalyser();
    this.outputAnalyser = this.ctx.createAnalyser();
    this.inputAnalyser.fftSize = 32;
    this.outputAnalyser.fftSize = 32;

    // Filter configurations
    this.tightFilterNode.type = 'highpass';
    this.toneFilterNode.type = 'lowpass';
    this.bassFilterNode.type = 'lowshelf';
    this.bassFilterNode.frequency.value = 120;
    this.midFilterNode.type = 'peaking';
    this.midFilterNode.frequency.value = 750;
    this.midFilterNode.Q.value = 1.2;
    this.trebleFilterNode.type = 'highshelf';
    this.trebleFilterNode.frequency.value = 3200;
    this.presenceFilterNode.type = 'highshelf';
    this.presenceFilterNode.frequency.value = 6000;

    // Connect routing graph:
    // Input -> InputAnalyser -> InputGain -> TightFilter -> WaveShaper -> EQ chain -> WetGain -> OutputGain -> OutputAnalyser -> Destination
    // Input -> BypassGain -> OutputGain
    this.inputGainNode.connect(this.inputAnalyser);
    this.inputGainNode.connect(this.tightFilterNode);

    this.tightFilterNode.connect(this.waveShaperNode);
    this.waveShaperNode.connect(this.toneFilterNode);
    this.toneFilterNode.connect(this.bassFilterNode);
    this.bassFilterNode.connect(this.midFilterNode);
    this.midFilterNode.connect(this.trebleFilterNode);
    this.trebleFilterNode.connect(this.presenceFilterNode);
    this.presenceFilterNode.connect(this.wetGainNode);

    this.wetGainNode.connect(this.outputGainNode);

    // Bypass path
    this.inputGainNode.connect(this.bypassGainNode);
    this.bypassGainNode.connect(this.outputGainNode);

    this.outputGainNode.connect(this.outputAnalyser);
    this.outputAnalyser.connect(this.ctx.destination);
  }

  public updateParams(state: PluginState) {
    if (!this.ctx) return;

    // Input & Output gains
    const inputLinear = Math.pow(10, state.inputDb / 20);
    const outputLinear = Math.pow(10, state.outputDb / 20);

    if (this.inputGainNode) this.inputGainNode.gain.setTargetAtTime(inputLinear, this.ctx.currentTime, 0.02);
    if (this.outputGainNode) this.outputGainNode.gain.setTargetAtTime(outputLinear, this.ctx.currentTime, 0.02);

    // Bypass state
    if (this.wetGainNode) this.wetGainNode.gain.setTargetAtTime(state.engaged ? 1.0 : 0.0, this.ctx.currentTime, 0.02);
    if (this.bypassGainNode) this.bypassGainNode.gain.setTargetAtTime(state.engaged ? 0.0 : 1.0, this.ctx.currentTime, 0.02);

    // Circuit specific params
    if (state.circuit === 'EMBER') {
      // Tight filter (highpass 60Hz to 400Hz)
      const tightFreq = 60 + (state.tight / 10) * 340;
      if (this.tightFilterNode) this.tightFilterNode.frequency.value = tightFreq;

      // Tone (lowpass 800Hz to 12000Hz)
      const toneFreq = 800 + (state.tone / 10) * 11200;
      if (this.toneFilterNode) this.toneFilterNode.frequency.value = toneFreq;

      // Distortion curve
      const driveAmount = state.drive;
      this.setDistortionCurve(driveAmount, state.mode, state.oversampling);

      // Disable EQ bypasses for 3-band
      if (this.bassFilterNode) this.bassFilterNode.gain.value = 0;
      if (this.midFilterNode) this.midFilterNode.gain.value = 0;
      if (this.trebleFilterNode) this.trebleFilterNode.gain.value = 0;
      if (this.presenceFilterNode) this.presenceFilterNode.gain.value = 0;
    } else if (state.circuit === 'BURNER') {
      // Burner 3-band EQ + Aggression + Gate
      const gainAmount = state.burnerGain;
      this.setDistortionCurve(gainAmount, state.aggression === 'RED' ? 'HOT' : 'CLASSIC', state.oversampling);

      if (this.bassFilterNode) this.bassFilterNode.gain.value = (state.bass - 5) * 2.5; // -12.5dB to +12.5dB
      if (this.midFilterNode) this.midFilterNode.gain.value = (state.mid - 5) * 2.5;
      if (this.trebleFilterNode) this.trebleFilterNode.gain.value = (state.treble - 5) * 2.5;

      if (this.toneFilterNode) this.toneFilterNode.frequency.value = 16000; // Open tone
      if (this.presenceFilterNode) this.presenceFilterNode.gain.value = 0;
      
      const gateCutoff = 60 + (state.tight / 10) * 180 + (state.gate / 10) * 160;
      if (this.tightFilterNode) this.tightFilterNode.frequency.value = gateCutoff;
    } else if (state.circuit === 'BLAZER') {
      // Blazer 2-band + Presence + Tight + Trim + Gate
      const gainAmount = state.blazerGain;
      this.setDistortionCurve(gainAmount, 'HOT', state.oversampling);

      const tightFreq = 60 + (state.blazerTight / 10) * 300 + (state.gate / 10) * 100;
      if (this.tightFilterNode) this.tightFilterNode.frequency.value = tightFreq;

      if (this.bassFilterNode) this.bassFilterNode.gain.value = (state.blazerBass - 5) * 2.5;
      if (this.midFilterNode) this.midFilterNode.gain.value = 0;
      if (this.trebleFilterNode) this.trebleFilterNode.gain.value = (state.blazerTreble - 5) * 2.5;
      if (this.presenceFilterNode) this.presenceFilterNode.gain.value = (state.presence - 5) * 2.5;

      if (this.toneFilterNode) this.toneFilterNode.frequency.value = 16000;
    }
  }

  private setDistortionCurve(amount: number, mode: string, oversampling: OversamplingOption = '4x') {
    if (!this.waveShaperNode || !this.ctx) return;
    const n_samples = 44100;
    const curve = new Float32Array(n_samples);
    const deg = Math.PI / 180;
    const k = amount * 8 + (mode === 'HOT' ? 25 : mode === 'WARM' ? 5 : 12);

    for (let i = 0; i < n_samples; ++i) {
      const x = (i * 2) / n_samples - 1;
      // Soft-clipping arc curve
      curve[i] = ((3 + k) * x * 20 * deg) / (Math.PI + k * Math.abs(x));
    }
    this.waveShaperNode.curve = curve;
    const oversampleMap: Record<OversamplingOption, OverSampleType> = {
      '1x': 'none',
      '2x': '2x',
      '4x': '4x',
      '8x': '4x',
      '16x': '4x',
    };
    this.waveShaperNode.oversample = oversampleMap[oversampling] || '4x';
  }

  public async toggleDemoAudio(): Promise<boolean> {
    this.init();
    if (!this.ctx || !this.inputGainNode) return false;

    if (this.ctx.state === 'suspended') {
      await this.ctx.resume();
    }

    if (this.isPlayingDemo) {
      if (this.sourceNode) {
        try {
          this.sourceNode.stop();
          this.sourceNode.disconnect();
        } catch {
          // ignore
        }
      }
      this.isPlayingDemo = false;
      return false;
    }

    // Synthesize a heavy rhythmic guitar riff using rich harmonic oscillators
    const osc1 = this.ctx.createOscillator();
    const osc2 = this.ctx.createOscillator();
    const sub = this.ctx.createOscillator();

    osc1.type = 'sawtooth';
    osc2.type = 'square';
    sub.type = 'triangle';

    // Drop D riff pattern notes
    const now = this.ctx.currentTime;
    const baseFreq = 73.42; // D2 note
    osc1.frequency.setValueAtTime(baseFreq, now);
    osc2.frequency.setValueAtTime(baseFreq * 1.005, now);
    sub.frequency.setValueAtTime(baseFreq / 2, now);

    // Filter to simulate guitar pickup resonance
    const pickupFilter = this.ctx.createBiquadFilter();
    pickupFilter.type = 'bandpass';
    pickupFilter.frequency.value = 1400;
    pickupFilter.Q.value = 2.5;

    const sourceGain = this.ctx.createGain();
    sourceGain.gain.setValueAtTime(0.4, now);

    osc1.connect(pickupFilter);
    osc2.connect(pickupFilter);
    sub.connect(sourceGain);
    pickupFilter.connect(sourceGain);

    sourceGain.connect(this.inputGainNode);

    osc1.start(now);
    osc2.start(now);
    sub.start(now);

    this.sourceNode = osc1; // reference
    this.isPlayingDemo = true;
    return true;
  }

  public getMeterPeak(type: 'input' | 'output'): number {
    const analyser = type === 'input' ? this.inputAnalyser : this.outputAnalyser;
    if (!analyser) return 0;

    analyser.getByteFrequencyData(this.meterDataArray);
    let sum = 0;
    for (let i = 0; i < this.meterDataArray.length; i++) {
      sum += this.meterDataArray[i];
    }
    const avg = sum / this.meterDataArray.length;
    return avg / 255;
  }

  public isDemoPlaying(): boolean {
    return this.isPlayingDemo;
  }
}

export const audioEngine = new SparkerAudioEngine();
