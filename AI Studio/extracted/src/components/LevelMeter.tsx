import React from 'react';

interface LevelMeterProps {
  levelDb: number; // e.g. -12 to +12 or 0 to 1 RMS level
  color: string;
  side: 'left' | 'right';
  audioMeterValue?: number; // 0.0 to 1.0 real-time peak signal
}

export const LevelMeter: React.FC<LevelMeterProps> = ({
  levelDb,
  color,
  side,
  audioMeterValue = 0,
}) => {
  // Convert levelDb (-12 to +12) + audio peak (0..1) to normalized 0..1 scale
  const gainLinear = Math.pow(10, levelDb / 20);
  const totalLevel = Math.min(1, Math.max(0, (audioMeterValue || 0.3) * gainLinear));

  // 6 tick bars vertically stacked
  const tickCount = 6;

  return (
    <div
      className={`flex flex-col justify-between h-[70px] py-1 opacity-80 ${
        side === 'right' ? 'ml-2' : 'mr-2'
      }`}
    >
      {[...Array(tickCount)].map((_, idx) => {
        // Ticks go from top (peak) to bottom (low)
        const threshold = (tickCount - idx) / tickCount;
        const isActive = totalLevel >= threshold - 0.12;

        return (
          <div
            key={idx}
            className="w-2.5 h-[1.5px] rounded-full transition-all duration-100"
            style={{
              backgroundColor: isActive ? color : '#272235',
              boxShadow: isActive ? `0 0 6px ${color}` : 'none',
              opacity: isActive ? 1 : 0.4,
            }}
          />
        );
      })}
    </div>
  );
};
