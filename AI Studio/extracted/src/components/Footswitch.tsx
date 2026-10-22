import React, { useState } from 'react';

interface FootswitchProps {
  engaged: boolean;
  onToggle: () => void;
  ledColor: string;
  ledShadow: string;
  circuitColor: string;
}

export const Footswitch: React.FC<FootswitchProps> = ({
  engaged,
  onToggle,
  ledColor,
  ledShadow,
  circuitColor,
}) => {
  const [isPressed, setIsPressed] = useState(false);

  return (
    <div className="flex flex-col items-center justify-center select-none py-1">
      {/* LED Indicator Light */}
      <div className="relative mb-3 flex items-center justify-center">
        <div
          className="w-4 h-4 rounded-full transition-all duration-300"
          style={{
            backgroundColor: engaged ? ledColor : '#2a2638',
            boxShadow: engaged ? ledShadow : 'none',
            border: `1px solid ${engaged ? ledColor : '#4a4360'}`,
          }}
        />
      </div>

      {/* Dark Radial Footswitch Button */}
      <button
        type="button"
        onClick={onToggle}
        onMouseDown={() => setIsPressed(true)}
        onMouseUp={() => setIsPressed(false)}
        onMouseLeave={() => setIsPressed(false)}
        className="group relative focus:outline-none transition-transform duration-100 flex items-center justify-center rounded-full p-0 cursor-pointer"
        style={{
          transform: isPressed ? 'scale(0.97) translateY(1px)' : 'scale(1)',
        }}
        title={engaged ? 'Click to Bypass (Turn OFF)' : 'Click to Engage (Turn ON)'}
      >
        {/* Single Outer Border Ring */}
        <div
          className="w-[72px] h-[72px] rounded-full flex items-center justify-center p-[4px] transition-all duration-300"
          style={{
            backgroundColor: '#0a0714',
            border: `1.5px solid ${engaged ? `${circuitColor}77` : '#221936'}`,
            boxShadow: engaged
              ? `0 0 16px ${circuitColor}25, inset 0 0 8px rgba(0,0,0,0.8)`
              : 'inset 0 0 8px rgba(0,0,0,0.8)',
          }}
        >
          {/* Smooth Dark Radial Dome */}
          <div
            className="w-full h-full rounded-full transition-all duration-150"
            style={{
              background: isPressed
                ? 'radial-gradient(circle at 45% 45%, #231b36 0%, #130e1e 70%, #09060f 100%)'
                : 'radial-gradient(circle at 38% 32%, #433561 0%, #211933 55%, #0e0a17 100%)',
              border: `1px solid ${engaged ? `${circuitColor}aa` : '#312648'}`,
              boxShadow: isPressed
                ? 'inset 0 2px 6px rgba(0,0,0,0.9)'
                : 'inset 0 1px 2px rgba(255,255,255,0.18), 0 3px 8px rgba(0,0,0,0.6)',
            }}
          />
        </div>
      </button>

      {/* Logo Typography */}
      <div className="mt-3 text-center select-none flex flex-col items-center">
        <h1
          className="text-2xl font-black tracking-widest text-white uppercase font-sans drop-shadow-md"
          style={{
            letterSpacing: '0.22em',
            fontFamily: 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif',
          }}
        >
          SPARKER
        </h1>
        <span
          className="text-xs font-bold tracking-[0.3em] uppercase mt-0.5 transition-colors duration-300"
          style={{ color: circuitColor }}
        >
          DISTORTION
        </span>
      </div>
    </div>
  );
};

