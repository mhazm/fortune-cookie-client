import React, { useState, useEffect } from 'react';
import { TelemetryData } from '../types/telemetry';
import { ChevronUp, Flashlight, Camera } from 'lucide-react';

interface Props {
  telemetry: TelemetryData;
  onUnlock: () => void;
}

export const LockScreen: React.FC<Props> = ({ telemetry, onUnlock }) => {
  const [time, setTime] = useState({ hours: '12', minutes: '00', date: 'Sunday, November 8' });

  useEffect(() => {
    const update = () => {
      const now = new Date();
      const hours = String(now.getHours()).padStart(2, '0');
      const minutes = String(now.getMinutes()).padStart(2, '0');
      const date = now.toLocaleDateString('en-US', {
        weekday: 'long',
        month: 'long',
        day: 'numeric',
      });
      setTime({ hours, minutes, date });
    };
    update();
    const interval = setInterval(update, 1000);
    return () => clearInterval(interval);
  }, []);

  const hasMatchedFleet = Boolean(telemetry.has_matched_fleet && telemetry.fleet_number);
  const fuelPct =
    telemetry.fuel_capacity && telemetry.fuel_capacity > 0
      ? Math.round((telemetry.fuel / telemetry.fuel_capacity) * 100)
      : 0;

  // Server mode text color matching ImGui
  const getServerModeColor = (mode: string) => {
    switch (mode) {
      case 'TruckersMP':
        return '#fb7185';
      case 'HaulMP':
        return '#c084fc';
      case 'SCS Convoy':
        return '#34d399';
      default:
        return '#a5c3eb';
    }
  };

  return (
    <div
      onClick={onUnlock}
      style={{
        width: 350,
        height: 758,
        paddingTop: 54,
        paddingBottom: 28,
        paddingLeft: 16,
        paddingRight: 16,
        display: 'flex',
        flexDirection: 'column',
        justifyContent: 'space-between',
        boxSizing: 'border-box',
        cursor: 'pointer',
        userSelect: 'none',
      }}
    >
      {/* 1. Header: Date, Giant 62px Clock, Driver Greeting Pill */}
      <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 4 }}>
        <span
          style={{
            fontSize: 12.5,
            fontWeight: 600,
            color: 'rgba(230, 240, 255, 0.85)',
            letterSpacing: '0.04em',
            textTransform: 'uppercase',
            textShadow: '0 2px 8px rgba(0,0,0,0.5)',
          }}
        >
          {time.date}
        </span>

        <h1
          style={{
            fontSize: 66,
            fontWeight: 800,
            lineHeight: 1,
            color: '#ffffff',
            letterSpacing: '-0.03em',
            textShadow: '0 4px 18px rgba(0,0,0,0.6)',
            margin: '2px 0 6px 0',
          }}
        >
          {time.hours}:{time.minutes}
        </h1>

        {/* Greeting Pill (tablet_gui.cpp:1359-1384) */}
        <div
          style={{
            padding: '4px 14px',
            borderRadius: 13,
            backgroundColor: 'rgba(12, 18, 34, 0.7)',
            backdropFilter: 'blur(12px)',
            border: '1px solid rgba(255, 255, 255, 0.18)',
            display: 'inline-flex',
            alignItems: 'center',
            gap: 6,
            fontSize: 12,
            fontWeight: 600,
            color: '#bee1ff',
            boxShadow: '0 2px 8px rgba(0,0,0,0.3)',
          }}
        >
          <span>
            {telemetry.driver_name
              ? `${telemetry.driver_name}  |  Lv.${telemetry.level || 1}`
              : `${telemetry.os_version || 'Nismara OS v1.0.2'}  |  Tap to Unlock`}
          </span>
        </div>
      </div>

      {/* 2. Frosted Widgets (Truck Status, Nismara Wallet, Server Status) */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
        {/* Dual Side-by-Side Widgets (tablet_gui.cpp:1386-1442) */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 10 }}>
          {/* Left Widget: TRUCK STATUS */}
          <div
            style={{
              backgroundColor: 'rgba(12, 18, 32, 0.76)',
              backdropFilter: 'blur(16px)',
              borderRadius: 16,
              border: '1px solid rgba(255, 255, 255, 0.18)',
              padding: '10px 12px',
              height: 74,
              display: 'flex',
              flexDirection: 'column',
              justifyContent: 'space-between',
              boxSizing: 'border-box',
            }}
          >
            <div style={{ fontSize: 11, fontWeight: 700, color: '#38bdf8', letterSpacing: '0.04em' }}>
              TRUCK STATUS
            </div>
            <div style={{ fontSize: 14, fontWeight: 700, color: '#ffffff', lineHeight: 1.1 }}>
              {Math.round(telemetry.speed || 0)} km/h | {fuelPct}% Fuel
            </div>
            <div style={{ fontSize: 11, color: '#a5b4cd', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>
              {hasMatchedFleet
                ? `${telemetry.fleet_number}  |  ${telemetry.engine_enabled ? 'Online' : 'Standby'}`
                : (telemetry.engine_enabled ? 'Engine Online' : 'Engine Standby')}
            </div>
          </div>

          {/* Right Widget: NISMARA WALLET */}
          <div
            style={{
              backgroundColor: 'rgba(12, 18, 32, 0.76)',
              backdropFilter: 'blur(16px)',
              borderRadius: 16,
              border: '1px solid rgba(255, 255, 255, 0.18)',
              padding: '10px 12px',
              height: 74,
              display: 'flex',
              flexDirection: 'column',
              justifyContent: 'space-between',
              boxSizing: 'border-box',
            }}
          >
            <div style={{ fontSize: 11, fontWeight: 700, color: '#facc15', letterSpacing: '0.04em' }}>
              NISMARA WALLET
            </div>
            <div style={{ fontSize: 14, fontWeight: 700, color: '#ffffff', lineHeight: 1.1 }}>
              {Math.round(telemetry.total_nc || 0).toLocaleString('id-ID')} NC
            </div>
            <div style={{ fontSize: 11, color: '#a5b4cd' }}>
              {telemetry.nismara_plus_active ? 'Nismara+ Active' : 'Standard Driver'}
            </div>
          </div>
        </div>

        {/* Full-Width SERVER STATUS Widget (tablet_gui.cpp:1444-1483) */}
        <div
          style={{
            backgroundColor: 'rgba(12, 18, 32, 0.76)',
            backdropFilter: 'blur(16px)',
            borderRadius: 16,
            border: '1px solid rgba(255, 255, 255, 0.18)',
            padding: '10px 14px',
            height: 72,
            display: 'flex',
            flexDirection: 'column',
            justifyContent: 'space-between',
            boxSizing: 'border-box',
          }}
        >
          <div style={{ fontSize: 11, fontWeight: 700, color: '#34d399', letterSpacing: '0.04em' }}>
            SERVER STATUS
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
            <div
              style={{
                width: 8,
                height: 8,
                borderRadius: 4,
                backgroundColor: telemetry.sdk_active !== false ? '#34d399' : '#fbbf24',
                boxShadow: telemetry.sdk_active !== false
                  ? '0 0 6px rgba(52, 211, 153, 0.8)'
                  : '0 0 6px rgba(251, 191, 36, 0.8)',
              }}
            />
            <span style={{ fontSize: 14, fontWeight: 700, color: '#ffffff' }}>
              {telemetry.sdk_active !== false ? 'Connected' : 'Standby'}
            </span>
          </div>
          <div
            style={{
              fontSize: 12,
              fontWeight: 500,
              color: getServerModeColor(telemetry.server_mode || 'Singleplayer'),
            }}
          >
            Playing {telemetry.server_mode || 'Singleplayer'}
          </div>
        </div>

        {/* Notification Card (tablet_gui.cpp:1485-1516) */}
        <div
          style={{
            backgroundColor: 'rgba(14, 20, 36, 0.82)',
            backdropFilter: 'blur(16px)',
            borderRadius: 18,
            border: '1px solid rgba(255, 255, 255, 0.2)',
            padding: '12px 14px',
            boxSizing: 'border-box',
            minHeight: 84,
            display: 'flex',
            flexDirection: 'column',
            justifyContent: 'space-between',
          }}
        >
          {telemetry.job_active ? (
            <>
              <div style={{ fontSize: 11, fontWeight: 700, color: '#34d399', letterSpacing: '0.04em' }}>
                ACTIVE JOB CONTRACT
              </div>
              <div style={{ fontSize: 14, fontWeight: 700, color: '#ffffff', lineHeight: 1.2 }}>
                {telemetry.city_src || 'Origin'} &rarr; {telemetry.city_dst || 'Destination'}
              </div>
              <div style={{ fontSize: 11.5, color: '#b4c3dc' }}>
                {telemetry.cargo || 'Cargo'}  |  {(telemetry.nav_distance_km || 0).toFixed(1)} km remaining
              </div>
            </>
          ) : (
            <>
              <div style={{ fontSize: 11, fontWeight: 700, color: '#38bdf8', letterSpacing: '0.04em' }}>
                NISMARA LOGISTICS OS
              </div>
              <div style={{ fontSize: 14, fontWeight: 700, color: '#ffffff' }}>
                No Active Cargo Contract
              </div>
              <div style={{ fontSize: 11.5, color: '#b4c3dc' }}>
                Accept a freight job in-game to start tracking.
              </div>
            </>
          )}
        </div>
      </div>

      {/* 3. Bottom Controls: Swipe Prompt, Flashlight, Camera */}
      <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center' }}>
        <div
          style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            color: 'rgba(255, 255, 255, 0.75)',
            marginBottom: 10,
          }}
        >
          <ChevronUp size={16} />
          <span style={{ fontSize: 11, fontWeight: 500, letterSpacing: '0.02em', marginTop: -2 }}>
            Swipe up or click to unlock
          </span>
        </div>

        <div style={{ width: '100%', display: 'flex', justifyContent: 'space-between', paddingLeft: 12, paddingRight: 12 }}>
          <div
            style={{
              width: 44,
              height: 44,
              borderRadius: 22,
              backgroundColor: 'rgba(255, 255, 255, 0.16)',
              backdropFilter: 'blur(16px)',
              border: '1px solid rgba(255, 255, 255, 0.22)',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              color: '#ffffff',
              boxShadow: '0 4px 12px rgba(0,0,0,0.35)',
            }}
          >
            <Flashlight size={19} />
          </div>
          <div
            style={{
              width: 44,
              height: 44,
              borderRadius: 22,
              backgroundColor: 'rgba(255, 255, 255, 0.16)',
              backdropFilter: 'blur(16px)',
              border: '1px solid rgba(255, 255, 255, 0.22)',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              color: '#ffffff',
              boxShadow: '0 4px 12px rgba(0,0,0,0.35)',
            }}
          >
            <Camera size={19} />
          </div>
        </div>
      </div>
    </div>
  );
};
