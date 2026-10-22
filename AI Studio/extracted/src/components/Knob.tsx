import React, { useState, useRef, useEffect, useCallback } from 'react';

interface KnobProps {
  label: string;
  value: number; // 0 to 10 by default, or min..max
  min?: number;
  max?: number;
  step?: number;
  defaultValue?: number;
  unit?: string;
  displayValue?: string;
  size?: 'large' | 'small';
  color?: string; // Theme accent color (e.g. #a855f7)
  glowColor?: string;
  onChange: (value: number) => void;
  showTicksRight?: boolean;
  showTicksLeft?: boolean;
}

export const Knob: React.FC<KnobProps> = ({
  label,
  value,
  min = 0,
  max = 10,
  step = 0.1,
  defaultValue,
  unit = '',
  displayValue,
  size = 'large',
  color = '#a855f7',
  glowColor = 'rgba(168, 85, 247, 0.5)',
  onChange,
  showTicksRight = false,
  showTicksLeft = false,
}) => {
  const [isDragging, setIsDragging] = useState(false);
  const [isHovered, setIsHovered] = useState(false);
  const startYRef = useRef<number>(0);
  const startValueRef = useRef<number>(value);
  const knobRef = useRef<HTMLDivElement>(null);

  const normalizedValue = Math.max(0, Math.min(1, (value - min) / (max - min)));
  
  // Angle calculations: 270 degrees total sweep (-135deg to +135deg)
  const MIN_ANGLE = -135;
  const MAX_ANGLE = 135;
  const angle = MIN_ANGLE + normalizedValue * (MAX_ANGLE - MIN_ANGLE);

  const isSmall = size === 'small';
  const diameter = isSmall ? 56 : 84;
  const innerRadius = isSmall ? 22 : 34;

  const lastClickTimeRef = useRef<number>(0);

  const handlePointerDown = (e: React.PointerEvent) => {
    const now = Date.now();
    if (now - lastClickTimeRef.current < 300) {
      // Fast double-click / double-tap: reset to 12 o'clock (exact midpoint)
      const twelveOClock = (min + max) / 2;
      const stepped = Math.round(twelveOClock / step) * step;
      onChange(Number(stepped.toFixed(2)));
      lastClickTimeRef.current = 0;
      return;
    }
    lastClickTimeRef.current = now;

    e.preventDefault();
    setIsDragging(true);
    startYRef.current = e.clientY;
    startValueRef.current = value;
    (e.target as HTMLElement).setPointerCapture(e.pointerId);
  };

  const handlePointerMove = (e: React.PointerEvent) => {
    if (!isDragging) return;
    const deltaY = startYRef.current - e.clientY;
    // Shift key enables fine precision mode
    const speedDivider = e.shiftKey ? 600 : 200;
    const sensitivity = (max - min) / speedDivider;
    let newValue = startValueRef.current + deltaY * sensitivity;
    newValue = Math.max(min, Math.min(max, newValue));
    
    // Round to step
    const stepped = Math.round(newValue / step) * step;
    onChange(Number(stepped.toFixed(2)));
  };

  const handlePointerUp = (e: React.PointerEvent) => {
    if (isDragging) {
      setIsDragging(false);
      try {
        (e.target as HTMLElement).releasePointerCapture(e.pointerId);
      } catch {
        // ignore
      }
    }
  };

  const handleWheel = (e: React.WheelEvent) => {
    e.preventDefault();
    const delta = e.deltaY < 0 ? step * 2 : -step * 2;
    let newValue = value + delta;
    newValue = Math.max(min, Math.min(max, newValue));
    onChange(Number(newValue.toFixed(2)));
  };

  const handleDoubleClick = (e: React.MouseEvent) => {
    e.preventDefault();
    e.stopPropagation();
    const twelveOClock = (min + max) / 2;
    const stepped = Math.round(twelveOClock / step) * step;
    onChange(Number(stepped.toFixed(2)));
  };

  // SVG arc calculation for outer indicator ring
  const strokeWidth = isSmall ? 3.5 : 4.5;
  const svgRadius = diameter / 2 - strokeWidth;
  const center = diameter / 2;
  const circumference = 2 * Math.PI * svgRadius;
  
  // Sweep range is 270deg out of 360deg (0.75 ratio)
  const maxArcLength = circumference * 0.75;
  const activeArcLength = normalizedValue * maxArcLength;

  // Format readout text
  const formattedReadout = displayValue !== undefined 
    ? displayValue 
    : `${value.toFixed(1)}${unit}`;

  return (
    <div className="flex flex-col items-center select-none group relative">
      <div className="flex items-center gap-1.5">
        {/* Optional tick marks on the left */}
        {showTicksLeft && (
          <div className="flex flex-col justify-between h-[72px] py-1 opacity-70">
            {[...Array(6)].map((_, i) => (
              <div
                key={i}
                className="w-2 h-[1.5px] rounded-full transition-colors"
                style={{
                  backgroundColor: i <= (1 - normalizedValue) * 5 ? color : '#3f3f46',
                  boxShadow: i <= (1 - normalizedValue) * 5 ? `0 0 6px ${color}` : 'none',
                }}
              />
            ))}
          </div>
        )}

        {/* Rotary Knob Container */}
        <div
          ref={knobRef}
          onPointerDown={handlePointerDown}
          onPointerMove={handlePointerMove}
          onPointerUp={handlePointerUp}
          onWheel={handleWheel}
          onDoubleClick={handleDoubleClick}
          onMouseEnter={() => setIsHovered(true)}
          onMouseLeave={() => setIsHovered(false)}
          className="relative cursor-grab active:cursor-grabbing touch-none flex items-center justify-center transition-transform hover:scale-[1.02]"
          style={{ width: diameter, height: diameter }}
        >
          {/* SVG Ring Arc Track */}
          <svg
            className="absolute inset-0 w-full h-full -rotate-90 pointer-events-none"
            viewBox={`0 0 ${diameter} ${diameter}`}
          >
            {/* Background ring track starting at 7 o'clock (-135deg) */}
            <circle
              cx={center}
              cy={center}
              r={svgRadius}
              fill="none"
              stroke="#1e1828"
              strokeWidth={strokeWidth}
              strokeDasharray={`${maxArcLength} ${circumference}`}
              strokeDashoffset={-circumference * 0.625}
              strokeLinecap="round"
            />
            {/* Active colored arc track */}
            <circle
              cx={center}
              cy={center}
              r={svgRadius}
              fill="none"
              stroke={color}
              strokeWidth={strokeWidth}
              strokeDasharray={`${activeArcLength} ${circumference}`}
              strokeDashoffset={-circumference * 0.625}
              strokeLinecap="round"
              style={{
                filter: isDragging || isHovered ? `drop-shadow(0 0 6px ${color})` : 'none',
                transition: isDragging ? 'none' : 'stroke-dasharray 0.15s ease-out',
              }}
            />
          </svg>

          {/* Outer Knob Rim with shadow/bezel */}
          <div
            className="rounded-full flex items-center justify-center relative shadow-2xl transition-all"
            style={{
              width: innerRadius * 2,
              height: innerRadius * 2,
              background: 'radial-gradient(circle at 35% 35%, #2a243a 0%, #151020 60%, #0d0915 100%)',
              border: `1.5px solid ${isDragging || isHovered ? color : '#312845'}`,
              boxShadow: isDragging 
                ? `0 0 16px ${glowColor}, inset 0 2px 4px rgba(255,255,255,0.15)` 
                : 'inset 0 2px 4px rgba(255,255,255,0.1), 0 8px 16px rgba(0,0,0,0.6)',
            }}
          >
            {/* Inner cap texture rings */}
            <div
              className="w-[82%] h-[82%] rounded-full flex items-center justify-center border border-[#302644]/50"
              style={{
                background: 'radial-gradient(circle, #1a1426 0%, #0d0a14 100%)',
              }}
            >
              {/* Rotating Pointer / Indicator Dot & Needle */}
              <div
                className="w-full h-full relative rounded-full"
                style={{
                  transform: `rotate(${angle}deg)`,
                  transition: isDragging ? 'none' : 'transform 0.1s ease-out',
                }}
              >
                {/* White Indicator Dot/Line at the top rim */}
                <div
                  className="absolute left-1/2 -translate-x-1/2 top-1 rounded-full bg-white"
                  style={{
                    width: isSmall ? '2px' : '3px',
                    height: isSmall ? '8px' : '11px',
                    boxShadow: '0 0 4px rgba(255,255,255,0.8)',
                  }}
                />
              </div>
            </div>
          </div>
        </div>

        {/* Optional tick marks on the right */}
        {showTicksRight && (
          <div className="flex flex-col justify-between h-[72px] py-1 opacity-70">
            {[...Array(6)].map((_, i) => (
              <div
                key={i}
                className="w-2 h-[1.5px] rounded-full transition-colors"
                style={{
                  backgroundColor: i <= (1 - normalizedValue) * 5 ? color : '#3f3f46',
                  boxShadow: i <= (1 - normalizedValue) * 5 ? `0 0 6px ${color}` : 'none',
                }}
              />
            ))}
          </div>
        )}
      </div>

      {/* Label and dB / Value Readout */}
      <div className="mt-2 text-center flex flex-col items-center">
        <span
          className="text-xs font-bold tracking-widest text-slate-200 uppercase font-sans"
          style={{ letterSpacing: '0.12em' }}
        >
          {label}
        </span>

        {formattedReadout && (
          <span
            className="text-[11px] font-mono mt-0.5 transition-colors"
            style={{ color: isHovered || isDragging ? color : '#a1a1aa' }}
          >
            {formattedReadout}
          </span>
        )}
      </div>
    </div>
  );
};
