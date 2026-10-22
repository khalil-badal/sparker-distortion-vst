import React from 'react';
import { PluginState, CircuitType, AggressionMode } from '../types';
import { getCircuitTheme } from '../constants';
import { Knob } from './Knob';
import { LevelMeter } from './LevelMeter';
import { Footswitch } from './Footswitch';

interface BurnerPageProps {
  state: PluginState;
  onChangeParam: <K extends keyof PluginState>(key: K, value: PluginState[K]) => void;
  audioInputPeak?: number;
  audioOutputPeak?: number;
}

export const BurnerPage: React.FC<BurnerPageProps> = ({
  state,
  onChangeParam,
  audioInputPeak = 0,
  audioOutputPeak = 0,
}) => {
  const theme = getCircuitTheme(state);

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
            value={state.burnerGain}
            min={0}
            max={10}
            defaultValue={7.5}
            color={theme.primary}
            glowColor={theme.primaryGlow}
            onChange={(val) => onChangeParam('burnerGain', val)}
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
            value={state.burnerVolume}
            min={0}
            max={10}
            defaultValue={7.0}
            color={theme.primary}
            glowColor={theme.primaryGlow}
            onChange={(val) => onChangeParam('burnerVolume', val)}
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

      {/* Middle Row: BASS, MID, TREBLE */}
      <div className="flex items-center justify-center gap-12 my-3">
        <Knob
          label="BASS"
          value={state.bass}
          min={0}
          max={10}
          defaultValue={6.0}
          color={theme.primary}
          glowColor={theme.primaryGlow}
          onChange={(val) => onChangeParam('bass', val)}
        />
        <Knob
          label="MID"
          value={state.mid}
          min={0}
          max={10}
          defaultValue={5.0}
          color={theme.primary}
          glowColor={theme.primaryGlow}
          onChange={(val) => onChangeParam('mid', val)}
        />
        <Knob
          label="TREBLE"
          value={state.treble}
          min={0}
          max={10}
          defaultValue={6.5}
          color={theme.primary}
          glowColor={theme.primaryGlow}
          onChange={(val) => onChangeParam('treble', val)}
        />
      </div>

      {/* Bottom Row: GATE & TIGHT Left, Footswitch Center, AGGRESSION & OVERSAMPLING Right */}
      <div className="grid grid-cols-3 items-end justify-between pt-2">
        {/* Bottom Left: GATE & TIGHT Knobs with generous spacing */}
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

        {/* Bottom Right: AGGRESSION Selector */}
        <div className="flex flex-col items-center justify-end pb-1 pr-2">
          <div className="flex flex-col items-center w-[180px]">
            <div className="w-full grid grid-cols-2 bg-[#0d172a] p-1 rounded-xl border border-[#1e293b]/80 shadow-inner gap-1">
              {(['BLUE', 'RED'] as AggressionMode[]).map((aggr) => {
                const isActive = state.aggression === aggr;
                const buttonColor = aggr === 'BLUE' ? '#38bdf8' : '#f43f5e';
                return (
                  <button
                    key={aggr}
                    type="button"
                    onClick={() => onChangeParam('aggression', aggr)}
                    className="w-full py-1.5 text-xs font-bold tracking-wider text-center rounded-lg transition-all"
                    style={{
                      color: isActive ? buttonColor : '#64748b',
                      backgroundColor: isActive ? `${buttonColor}20` : 'transparent',
                      border: isActive ? `1.5px solid ${buttonColor}` : '1.5px solid transparent',
                      boxShadow: isActive ? `0 0 10px ${buttonColor}80` : 'none',
                    }}
                  >
                    {aggr}
                  </button>
                );
              })}
            </div>
            <span
              className="text-[10px] font-bold tracking-widest uppercase mt-1 text-center w-full block transition-colors"
              style={{ color: theme.accent }}
            >
              AGGRESSION
            </span>
          </div>
        </div>
      </div>
    </div>
  );
};
