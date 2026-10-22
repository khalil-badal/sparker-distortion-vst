import React from 'react';
import { PluginState, CircuitType, OversamplingOption } from '../types';
import { CIRCUIT_THEMES } from '../constants';
import { Knob } from './Knob';
import { LevelMeter } from './LevelMeter';
import { Footswitch } from './Footswitch';

interface BlazerPageProps {
  state: PluginState;
  onChangeParam: <K extends keyof PluginState>(key: K, value: PluginState[K]) => void;
  audioInputPeak?: number;
  audioOutputPeak?: number;
}

export const BlazerPage: React.FC<BlazerPageProps> = ({
  state,
  onChangeParam,
  audioInputPeak = 0,
  audioOutputPeak = 0,
}) => {
  const theme = CIRCUIT_THEMES.BLAZER;

  return (
    <div className="w-full h-full flex flex-col justify-between p-4 sm:p-6 relative select-none">
      {/* Top Row: INPUT, GAIN, KHYLE AUDIO, VOLUME, OUTPUT */}
      <div className="grid grid-cols-5 items-center justify-between gap-2">
        {/* INPUT */}
        <div className="flex items-center justify-center">
          <Knob
            label="INPUT"
            value={state.inputDb}
            min={-12}
            max={12}
            step={0.1}
            defaultValue={0.0}
            unit=" dB"
            displayValue={`${state.inputDb > 0 ? '+' : ''}${state.inputDb.toFixed(1)} dB`}
            color={theme.primary}
            glowColor={theme.primaryGlow}
            onChange={(val) => onChangeParam('inputDb', val)}
          />
          <LevelMeter levelDb={state.inputDb} color={theme.primary} side="right" audioMeterValue={audioInputPeak} />
        </div>

        {/* GAIN */}
        <div className="flex justify-center">
          <Knob
            label="GAIN"
            value={state.blazerGain}
            min={0}
            max={10}
            defaultValue={8.0}
            color={theme.primary}
            glowColor={theme.primaryGlow}
            onChange={(val) => onChangeParam('blazerGain', val)}
          />
        </div>

        {/* KHYLE AUDIO Logo */}
        <div className="flex justify-center items-center">
          <span className="text-sm font-black tracking-[0.25em] text-slate-300/80 uppercase font-sans">
            KHYLE AUDIO
          </span>
        </div>

        {/* VOLUME */}
        <div className="flex justify-center">
          <Knob
            label="VOLUME"
            value={state.blazerVolume}
            min={0}
            max={10}
            defaultValue={6.8}
            color={theme.primary}
            glowColor={theme.primaryGlow}
            onChange={(val) => onChangeParam('blazerVolume', val)}
          />
        </div>

        {/* OUTPUT */}
        <div className="flex items-center justify-center">
          <LevelMeter levelDb={state.outputDb} color={theme.primary} side="left" audioMeterValue={audioOutputPeak} />
          <Knob
            label="OUTPUT"
            value={state.outputDb}
            min={-12}
            max={12}
            step={0.1}
            defaultValue={0.0}
            unit=" dB"
            displayValue={`${state.outputDb > 0 ? '+' : ''}${state.outputDb.toFixed(1)} dB`}
            color={theme.primary}
            glowColor={theme.primaryGlow}
            onChange={(val) => onChangeParam('outputDb', val)}
          />
        </div>
      </div>

      {/* Middle Row: BASS, TREBLE, PRES */}
      <div className="flex items-center justify-center gap-12 my-3">
        <Knob
          label="BASS"
          value={state.blazerBass}
          min={0}
          max={10}
          defaultValue={5.5}
          color={theme.primary}
          glowColor={theme.primaryGlow}
          onChange={(val) => onChangeParam('blazerBass', val)}
        />
        <Knob
          label="TREBLE"
          value={state.blazerTreble}
          min={0}
          max={10}
          defaultValue={6.0}
          color={theme.primary}
          glowColor={theme.primaryGlow}
          onChange={(val) => onChangeParam('blazerTreble', val)}
        />
        <Knob
          label="PRES"
          value={state.presence}
          min={0}
          max={10}
          defaultValue={5.0}
          color={theme.primary}
          glowColor={theme.primaryGlow}
          onChange={(val) => onChangeParam('presence', val)}
        />
      </div>

      {/* Bottom Area: Controls Left, Footswitch Center, CIRCUIT Right */}
      <div className="grid grid-cols-3 items-end justify-between pt-2">
        {/* Bottom Left: GATE & TIGHT knobs with generous spacing */}
        <div className="flex flex-col items-start gap-4 pl-4 pb-1">
          <div className="flex items-center gap-14">
            <Knob
              label="GATE"
              value={state.gate}
              min={0}
              max={10}
              defaultValue={2.0}
              size="small"
              color={theme.primary}
              glowColor={theme.primaryGlow}
              onChange={(val) => onChangeParam('gate', val)}
            />
            <Knob
              label="TIGHT"
              value={state.blazerTight}
              min={0}
              max={10}
              defaultValue={4.0}
              size="small"
              color={theme.primary}
              glowColor={theme.primaryGlow}
              onChange={(val) => onChangeParam('blazerTight', val)}
            />
          </div>
        </div>

        {/* Bottom Center: Footswitch & Main Logo */}
        <div className="flex justify-center">
          <Footswitch
            engaged={state.engaged}
            onToggle={() => onChangeParam('engaged', !state.engaged)}
            ledColor={theme.ledColor}
            ledShadow={theme.ledShadow}
            circuitColor={theme.text}
          />
        </div>

        {/* Bottom Right: TRIM Knob */}
        <div className="flex flex-col items-center justify-end pb-1 pr-4">
          <Knob
            label="TRIM"
            value={state.trim}
            min={0}
            max={10}
            defaultValue={3.5}
            size="small"
            color={theme.primary}
            glowColor={theme.primaryGlow}
            onChange={(val) => onChangeParam('trim', val)}
          />
        </div>
      </div>
    </div>
  );
};
