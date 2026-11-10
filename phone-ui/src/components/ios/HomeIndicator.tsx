import React from 'react';

interface Props {
  onTap?: () => void;
  lightMode?: boolean;
}

export const HomeIndicator: React.FC<Props> = ({ onTap, lightMode = false }) => {
  return (
    <div
      onClick={onTap}
      style={{
        position: 'absolute',
        bottom: 8,
        left: 0,
        right: 0,
        height: 20,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 50,
        cursor: 'pointer',
      }}
    >
      <div
        style={{
          width: 134,
          height: 4,
          borderRadius: 2,
          backgroundColor: lightMode ? 'rgba(15,23,42,0.5)' : 'rgba(255,255,255,0.45)',
          transition: 'all 0.2s',
        }}
      />
    </div>
  );
};
