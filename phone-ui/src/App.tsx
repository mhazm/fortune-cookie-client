import React, { useState, useEffect, useRef } from 'react';
import { useNismaraNative } from './hooks/useNismaraNative';
import { PhoneScreenState, PhoneAppId } from './types/telemetry';
import { StatusBar } from './components/ios/StatusBar';
import { DynamicIsland } from './components/ios/DynamicIsland';
import { HomeIndicator } from './components/ios/HomeIndicator';
import { LockScreen } from './screens/LockScreen';
import { HomeScreen } from './screens/HomeScreen';
import { DispatchApp } from './screens/DispatchApp';
import { NismaraIdApp } from './screens/NismaraIdApp';
import { GarageApp } from './screens/GarageApp';
import { TelemetryApp } from './screens/TelemetryApp';
import { PenaltyApp } from './screens/PenaltyApp';
import { SettingsApp } from './screens/SettingsApp';
import { WallpapersApp } from './screens/WallpapersApp';

export const App: React.FC = () => {
  const { telemetry, lockPhone, unlockPhone } = useNismaraNative();
  const [screenState, setScreenState] = useState<PhoneScreenState>('lock');
  const [activeApp, setActiveApp] = useState<PhoneAppId>('dispatch');
  const [theme, setTheme] = useState('ultramarine');
  const prevOpenSeqRef = useRef<number | undefined>(telemetry.overlay_open_seq);

  // Honor auto_lock_on_close when Companion Phone is reopened via F11
  useEffect(() => {
    const seq = telemetry.overlay_open_seq;
    if (seq !== undefined && prevOpenSeqRef.current !== undefined && seq !== prevOpenSeqRef.current) {
      if (telemetry.auto_lock_on_close !== false) {
        setScreenState('lock');
      }
    }
    prevOpenSeqRef.current = seq;
  }, [telemetry.overlay_open_seq, telemetry.auto_lock_on_close]);

  const handleUnlock = () => {
    setScreenState('home');
    unlockPhone();
  };

  const handleLock = () => {
    setScreenState('lock');
    lockPhone();
  };

  const handleOpenApp = (app: PhoneAppId) => {
    setActiveApp(app);
    setScreenState('app');
  };

  const handleHomeIndicator = () => {
    if (screenState === 'app') {
      setScreenState('home');
    } else if (screenState === 'home') {
      handleLock();
    }
  };

  // Theme wallpaper gradient
  const themeGradients: Record<string, string> = {
    ultramarine: 'linear-gradient(180deg, #1e3a8a 0%, #1e1b4b 40%, #020617 100%)',
    midnight: 'linear-gradient(180deg, #1e293b 0%, #0f172a 40%, #000000 100%)',
    cyberpunk: 'linear-gradient(180deg, #701a75 0%, #3b0764 40%, #020617 100%)',
    executive: 'linear-gradient(180deg, #78350f 0%, #292524 40%, #000000 100%)',
    sunset: 'linear-gradient(180deg, #881337 0%, #7c2d12 40%, #020617 100%)',
  };

  return (
    <div
      style={{
        width: 350,
        height: 758,
        position: 'relative',
        overflow: 'hidden',
        background: themeGradients[theme] || themeGradients.ultramarine,
      }}
    >
      {/* Screen Render Switcher (Base layer) */}
      <div style={{ width: 350, height: 758, position: 'absolute', inset: 0, zIndex: 0 }}>
        {screenState === 'lock' && (
          <LockScreen telemetry={telemetry} onUnlock={handleUnlock} />
        )}

        {screenState === 'home' && (
          <HomeScreen
            telemetry={telemetry}
            onOpenApp={handleOpenApp}
            onLock={handleLock}
          />
        )}

        {screenState === 'app' && (
          <>
            {activeApp === 'dispatch' && (
              <DispatchApp telemetry={telemetry} onBack={() => setScreenState('home')} />
            )}
            {activeApp === 'nismara_id' && (
              <NismaraIdApp
                telemetry={telemetry}
                onBack={() => setScreenState('home')}
                onOpenSettings={() => handleOpenApp('settings')}
              />
            )}
            {activeApp === 'garage' && (
              <GarageApp
                telemetry={telemetry}
                onBack={() => setScreenState('home')}
                onSelectFleet={(fleetNumber) => window.nismara?.selectFleet?.(fleetNumber)}
              />
            )}
            {activeApp === 'telemetry' && (
              <TelemetryApp telemetry={telemetry} onBack={() => setScreenState('home')} />
            )}
            {activeApp === 'penalty' && (
              <PenaltyApp telemetry={telemetry} onBack={() => setScreenState('home')} />
            )}
            {activeApp === 'settings' && (
              <SettingsApp telemetry={telemetry} onBack={() => setScreenState('home')} />
            )}
            {activeApp === 'wallpapers' && (
              <WallpapersApp
                currentTheme={theme}
                onSelectTheme={setTheme}
                frameColor={telemetry.phone_frame_color ?? 0}
                onBack={() => setScreenState('home')}
              />
            )}
          </>
        )}
      </div>

      {/* Status Bar (Floating top layer) */}
      <StatusBar />

      {/* Dynamic Island (Floating top layer) */}
      <DynamicIsland telemetry={telemetry} screenState={screenState} activeApp={activeApp} />

      {/* Home Indicator Bar (Floating bottom layer) */}
      <HomeIndicator onTap={handleHomeIndicator} />
    </div>
  );
};
