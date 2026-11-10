import React, { useState } from 'react';
import { Check, ChevronLeft, Palette, Smartphone } from 'lucide-react';

interface Props {
  currentTheme: string;
  onSelectTheme: (theme: string) => void;
  frameColor?: number;
  onBack: () => void;
}

export const WallpapersApp: React.FC<Props> = ({
  currentTheme,
  onSelectTheme,
  frameColor = 0,
  onBack,
}) => {
  const [tab, setTab] = useState<'wallpaper' | 'frame'>('wallpaper');
  const [selectedFrame, setSelectedFrame] = useState<number>(frameColor);

  const themes = [
    {
      id: 'ultramarine',
      name: 'Nismara Ultramarine',
      desc: 'Deep Royal Indigo & Cyan Glow',
      grad: 'linear-gradient(135deg, #2563eb 0%, #1e1b4b 55%, #020617 100%)',
    },
    {
      id: 'midnight',
      name: 'Midnight Titanium',
      desc: 'Obsidian Slate & Carbon Matte',
      grad: 'linear-gradient(135deg, #334155 0%, #0f172a 55%, #000000 100%)',
    },
    {
      id: 'executive',
      name: 'Executive Gold',
      desc: 'Warm Amber & Luxury Onyx',
      grad: 'linear-gradient(135deg, #d97706 0%, #292524 55%, #000000 100%)',
    },
    {
      id: 'cyberpunk',
      name: 'Cyberpunk Neon',
      desc: 'Magenta Pulse & Deep Violet',
      grad: 'linear-gradient(135deg, #c026d3 0%, #3b0764 55%, #020617 100%)',
    },
    {
      id: 'sunset',
      name: 'Sunset Horizon',
      desc: 'Crimson Dusk & Warm Copper',
      grad: 'linear-gradient(135deg, #e11d48 0%, #7c2d12 55%, #020617 100%)',
    },
  ];

  // 5 Hardware Frame Colors matching TabletGui::RenderAppWallpapers (tablet_gui.cpp:3350-3385)
  const frameColors = [
    {
      idx: 0,
      name: 'Ultramarine',
      desc: 'Sapphire anodized aluminum',
      swatch: '#5d6cf0',
      border: '#818cf8',
    },
    {
      idx: 1,
      name: 'Black Titanium',
      desc: 'Stealth dark matte finish',
      swatch: '#272a30',
      border: '#64748b',
    },
    {
      idx: 2,
      name: 'Natural Titanium',
      desc: 'Brushed metallic silver-grey',
      swatch: '#9ca3af',
      border: '#cbd5e1',
    },
    {
      idx: 3,
      name: 'Teal',
      desc: 'Emerald-cyan anodized finish',
      swatch: '#2dd4bf',
      border: '#5eead4',
    },
    {
      idx: 4,
      name: 'Rose Pink',
      desc: 'Soft rose anodized finish',
      swatch: '#f472b6',
      border: '#f9a8d4',
    },
  ];

  const handleSelectFrame = (idx: number) => {
    setSelectedFrame(idx);
    window.nismara?.setFrameColor?.(idx);
  };

  return (
    <div
      style={{
        width: 350,
        height: 758,
        paddingTop: 52,
        paddingBottom: 28,
        paddingLeft: 16,
        paddingRight: 16,
        display: 'flex',
        flexDirection: 'column',
        boxSizing: 'border-box',
        backgroundColor: 'var(--surface-app)',
        color: 'var(--text-primary)',
        fontFamily: 'var(--font-sans)',
        userSelect: 'none',
      }}
    >
      {/* Header */}
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          paddingBottom: 10,
          borderBottom: '1px solid var(--border-subtle)',
        }}
      >
        <button
          onClick={onBack}
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: 2,
            fontSize: 12,
            fontWeight: 600,
            color: '#e879f9',
            background: 'none',
            border: 'none',
            cursor: 'pointer',
            padding: 0,
          }}
        >
          <ChevronLeft size={15} />
          <span>Home</span>
        </button>
        <span style={{ fontSize: 13.5, fontWeight: 700, color: '#ffffff' }}>
          Tema & Tampilan
        </span>
        <div style={{ width: 42 }} />
      </div>

      {/* Segmented Control: [Tema Layar] | [Warna Casing] */}
      <div
        style={{
          display: 'grid',
          gridTemplateColumns: '1fr 1fr',
          backgroundColor: 'rgba(15, 23, 42, 0.85)',
          borderRadius: 11,
          padding: 3,
          margin: '10px 0',
          border: '1px solid var(--border-subtle)',
        }}
      >
        <button
          onClick={() => setTab('wallpaper')}
          style={{
            padding: '6px 0',
            borderRadius: 8,
            fontSize: 11,
            fontWeight: tab === 'wallpaper' ? 700 : 500,
            color: tab === 'wallpaper' ? '#020617' : '#94a3b8',
            backgroundColor: tab === 'wallpaper' ? '#e879f9' : 'transparent',
            border: 'none',
            cursor: 'pointer',
            transition: 'all 0.16s ease',
          }}
        >
          Tema Layar
        </button>
        <button
          onClick={() => setTab('frame')}
          style={{
            padding: '6px 0',
            borderRadius: 8,
            fontSize: 11,
            fontWeight: tab === 'frame' ? 700 : 500,
            color: tab === 'frame' ? '#020617' : '#94a3b8',
            backgroundColor: tab === 'frame' ? '#e879f9' : 'transparent',
            border: 'none',
            cursor: 'pointer',
            transition: 'all 0.16s ease',
          }}
        >
          Warna Casing
        </button>
      </div>

      <div data-scroll-container="true" className="scroll-area">
        <div
          style={{
            display: 'flex',
            flexDirection: 'column',
            gap: 10,
            paddingBottom: 24,
          }}
        >
        {tab === 'wallpaper' ? (
          <>
            <div style={{ display: 'flex', alignItems: 'center', gap: 6, marginTop: 2 }}>
              <Palette size={13} style={{ color: '#e879f9' }} />
              <span
                style={{
                  fontSize: 10.5,
                  fontWeight: 800,
                  color: '#94a3b8',
                  letterSpacing: '0.05em',
                }}
              >
                PRESET TEMA NISMARA OS
              </span>
            </div>

            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 9 }}>
              {themes.map((t) => {
                const active = currentTheme === t.id;
                return (
                  <div
                    key={t.id}
                    onClick={() => onSelectTheme(t.id)}
                    style={{
                      background: t.grad,
                      borderRadius: 15,
                      padding: 12,
                      height: 106,
                      display: 'flex',
                      flexDirection: 'column',
                      justifyContent: 'space-between',
                      cursor: 'pointer',
                      border: active
                        ? '2px solid #ffffff'
                        : '1px solid rgba(255, 255, 255, 0.16)',
                      boxShadow: active
                        ? '0 0 0 2px rgba(232, 121, 249, 0.45), 0 8px 18px rgba(0,0,0,0.4)'
                        : '0 4px 12px rgba(0,0,0,0.3)',
                      boxSizing: 'border-box',
                    }}
                  >
                    <div>
                      <div
                        style={{
                          fontSize: 12,
                          fontWeight: 800,
                          color: '#ffffff',
                          textShadow: '0 1px 4px rgba(0,0,0,0.6)',
                        }}
                      >
                        {t.name}
                      </div>
                      <div
                        style={{
                          fontSize: 9.5,
                          color: 'rgba(241, 245, 249, 0.8)',
                          marginTop: 2,
                          lineHeight: 1.25,
                        }}
                      >
                        {t.desc}
                      </div>
                    </div>

                    {active && (
                      <div
                        style={{
                          width: 22,
                          height: 22,
                          borderRadius: 11,
                          backgroundColor: '#ffffff',
                          color: '#020617',
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'center',
                          alignSelf: 'flex-end',
                          boxShadow: '0 2px 6px rgba(0,0,0,0.4)',
                        }}
                      >
                        <Check size={13} strokeWidth={3} />
                      </div>
                    )}
                  </div>
                );
              })}
            </div>
          </>
        ) : (
          <>
            <div style={{ display: 'flex', alignItems: 'center', gap: 6, marginTop: 2 }}>
              <Smartphone size={13} style={{ color: '#e879f9' }} />
              <span
                style={{
                  fontSize: 10.5,
                  fontWeight: 800,
                  color: '#94a3b8',
                  letterSpacing: '0.05em',
                }}
              >
                WARNA CASING COMPANION PHONE
              </span>
            </div>

            <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
              {frameColors.map((fc) => {
                const active = selectedFrame === fc.idx;
                return (
                  <div
                    key={fc.idx}
                    onClick={() => handleSelectFrame(fc.idx)}
                    style={{
                      backgroundColor: 'var(--surface-card)',
                      borderRadius: 13,
                      border: active
                        ? '1.5px solid #e879f9'
                        : '1px solid var(--border-subtle)',
                      padding: '10px 12px',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'space-between',
                      cursor: 'pointer',
                    }}
                  >
                    <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
                      <div
                        style={{
                          width: 28,
                          height: 28,
                          borderRadius: 14,
                          backgroundColor: fc.swatch,
                          border: `2px solid ${fc.border}`,
                          boxShadow: '0 2px 6px rgba(0,0,0,0.4)',
                        }}
                      />
                      <div>
                        <div style={{ fontSize: 12, fontWeight: 700, color: '#ffffff' }}>
                          {fc.name}
                        </div>
                        <div style={{ fontSize: 10, color: '#94a3b8', marginTop: 1 }}>
                          {fc.desc}
                        </div>
                      </div>
                    </div>

                    {active && (
                      <div
                        style={{
                          width: 22,
                          height: 22,
                          borderRadius: 11,
                          backgroundColor: '#e879f9',
                          color: '#020617',
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'center',
                        }}
                      >
                        <Check size={13} strokeWidth={3} />
                      </div>
                    )}
                  </div>
                );
              })}
            </div>
          </>
        )}
        </div>
      </div>
    </div>
  );
};
