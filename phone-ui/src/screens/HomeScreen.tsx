import React, { useState } from 'react';
import { TelemetryData, PhoneAppId } from '../types/telemetry';
import {
  Gauge,
  ClipboardList,
  CreditCard,
  Warehouse,
  ShieldAlert,
  Image as ImageIcon,
  Settings,
  Lock,
} from 'lucide-react';

interface Props {
  telemetry: TelemetryData;
  onOpenApp: (app: PhoneAppId) => void;
  onLock: () => void;
}

export const HomeScreen: React.FC<Props> = ({ telemetry, onOpenApp, onLock }) => {
  const [avatarError, setAvatarError] = useState(false);
  const hasMatchedFleet = Boolean(telemetry.has_matched_fleet && telemetry.fleet_number);
  const driverInitials = (telemetry.driver_name || 'ND')
    .split(/\s+/)
    .filter(Boolean)
    .slice(0, 2)
    .map((part) => part[0]?.toUpperCase() || '')
    .join('') || 'ND';
  const gearText = telemetry.gear_text
    ? telemetry.gear_text
    : telemetry.gear > 0
    ? `D${telemetry.gear}`
    : telemetry.gear < 0
    ? 'R'
    : 'N';
  const speedKmh = Math.round(telemetry.speed || 0);

  // Base truck title
  const baseTruck =
    telemetry.truck_name
      ? (telemetry.truck_brand && !telemetry.truck_name.toLowerCase().includes(telemetry.truck_brand.toLowerCase())
          ? `${telemetry.truck_brand} ${telemetry.truck_name}`
          : telemetry.truck_name)
      : (telemetry.truck_brand ? `${telemetry.truck_brand} Truck` : '');

  const truckLabel = hasMatchedFleet
    ? `[${telemetry.fleet_number}] ${baseTruck || 'In-Game Truck'}`
    : (baseTruck ? `[UNREGISTERED] ${baseTruck}` : 'Standby / Cab Free Roam');

  // 8 Grid Apps matching tablet_gui.cpp:1634-1643
  const gridApps = [
    {
      id: 'telemetry' as PhoneAppId,
      name: 'Telemetry',
      icon: Gauge,
      gradient: 'linear-gradient(135deg, #38bdf8 0%, #0284c7 100%)',
    },
    {
      id: 'dispatch' as PhoneAppId,
      name: 'Job',
      icon: ClipboardList,
      gradient: 'linear-gradient(135deg, #fbbf24 0%, #d97706 100%)',
      badge: telemetry.job_active ? 'JOB' : undefined,
      badgeBg: '#f59e0b',
    },
    {
      id: 'nismara_id' as PhoneAppId,
      name: 'Nismara ID',
      icon: CreditCard,
      gradient: 'linear-gradient(135deg, #a78bfa 0%, #6d28d9 100%)',
    },
    {
      id: 'garage' as PhoneAppId,
      name: 'Garage',
      icon: Warehouse,
      gradient: 'linear-gradient(135deg, #34d399 0%, #059669 100%)',
    },
    {
      id: 'penalty' as PhoneAppId,
      name: 'Penalty',
      icon: ShieldAlert,
      gradient: 'linear-gradient(135deg, #ef4444 0%, #b91c1c 100%)',
      badge: telemetry.penalty_points > 0 ? `${telemetry.penalty_points}` : undefined,
      badgeBg: '#ef4444',
    },
    {
      id: 'wallpapers' as PhoneAppId,
      name: 'Wallpapers',
      icon: ImageIcon,
      gradient: 'linear-gradient(135deg, #e879f9 0%, #9333ea 100%)',
    },
    {
      id: 'settings' as PhoneAppId,
      name: 'Settings',
      icon: Settings,
      gradient: 'linear-gradient(135deg, #94a3b8 0%, #475569 100%)',
    },
    {
      id: 'lock' as any,
      name: 'Lock',
      icon: Lock,
      gradient: 'linear-gradient(135deg, #4b5563 0%, #1f2937 100%)',
      isLock: true,
    },
  ];

  // 4 Dock Apps matching tablet_gui.cpp:1723-1728
  const dockApps = [
    {
      id: 'telemetry' as PhoneAppId,
      name: 'Telemetry',
      icon: Gauge,
      gradient: 'linear-gradient(135deg, #38bdf8 0%, #0284c7 100%)',
    },
    {
      id: 'dispatch' as PhoneAppId,
      name: 'Job',
      icon: ClipboardList,
      gradient: 'linear-gradient(135deg, #fbbf24 0%, #d97706 100%)',
    },
    {
      id: 'nismara_id' as PhoneAppId,
      name: 'Nismara ID',
      icon: CreditCard,
      gradient: 'linear-gradient(135deg, #a78bfa 0%, #6d28d9 100%)',
    },
    {
      id: 'settings' as PhoneAppId,
      name: 'Settings',
      icon: Settings,
      gradient: 'linear-gradient(135deg, #94a3b8 0%, #475569 100%)',
    },
  ];

  return (
    <div
      style={{
        width: 350,
        height: 758,
        paddingTop: 52,
        paddingBottom: 22,
        paddingLeft: 16,
        paddingRight: 16,
        display: 'flex',
        flexDirection: 'column',
        justifyContent: 'space-between',
        boxSizing: 'border-box',
        userSelect: 'none',
      }}
    >
      {/* 1. Top Section: Smart Widget & App Grid */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: 18 }}>
        {/* Top iOS 18 Smart Widget (tablet_gui.cpp:1546-1623) */}
        <div
          onClick={() => onOpenApp('nismara_id')}
          style={{
            backgroundColor: 'rgba(11, 17, 30, 0.8)',
            backdropFilter: 'blur(16px)',
            borderRadius: 20,
            border: '1px solid rgba(255, 255, 255, 0.19)',
            padding: '12px 14px',
            boxSizing: 'border-box',
            boxShadow: '0 8px 24px rgba(0,0,0,0.3)',
            cursor: 'pointer',
            display: 'flex',
            flexDirection: 'column',
            gap: 8,
          }}
        >
          {/* Top Row: Driver Avatar, Title & Status Dot */}
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', gap: 8 }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 8, minWidth: 0, flex: 1 }}>
              <div
                style={{
                  width: 28,
                  height: 28,
                  borderRadius: 14,
                  overflow: 'hidden',
                  flexShrink: 0,
                  border: '1.5px solid rgba(56, 189, 248, 0.7)',
                  backgroundColor: 'rgba(15, 23, 42, 0.9)',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                }}
              >
                {telemetry.driver_avatar && !avatarError ? (
                  <img
                    src={telemetry.driver_avatar}
                    alt={telemetry.driver_name}
                    onError={() => setAvatarError(true)}
                    style={{ width: '100%', height: '100%', objectFit: 'cover', display: 'block' }}
                  />
                ) : (
                  <span style={{ fontSize: 10, fontWeight: 800, color: '#38bdf8' }}>
                    {driverInitials}
                  </span>
                )}
              </div>
              <span
                style={{
                  fontSize: 14,
                  fontWeight: 700,
                  color: '#38bdf8',
                  letterSpacing: '-0.01em',
                  whiteSpace: 'nowrap',
                  overflow: 'hidden',
                  textOverflow: 'ellipsis',
                }}
              >
                {telemetry.driver_name
                  ? `${telemetry.driver_name} (${telemetry.rank || 'Driver'})`
                  : 'Nismara Driver Unit'}
              </span>
            </div>
            <div
              style={{
                width: 9,
                height: 9,
                borderRadius: 4.5,
                backgroundColor: telemetry.driver_name ? '#34d399' : '#fbbf24',
                boxShadow: telemetry.driver_name
                  ? '0 0 8px rgba(52, 211, 153, 0.8)'
                  : '0 0 8px rgba(251, 191, 36, 0.8)',
                flexShrink: 0,
              }}
            />
          </div>

          {/* Line 2: Truck Label */}
          <div
            style={{
              fontSize: 13,
              fontWeight: hasMatchedFleet ? 700 : 500,
              color: hasMatchedFleet ? '#34d399' : (telemetry.truck_name ? '#fbbf24' : '#e2e8f0'),
              whiteSpace: 'nowrap',
              overflow: 'hidden',
              textOverflow: 'ellipsis',
            }}
          >
            {truckLabel}
          </div>

          {/* Two Structured Mini-Pills (SPEED & GEAR | WALLET BALANCE) */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 8, marginTop: 2 }}>
            {/* Pill 1: Speed & Gear */}
            <div
              style={{
                backgroundColor: 'rgba(20, 30, 52, 0.75)',
                borderRadius: 10,
                padding: '6px 10px',
                display: 'flex',
                flexDirection: 'column',
                justifyContent: 'center',
              }}
            >
              <span style={{ fontSize: 10, fontWeight: 700, color: '#94a3b8', letterSpacing: '0.04em' }}>
                SPEED & GEAR
              </span>
              <span style={{ fontSize: 13.5, fontWeight: 700, color: '#ffffff', marginTop: 2 }}>
                {speedKmh} km/h [{gearText}]
              </span>
            </div>

            {/* Pill 2: Wallet Balance */}
            <div
              style={{
                backgroundColor: 'rgba(20, 30, 52, 0.75)',
                borderRadius: 10,
                padding: '6px 10px',
                display: 'flex',
                flexDirection: 'column',
                justifyContent: 'center',
              }}
            >
              <span style={{ fontSize: 10, fontWeight: 700, color: '#facc15', letterSpacing: '0.04em' }}>
                WALLET BALANCE
              </span>
              <span style={{ fontSize: 13.5, fontWeight: 700, color: '#facc15', marginTop: 2 }}>
                {Math.round(telemetry.total_nc || 0).toLocaleString('id-ID')} NC
              </span>
            </div>
          </div>
        </div>

        {/* 2. iOS App Icon Grid (4 cols x 2 rows = 8 icons, tablet_gui.cpp:1634-1710) */}
        <div
          style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(4, 1fr)',
            columnGap: 14,
            rowGap: 18,
            paddingLeft: 4,
            paddingRight: 4,
          }}
        >
          {gridApps.map((app) => {
            const Icon = app.icon;
            return (
              <div
                key={app.name}
                onClick={() => {
                  if (app.isLock) {
                    onLock();
                  } else {
                    onOpenApp(app.id);
                  }
                }}
                style={{
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  cursor: 'pointer',
                  userSelect: 'none',
                }}
              >
                {/* Squircle Icon 56x56 */}
                <div
                  style={{
                    width: 56,
                    height: 56,
                    borderRadius: 16,
                    background: app.gradient,
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    color: '#ffffff',
                    position: 'relative',
                    boxShadow: '0 4px 14px rgba(0,0,0,0.3)',
                    border: '1px solid rgba(255,255,255,0.15)',
                    transition: 'transform 0.15s ease',
                  }}
                  onMouseDown={(e) => ((e.currentTarget as HTMLElement).style.transform = 'scale(0.92)')}
                  onMouseUp={(e) => ((e.currentTarget as HTMLElement).style.transform = 'scale(1)')}
                  onMouseLeave={(e) => ((e.currentTarget as HTMLElement).style.transform = 'scale(1)')}
                >
                  <Icon size={26} strokeWidth={2.2} />

                  {/* Badge */}
                  {app.badge && (
                    <div
                      style={{
                        position: 'absolute',
                        top: -4,
                        right: -4,
                        backgroundColor: app.badgeBg,
                        color: '#ffffff',
                        fontSize: 9,
                        fontWeight: 800,
                        padding: '1px 5px',
                        borderRadius: 8,
                        boxShadow: '0 2px 6px rgba(0,0,0,0.4)',
                        border: '1px solid rgba(255,255,255,0.3)',
                      }}
                    >
                      {app.badge}
                    </div>
                  )}
                </div>

                {/* App Label */}
                <span
                  style={{
                    fontSize: 11.5,
                    fontWeight: 500,
                    color: '#f8fafc',
                    marginTop: 6,
                    textAlign: 'center',
                    letterSpacing: '-0.01em',
                    textShadow: '0 1px 4px rgba(0,0,0,0.8)',
                  }}
                >
                  {app.name}
                </span>
              </div>
            );
          })}
        </div>
      </div>

      {/* 3. Bottom Section: Page Indicator & Frosted Glass Dock (tablet_gui.cpp:1712-1750) */}
      <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 10 }}>
        {/* Page Dots */}
        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
          <div style={{ width: 6, height: 6, borderRadius: 3, backgroundColor: 'rgba(255, 255, 255, 0.9)' }} />
          <div style={{ width: 6, height: 6, borderRadius: 3, backgroundColor: 'rgba(255, 255, 255, 0.35)' }} />
        </div>

        {/* Frosted Dock */}
        <div
          style={{
            width: '100%',
            height: 74,
            borderRadius: 26,
            backgroundColor: 'rgba(16, 23, 40, 0.78)',
            backdropFilter: 'blur(20px)',
            border: '1px solid rgba(255, 255, 255, 0.2)',
            boxShadow: '0 8px 26px rgba(0,0,0,0.45)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-around',
            paddingLeft: 12,
            paddingRight: 12,
            boxSizing: 'border-box',
          }}
        >
          {dockApps.map((app) => {
            const Icon = app.icon;
            return (
              <div
                key={`dock-${app.id}`}
                onClick={() => onOpenApp(app.id)}
                style={{
                  width: 50,
                  height: 50,
                  borderRadius: 14,
                  background: app.gradient,
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  color: '#ffffff',
                  cursor: 'pointer',
                  boxShadow: '0 3px 10px rgba(0,0,0,0.3)',
                  border: '1px solid rgba(255,255,255,0.15)',
                  transition: 'transform 0.15s ease',
                }}
                onMouseDown={(e) => ((e.currentTarget as HTMLElement).style.transform = 'scale(0.92)')}
                onMouseUp={(e) => ((e.currentTarget as HTMLElement).style.transform = 'scale(1)')}
                onMouseLeave={(e) => ((e.currentTarget as HTMLElement).style.transform = 'scale(1)')}
              >
                <Icon size={24} strokeWidth={2.2} />
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
};
