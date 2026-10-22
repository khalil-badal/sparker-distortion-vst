import React, { useState, useEffect, useCallback } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { PluginState, Preset } from './types';
import { DEFAULT_PLUGIN_STATE, getCircuitTheme } from './constants';
import { audioEngine } from './utils/audioEngine';
import { EmberPage } from './components/EmberPage';
import { BurnerPage } from './components/BurnerPage';
import { BlazerPage } from './components/BlazerPage';
import { PluginHeader } from './components/PluginHeader';

export default function App() {
  const [state, setState] = useState<PluginState>(DEFAULT_PLUGIN_STATE);
  const [isPlayingDemo, setIsPlayingDemo] = useState(false);
  const [audioInputPeak, setAudioInputPeak] = useState(0);
  const [audioOutputPeak, setAudioOutputPeak] = useState(0);

  const handleParamChange = useCallback(
    <K extends keyof PluginState>(key: K, value: PluginState[K]) => {
      setState((prev) => {
        const next = { ...prev, [key]: value };
        audioEngine.updateParams(next);
        return next;
      });
    },
    []
  );

  const handleToggleDemo = async () => {
    const isNowPlaying = await audioEngine.toggleDemoAudio();
    setIsPlayingDemo(isNowPlaying);
    audioEngine.updateParams(state);
  };

  const handleSelectPreset = (preset: Preset) => {
    setState((prev) => {
      const next = { ...prev, ...preset.state };
      audioEngine.updateParams(next);
      return next;
    });
  };

  const handleResetState = () => {
    setState(DEFAULT_PLUGIN_STATE);
    audioEngine.updateParams(DEFAULT_PLUGIN_STATE);
  };

  // Keyboard shortcut: Spacebar to toggle footswitch/bypass
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.code === 'Space' && e.target === document.body) {
        e.preventDefault();
        handleParamChange('engaged', !state.engaged);
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [state.engaged, handleParamChange]);

  // Real-time audio meter animation loop
  useEffect(() => {
    let animId: number;
    const loop = () => {
      if (isPlayingDemo) {
        setAudioInputPeak(audioEngine.getMeterPeak('input'));
        setAudioOutputPeak(audioEngine.getMeterPeak('output'));
      } else {
        setAudioInputPeak(0);
        setAudioOutputPeak(0);
      }
      animId = requestAnimationFrame(loop);
    };
    loop();
    return () => cancelAnimationFrame(animId);
  }, [isPlayingDemo]);

  const currentTheme = getCircuitTheme(state);

  return (
    <div className="min-h-screen bg-[#07050a] text-slate-100 flex flex-col items-center justify-between font-sans selection:bg-purple-900 selection:text-white">
      {/* Main VST Plugin Chassis */}
      <main className="flex-1 w-full flex items-center justify-center p-4 sm:p-6 md:p-8">
        <div
          className="w-full max-w-4xl aspect-[4/3] min-h-[580px] rounded-2xl relative overflow-hidden shadow-2xl transition-all duration-500 border flex flex-col"
          style={{
            background:
              state.circuit === 'EMBER'
                ? 'radial-gradient(ellipse at center, #260c33 0%, #170620 60%, #0c0212 100%)'
                : state.circuit === 'BURNER'
                ? state.aggression === 'RED'
                  ? 'radial-gradient(ellipse at 50% 35%, #440c26 0%, #290833 45%, #150522 75%, #080212 100%)'
                  : 'radial-gradient(ellipse at 50% 35%, #0d2c4e 0%, #1f0b38 45%, #130522 75%, #080212 100%)'
                : 'radial-gradient(ellipse at center, #20160b 0%, #140d06 60%, #0a0603 100%)',
            borderColor: currentTheme.border,
            boxShadow:
              state.circuit === 'BURNER'
                ? state.aggression === 'RED'
                  ? '0 25px 60px -15px rgba(0,0,0,0.9), 0 0 35px rgba(244, 63, 94, 0.25), 0 0 35px rgba(192, 132, 252, 0.25)'
                  : '0 25px 60px -15px rgba(0,0,0,0.9), 0 0 35px rgba(56, 189, 248, 0.25), 0 0 35px rgba(192, 132, 252, 0.25)'
                : `0 25px 60px -15px rgba(0,0,0,0.9), 0 0 40px ${currentTheme.primaryGlow}`,
          }}
        >
          {/* Integrated VST Top Header & Preset Loader */}
          <PluginHeader
            state={state}
            onSelectPreset={handleSelectPreset}
            onChangeCircuit={(circ) => handleParamChange('circuit', circ)}
          />

          {/* Render Current Page with smooth fade transition */}
          <AnimatePresence mode="wait">
            <motion.div
              key={state.circuit}
              initial={{ opacity: 0, scale: 0.98 }}
              animate={{ opacity: 1, scale: 1 }}
              exit={{ opacity: 0, scale: 1.02 }}
              transition={{ duration: 0.2 }}
              className="w-full h-full flex-1 flex flex-col"
            >
              {state.circuit === 'EMBER' && (
                <EmberPage
                  state={state}
                  onChangeParam={handleParamChange}
                  audioInputPeak={audioInputPeak}
                  audioOutputPeak={audioOutputPeak}
                />
              )}
              {state.circuit === 'BURNER' && (
                <BurnerPage
                  state={state}
                  onChangeParam={handleParamChange}
                  audioInputPeak={audioInputPeak}
                  audioOutputPeak={audioOutputPeak}
                />
              )}
              {state.circuit === 'BLAZER' && (
                <BlazerPage
                  state={state}
                  onChangeParam={handleParamChange}
                  audioInputPeak={audioInputPeak}
                  audioOutputPeak={audioOutputPeak}
                />
              )}
            </motion.div>
          </AnimatePresence>
        </div>
      </main>


    </div>
  );
}
