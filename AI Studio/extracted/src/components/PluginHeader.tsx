import React, { useState, useRef, useEffect } from 'react';
import { ChevronLeft, ChevronRight } from 'lucide-react';
import { PluginState, Preset, CircuitType } from '../types';
import { PRESETS, getCircuitTheme } from '../constants';

interface PluginHeaderProps {
  state: PluginState;
  onSelectPreset: (preset: Preset) => void;
  onChangeCircuit: (circuit: CircuitType) => void;
}

export const PluginHeader: React.FC<PluginHeaderProps> = ({
  state,
  onSelectPreset,
  onChangeCircuit,
}) => {
  const [isDropdownOpen, setIsDropdownOpen] = useState(false);
  const dropdownRef = useRef<HTMLDivElement>(null);
  const theme = getCircuitTheme(state);

  // Find currently active preset if state matches
  const currentPresetIndex = PRESETS.findIndex((p) => {
    return p.state.circuit === state.circuit;
  });

  const activePreset = PRESETS[currentPresetIndex] || PRESETS[0];

  // Close dropdown on outside click
  useEffect(() => {
    const handleClickOutside = (e: MouseEvent) => {
      if (dropdownRef.current && !dropdownRef.current.contains(e.target as Node)) {
        setIsDropdownOpen(false);
      }
    };
    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, []);

  const handlePrevPreset = () => {
    const prevIdx = (currentPresetIndex - 1 + PRESETS.length) % PRESETS.length;
    onSelectPreset(PRESETS[prevIdx]);
  };

  const handleNextPreset = () => {
    const nextIdx = (currentPresetIndex + 1) % PRESETS.length;
    onSelectPreset(PRESETS[nextIdx]);
  };

  const handleToggleCircuit = () => {
    const circuits: CircuitType[] = ['EMBER', 'BURNER', 'BLAZER'];
    const currentIndex = circuits.indexOf(state.circuit);
    const nextCircuit = circuits[(currentIndex + 1) % circuits.length];
    onChangeCircuit(nextCircuit);
  };

  return (
    <div className="w-full bg-[#08060d]/90 backdrop-blur-md border-b border-[#1f1730] px-4 py-2.5 grid grid-cols-3 items-center relative z-30 select-none">
      {/* Left: Branding */}
      <div className="flex items-center gap-3 justify-center pl-4 sm:pl-8">
        <div className="flex items-center gap-2 font-black tracking-wider text-slate-100 text-sm">
          <span className="bg-gradient-to-r from-slate-100 via-slate-300 to-slate-400 bg-clip-text text-transparent">
            SPARKER
          </span>
          <span className="text-[10px] font-mono text-slate-500 font-normal">v1.0</span>
        </div>
      </div>

      {/* Center: Integrated VST Digital LCD Preset Display */}
      <div className="w-full max-w-sm mx-auto relative" ref={dropdownRef}>
        <div className="flex items-center justify-between bg-[#040208] border border-[#221838] rounded-xl p-1 shadow-inner gap-1">
          {/* Stepper Prev Button */}
          <button
            type="button"
            onClick={handlePrevPreset}
            title="Previous Preset"
            className="p-1.5 text-slate-400 hover:text-slate-100 hover:bg-[#181126] rounded-lg transition-all"
          >
            <ChevronLeft className="w-4 h-4" />
          </button>

          {/* Central Screen Readout */}
          <button
            type="button"
            onClick={() => setIsDropdownOpen(!isDropdownOpen)}
            className="flex-1 flex flex-col items-center justify-center px-3 py-1.5 hover:bg-[#0f0a1c] rounded-lg transition-colors group cursor-pointer text-center"
          >
            <div
              className="text-xs font-bold tracking-wide truncate max-w-[200px] sm:max-w-[260px] transition-colors"
              style={{ color: theme.text }}
            >
              {activePreset.name}
            </div>
          </button>

          {/* Stepper Next Button */}
          <button
            type="button"
            onClick={handleNextPreset}
            title="Next Preset"
            className="p-1.5 text-slate-400 hover:text-slate-100 hover:bg-[#181126] rounded-lg transition-all"
          >
            <ChevronRight className="w-4 h-4" />
          </button>
        </div>

        {/* Preset Dropdown Browser Modal */}
        {isDropdownOpen && (
          <div className="absolute top-full left-0 right-0 mt-2 bg-[#0c0817] border border-[#2d2047] rounded-xl shadow-2xl p-2 z-50 animate-in fade-in zoom-in-95 duration-150">
            <div className="max-h-60 overflow-y-auto space-y-1 pr-1 custom-scrollbar">
              {PRESETS.map((preset) => {
                const isSelected = activePreset.id === preset.id;
                return (
                  <button
                    key={preset.id}
                    type="button"
                    onClick={() => {
                      onSelectPreset(preset);
                      setIsDropdownOpen(false);
                    }}
                    className={`w-full text-left px-3 py-2 rounded-lg text-xs font-semibold transition-all ${
                      isSelected
                        ? 'bg-[#1e1436] border border-purple-500/40 text-purple-200'
                        : 'hover:bg-[#150d26] text-slate-300'
                    }`}
                  >
                    {preset.name}
                  </button>
                );
              })}
            </div>
          </div>
        )}
      </div>

      {/* Right: Clickable Circuit Badge Toggle */}
      <div className="flex items-center justify-center sm:pr-4">
        <button
          type="button"
          onClick={handleToggleCircuit}
          title="Click to toggle circuit mode (EMBER → BURNER → BLAZER)"
          className="px-3.5 py-1.5 rounded-xl text-xs font-black tracking-widest uppercase border transition-all cursor-pointer hover:scale-105 active:scale-95 shadow-md flex items-center justify-center"
          style={{
            color: theme.accent,
            backgroundColor: `${theme.primary}22`,
            borderColor: `${theme.primary}60`,
            boxShadow: `0 0 14px ${theme.primaryGlow}`,
          }}
        >
          <span>{state.circuit}</span>
        </button>
      </div>
    </div>
  );
};
