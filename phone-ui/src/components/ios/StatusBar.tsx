import React, { useState, useEffect } from 'react';
import { Wifi, Battery } from 'lucide-react';

interface Props {
  lightMode?: boolean;
}

export const StatusBar: React.FC<Props> = ({ lightMode = false }) => {
  const [timeStr, setTimeStr] = useState('09:41');

  useEffect(() => {
    const update = () => {
      const now = new Date();
      const hours = String(now.getHours()).padStart(2, '0');
      const minutes = String(now.getMinutes()).padStart(2, '0');
      setTimeStr(`${hours}:${minutes}`);
    };
    update();
    const interval = setInterval(update, 1000);
    return () => clearInterval(interval);
  }, []);

  const textColor = lightMode ? '#0f172a' : '#f8fafc';

  return (
    <div
      style={{
        position: 'absolute',
        top: 0,
        left: 0,
        right: 0,
        width: 350,
        height: 44,
        paddingTop: 10,
        paddingLeft: 28,
        paddingRight: 28,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        zIndex: 40,
        pointerEvents: 'none',
        color: textColor,
        boxSizing: 'border-box',
      }}
    >
      {/* Left: Clock */}
      <div style={{ width: 48, fontSize: 13, fontWeight: 700, letterSpacing: '-0.02em' }}>
        {timeStr}
      </div>

      {/* Center Spacer for Dynamic Island */}
      <div style={{ width: 124, height: 32 }} />

      {/* Right: Cellular, Wifi, Battery */}
      <div style={{ width: 48, display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: 6 }}>
        {/* Signal dots */}
        <div style={{ display: 'flex', alignItems: 'flex-end', gap: 1.5, height: 10 }}>
          <div style={{ width: 2.5, height: 4, backgroundColor: 'currentColor', borderRadius: 0.5 }} />
          <div style={{ width: 2.5, height: 6, backgroundColor: 'currentColor', borderRadius: 0.5 }} />
          <div style={{ width: 2.5, height: 8, backgroundColor: 'currentColor', borderRadius: 0.5 }} />
          <div style={{ width: 2.5, height: 10, backgroundColor: 'currentColor', borderRadius: 0.5 }} />
        </div>
        <Wifi size={13} strokeWidth={2.5} />
        <div style={{ display: 'flex', alignItems: 'center' }}>
          <Battery size={16} strokeWidth={2.2} />
        </div>
      </div>
    </div>
  );
};
