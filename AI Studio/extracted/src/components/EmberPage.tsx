import React from 'react';
import { PluginState, CircuitType, EmberMode, OversamplingOption } from '../types';
import { CIRCUIT_THEMES } from '../constants';
import { Knob } from './Knob';
import { LevelMeter } from './LevelMeter';
import { Footswitch } from './Footswitch';

interface EmberPageProps {
  state: PluginState;
  onChangeParam: <K extends keyof PluginState>(key: K, value: PluginState[K]) => void;
  audioInputPeak?: number;
  audioOutputPeak?: number;
}

export const EmberPage: React.FC<EmberPageProps> = ({
  state,
  onChangeParam,
  audioInputPeak = 0,
  audioOutputPeak = 0,
}) => {
  const theme = CIRCUIT_THEMES.EMBER;

  return (
    <div className="w-full h-full flex flex-col justify-between p-4 sm:p-6 relative select-none">
      {/* Top Row: INPUT, DRIVE, KHYLE AUDIO, LEVEL, OUTPUT */}
      <div className="grid grid-cols-5 items-center justify-between gap-2">
        {/* INPUT with meter ticks right */}
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
            value={state.drive}
            min={0}
            max={10}
            defaultValue={6.5}
            color={theme.primary}
            glowColor={theme.primaryGlow}
            onChange={(val) => onChangeParam('drive', val)}
          />
        </div>

        {/* KHYLE AUDIO Center Logo */}
        <div className="flex justify-center items-center">
          <span className="text-sm font-black tracking-[0.25em] text-slate-300/80 uppercase font-sans">
            KHYLE AUDIO
          </span>
        </div>

        {/* LEVEL */}
        <div className="flex justify-center">
          <Knob
            label="LEVEL"
            value={state.level}
            min={0}
            max={10}
            defaultValue={7.0}
            color={theme.primary}
            glowColor={theme.primaryGlow}
            onChange={(val) => onChangeParam('level', val)}
          />
        </div>

        {/* OUTPUT with meter ticks left */}
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

      {/* Middle Row: TONE Center Knob */}
      <div className="flex justify-center my-3">
        <Knob
          label="TONE"
          value={state.tone}
          min={0}
          max={10}
          defaultValue={5.0}
          color={theme.primary}
          glowColor={theme.primaryGlow}
          onChange={(val) => onChangeParam('tone', val)}
        />
      </div>

      {/* Bottom Area: Controls Left, Footswitch Center, Controls Right */}
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
              value={state.tight}
              min={0}
              max={10}
              defaultValue={3.0}
              size="small"
              color={theme.primary}
              glowColor={theme.primaryGlow}
              onChange={(val) => onChangeParam('tight', val)}
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

        {/* Bottom Right: MODE Selector */}
        <div className="flex flex-col items-center justify-end pb-1 pr-2">
          <div className="flex flex-col items-center w-[220px]">
            <div className="w-full grid grid-cols-3 bg-[#130f1e] p-1 rounded-xl border border-[#2b223e]/60 shadow-inner gap-1">
              {(['WARM', 'CLASSIC', 'HOT'] as EmberMode[]).map((m) => {
                const isActive = state.mode === m;
                return (
                  <button
                    key={m}
                    type="button"
                    onClick={() => onChangeParam('mode', m)}
                    className="w-full py-1.5 text-xs font-bold tracking-wider text-center rounded-lg transition-all"
                    style={{
                      color: isActive ? theme.accent : '#64748b',
                      backgroundColor: isActive ? 'rgba(217, 70, 239, 0.2)' : 'transparent',
                      border: isActive ? `1.5px solid ${theme.primary}` : '1.5px solid transparent',
                      boxShadow: isActive ? `0 0 10px ${theme.primaryGlow}` : 'none',
                    }}
                  >
                    {m}
                  </button>
                );
              })}
            </div>
            <span
              className="text-[10px] font-bold tracking-widest uppercase mt-1 text-center w-full block"
              style={{ color: theme.accent }}
            >
              MODE
            </span>
          </div>
        </div>
      </div>
    </div>
  );
};
