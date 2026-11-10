import React, { useState } from 'react';
import { TelemetryData } from '../types/telemetry';
import {
  ChevronLeft,
  Compass,
  Fuel,
  Gauge,
  Navigation,
  ShieldAlert,
  ShieldCheck,
  Thermometer,
  Truck,
  Wrench,
  Zap,
} from 'lucide-react';

interface Props {
  telemetry: TelemetryData;
  onBack: () => void;
}

export const TelemetryApp: React.FC<Props> = ({ telemetry, onBack }) => {
  const [tab, setTab] = useState<'cockpit' | 'powertrain' | 'diagnostik'>('cockpit');

  const speedKmh = Math.round(telemetry.speed || 0);
  const speedLimit = Math.round(telemetry.speed_limit_kmh || 0);
  const isOverLimit = speedLimit > 0 && speedKmh > speedLimit + 5;

  const rpm = Math.round(telemetry.rpm || 0);
  const rpmLimit = telemetry.max_rpm && telemetry.max_rpm > 500 ? telemetry.max_rpm : 2500;
  const rpmPct = Math.min(100, Math.max(0, (rpm / rpmLimit) * 100));

  let rpmZoneLabel = 'IDLE';
  let rpmZoneColor = '#94a3b8';
  if (rpmPct >= 82 || rpm >= 2050) {
    rpmZoneLabel = 'REDLINE';
    rpmZoneColor = '#f87171';
  } else if (rpm >= 1000 && rpm <= 1550) {
    rpmZoneLabel = 'ECO PEAK';
    rpmZoneColor = '#34d399';
  } else if (rpm > 1550) {
    rpmZoneLabel = 'HIGH POWER';
    rpmZoneColor = '#fbbf24';
  }

  const gearText = telemetry.gear_text
    ? telemetry.gear_text
    : telemetry.gear > 0
    ? `D${telemetry.gear}`
    : telemetry.gear < 0
    ? 'R'
    : 'N';

  const fuelLiters = Math.round(telemetry.fuel || 0);
  const fuelCap = Math.round(telemetry.fuel_capacity || 0);
  const fuelPct = fuelCap > 0 ? Math.min(100, Math.max(0, Math.round((fuelLiters / fuelCap) * 100))) : 0;
  const fuelLow = Boolean(telemetry.fuel_warning) || (fuelCap > 0 && fuelPct < 15);
  const fuelRangeKm = Math.round(telemetry.fuel_range || 0);
  const fuelAvgL100 = (telemetry.fuel_avg_consumption || 0) * 100;

  // Heading & 8-Wind Cardinal Direction (matching tablet_gui.cpp:2002-2010)
  const headingDeg = (((telemetry.heading_deg || 0) % 360) + 360) % 360;
  const dirs = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'];
  const dirIdx = Math.floor((headingDeg + 22.5) / 45) % 8;
  const cardinalDir = dirs[dirIdx] || 'N';

  // 6 Telltale Lamps (matching tablet_gui.cpp:1966-1993)
  const retarderLvl = telemetry.retarder_level || 0;
  const retarderActive = Boolean(telemetry.motor_brake) || retarderLvl > 0;
  const retarderLabel = retarderLvl > 0 ? `R${retarderLvl}` : 'RET';

  const telltaleLamps = [
    { label: '(P)', sub: 'PARK', active: Boolean(telemetry.park_brake), color: '#f87171' },
    { label: 'ENG', sub: 'ENGINE', active: Boolean(telemetry.engine_enabled), color: '#34d399' },
    { label: retarderLabel, sub: 'BRAKE', active: retarderActive, color: '#fbbf24' },
    { label: 'BEAM', sub: 'HIGH', active: Boolean(telemetry.light_high_beam), color: '#38bdf8' },
    { label: 'HAZD', sub: 'WARN', active: Boolean(telemetry.hazard_warning), color: '#fb923c' },
    { label: 'DIFF', sub: 'LOCK', active: Boolean(telemetry.differential_lock), color: '#c084fc' },
  ];

  // Structural Health & Wear (matching tablet_gui.cpp:2158-2235)
  const wearEngine = (telemetry.wear_engine || 0) * 100;
  const wearTrans = (telemetry.wear_transmission || 0) * 100;
  const wearCabin = (telemetry.wear_cabin || 0) * 100;
  const wearChassis = (telemetry.wear_chassis || 0) * 100;
  const wearWheels = (telemetry.wear_wheels || 0) * 100;
  const maxWearPct = Math.max(wearEngine, wearTrans, wearCabin, wearChassis, wearWheels);
  const overallHealthPct = Math.max(0, Math.min(100, 100 - maxWearPct));

  let healthStatus = 'KONDISI PRIMA';
  let healthColor = '#34d399';
  if (overallHealthPct < 70) {
    healthStatus = 'PERLU SERVIS KRITIS!';
    healthColor = '#f87171';
  } else if (overallHealthPct < 90) {
    healthStatus = 'TERDETEKSI KEAUSAN';
    healthColor = '#fbbf24';
  }

  const hasMatchedFleet = Boolean(telemetry.has_matched_fleet && telemetry.fleet_number);
  const baseTruck = telemetry.truck_name
    ? telemetry.truck_brand && !telemetry.truck_name.toLowerCase().includes(telemetry.truck_brand.toLowerCase())
      ? `${telemetry.truck_brand} ${telemetry.truck_name}`
      : telemetry.truck_name
    : telemetry.truck_brand || 'Standard Tractor';

  const airPsi = telemetry.brake_air_psi && telemetry.brake_air_psi > 0
    ? telemetry.brake_air_psi
    : (telemetry.air_pressure || 0) * 14.5038;

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
            color: '#38bdf8',
            background: 'none',
            border: 'none',
            cursor: 'pointer',
            padding: 0,
          }}
        >
          <ChevronLeft size={15} />
          <span>Home</span>
        </button>
        <span style={{ fontSize: 13.5, fontWeight: 700, color: '#ffffff', letterSpacing: '-0.01em' }}>
          Telemetry Cockpit
        </span>
        <span
          className="tabular-nums"
          style={{
            fontSize: 9.5,
            fontWeight: 800,
            padding: '2px 6px',
            borderRadius: 6,
            backgroundColor: telemetry.engine_enabled
              ? 'rgba(16, 185, 129, 0.16)'
              : 'rgba(148, 163, 184, 0.14)',
            color: telemetry.engine_enabled ? '#34d399' : '#94a3b8',
            border: telemetry.engine_enabled
              ? '1px solid rgba(52, 211, 153, 0.35)'
              : '1px solid rgba(255, 255, 255, 0.08)',
          }}
        >
          {telemetry.engine_enabled ? 'IGN ON' : 'IGN OFF'}
        </span>
      </div>

      {/* Segmented Control: [Cockpit] [Powertrain] [Diagnostik] */}
      <div
        style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(3, 1fr)',
          backgroundColor: 'rgba(15, 23, 42, 0.85)',
          borderRadius: 11,
          padding: 3,
          margin: '10px 0',
          border: '1px solid var(--border-subtle)',
        }}
      >
        {(['cockpit', 'powertrain', 'diagnostik'] as const).map((t) => {
          const label = t === 'cockpit' ? 'Cockpit' : t === 'powertrain' ? 'Powertrain' : 'Diagnostik';
          const active = tab === t;
          return (
            <button
              key={t}
              onClick={() => setTab(t)}
              style={{
                padding: '6px 0',
                borderRadius: 8,
                fontSize: 11,
                fontWeight: active ? 700 : 500,
                color: active ? '#020617' : '#94a3b8',
                backgroundColor: active ? '#38bdf8' : 'transparent',
                border: 'none',
                cursor: 'pointer',
                transition: 'all 0.16s ease',
              }}
            >
              {label}
            </button>
          );
        })}
      </div>

      {/* Scrollable Content */}
      <div data-scroll-container="true" className="scroll-area">
        <div
          style={{
            display: 'flex',
            flexDirection: 'column',
            gap: 10,
            paddingBottom: 24,
          }}
        >
          {tab === 'cockpit' && (
            <>
              {/* 1. Digital Cluster Card (matching tablet_gui.cpp:1875-1995) */}
              <div
                style={{
                  backgroundColor: 'var(--surface-card)',
                  borderRadius: 16,
                  border: '1px solid var(--border-subtle)',
                  padding: '13px 14px',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: 10,
                }}
              >
                {/* Top Row: Cluster Title, Speed Limit Sign, Cruise Badge */}
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                    <Gauge size={13} color="#38bdf8" />
                    <span
                      style={{
                        fontSize: 10,
                        fontWeight: 800,
                        color: '#38bdf8',
                        letterSpacing: '0.06em',
                      }}
                    >
                      DIGITAL CLUSTER
                    </span>
                  </div>

                  <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                    {speedLimit > 0 && (
                      <div
                        className="tabular-nums"
                        title="Speed Limit"
                        style={{
                          minWidth: 26,
                          height: 22,
                          padding: '0 5px',
                          borderRadius: 11,
                          backgroundColor: '#f8fafc',
                          border: '2.5px solid #ef4444',
                          color: '#020617',
                          fontSize: 10.5,
                          fontWeight: 900,
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'center',
                        }}
                      >
                        {speedLimit}
                      </div>
                    )}
                    <span
                      className="tabular-nums"
                      style={{
                        fontSize: 9.5,
                        fontWeight: 800,
                        color: telemetry.cruise_control > 0 ? '#34d399' : '#94a3b8',
                        backgroundColor:
                          telemetry.cruise_control > 0
                            ? 'rgba(52, 211, 153, 0.16)'
                            : 'rgba(148, 163, 184, 0.12)',
                        border:
                          telemetry.cruise_control > 0
                            ? '1px solid rgba(52, 211, 153, 0.35)'
                            : '1px solid rgba(255, 255, 255, 0.08)',
                        padding: '2px 7px',
                        borderRadius: 6,
                      }}
                    >
                      {telemetry.cruise_control > 0
                        ? `CC ${Math.round(telemetry.cruise_control)} km/h`
                        : 'CRUISE OFF'}
                    </span>
                  </div>
                </div>

                {/* Speed & Gear Display */}
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <div style={{ display: 'flex', alignItems: 'baseline', gap: 6 }}>
                    <span
                      className="tabular-nums"
                      style={{
                        fontSize: 46,
                        fontWeight: 900,
                        color: isOverLimit ? '#f87171' : '#ffffff',
                        lineHeight: 1,
                        letterSpacing: '-0.03em',
                      }}
                    >
                      {speedKmh}
                    </span>
                    <div style={{ display: 'flex', flexDirection: 'column' }}>
                      <span style={{ fontSize: 12, fontWeight: 800, color: '#94a3b8' }}>KM/H</span>
                      {isOverLimit && (
                        <span style={{ fontSize: 9, fontWeight: 800, color: '#f87171' }}>OVER LIMIT</span>
                      )}
                    </div>
                  </div>

                  {/* Gear Box */}
                  <div
                    className="tabular-nums"
                    style={{
                      minWidth: 66,
                      height: 46,
                      padding: '0 10px',
                      borderRadius: 12,
                      backgroundColor: 'var(--surface-card-elevated)',
                      border:
                        gearText === 'R' || gearText.startsWith('R')
                          ? '1.5px solid #f87171'
                          : gearText === 'N'
                          ? '1.5px solid #94a3b8'
                          : '1.5px solid #38bdf8',
                      display: 'flex',
                      flexDirection: 'column',
                      alignItems: 'center',
                      justifyContent: 'center',
                    }}
                  >
                    <span style={{ fontSize: 8.5, fontWeight: 700, color: '#94a3b8', letterSpacing: '0.06em' }}>
                      GEAR
                    </span>
                    <span
                      style={{
                        fontSize: 19,
                        fontWeight: 900,
                        lineHeight: 1.05,
                        color:
                          gearText === 'R' || gearText.startsWith('R')
                            ? '#f87171'
                            : gearText === 'N'
                            ? '#e2e8f0'
                            : '#38bdf8',
                      }}
                    >
                      {gearText}
                    </span>
                  </div>
                </div>

                {/* Eco-Band RPM Meter */}
                <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <span className="tabular-nums" style={{ fontSize: 11, fontWeight: 700, color: '#cbd5e1' }}>
                      {rpm} <span style={{ fontSize: 10, color: '#94a3b8', fontWeight: 600 }}>/ {rpmLimit} RPM</span>
                    </span>
                    <span
                      style={{
                        fontSize: 9.5,
                        fontWeight: 800,
                        color: rpmZoneColor,
                        letterSpacing: '0.04em',
                      }}
                    >
                      {rpmZoneLabel}
                    </span>
                  </div>

                  <div
                    style={{
                      width: '100%',
                      height: 8,
                      borderRadius: 4,
                      backgroundColor: 'rgba(15, 23, 42, 0.95)',
                      border: '1px solid rgba(255,255,255,0.06)',
                      overflow: 'hidden',
                      position: 'relative',
                    }}
                  >
                    {/* Eco Zone Marker (40% to 62%) */}
                    <div
                      style={{
                        position: 'absolute',
                        left: '40%',
                        width: '22%',
                        top: 0,
                        bottom: 0,
                        backgroundColor: 'rgba(52, 211, 153, 0.16)',
                      }}
                    />
                    <div
                      style={{
                        height: '100%',
                        borderRadius: 4,
                        width: `${rpmPct}%`,
                        backgroundColor: rpmZoneColor,
                        transition: 'width 0.1s ease',
                      }}
                    />
                  </div>
                </div>

                {/* Odometer & Est. Fuel Range Mini-Grid */}
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 8 }}>
                  <div
                    style={{
                      backgroundColor: 'var(--surface-card-elevated)',
                      borderRadius: 10,
                      padding: '6px 10px',
                      border: '1px solid rgba(255,255,255,0.06)',
                    }}
                  >
                    <div style={{ fontSize: 9.5, fontWeight: 700, color: '#94a3b8' }}>ODOMETER</div>
                    <div
                      className="tabular-nums"
                      style={{ fontSize: 12.5, fontWeight: 800, color: '#ffffff', marginTop: 1 }}
                    >
                      {Math.round(telemetry.odometer || 0).toLocaleString('id-ID')} km
                    </div>
                  </div>
                  <div
                    style={{
                      backgroundColor: 'var(--surface-card-elevated)',
                      borderRadius: 10,
                      padding: '6px 10px',
                      border: '1px solid rgba(255,255,255,0.06)',
                    }}
                  >
                    <div style={{ fontSize: 9.5, fontWeight: 700, color: '#94a3b8' }}>JARAK TEMPUH BBM</div>
                    <div
                      className="tabular-nums"
                      style={{
                        fontSize: 12.5,
                        fontWeight: 800,
                        color: fuelLow ? '#f87171' : '#34d399',
                        marginTop: 1,
                      }}
                    >
                      {fuelRangeKm > 0 ? `~${fuelRangeKm.toLocaleString('id-ID')} km` : `${fuelLiters} L`}
                    </div>
                  </div>
                </div>

                {/* 6 Telltale Dashboard Lamps */}
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(6, 1fr)', gap: 5 }}>
                  {telltaleLamps.map((lamp) => (
                    <div
                      key={lamp.sub}
                      style={{
                        padding: '5px 2px',
                        borderRadius: 8,
                        textAlign: 'center',
                        backgroundColor: lamp.active ? `${lamp.color}22` : 'rgba(15, 23, 42, 0.7)',
                        border: lamp.active
                          ? `1px solid ${lamp.color}`
                          : '1px solid rgba(255, 255, 255, 0.06)',
                        display: 'flex',
                        flexDirection: 'column',
                        alignItems: 'center',
                        gap: 1,
                      }}
                    >
                      <span
                        className="tabular-nums"
                        style={{
                          fontSize: 10,
                          fontWeight: 800,
                          color: lamp.active ? lamp.color : '#64748b',
                        }}
                      >
                        {lamp.label}
                      </span>
                      <span
                        style={{
                          fontSize: 7.5,
                          fontWeight: 700,
                          color: lamp.active ? '#e2e8f0' : '#475569',
                        }}
                      >
                        {lamp.sub}
                      </span>
                    </div>
                  ))}
                </div>
              </div>

              {/* 2. Navigation & GPS Card (matching tablet_gui.cpp:1998-2046) */}
              <div
                style={{
                  backgroundColor: 'var(--surface-card)',
                  borderRadius: 16,
                  border: '1px solid var(--border-subtle)',
                  padding: '12px 14px',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: 8,
                }}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                    <Compass size={13} color="#34d399" />
                    <span
                      style={{
                        fontSize: 10,
                        fontWeight: 800,
                        color: '#34d399',
                        letterSpacing: '0.06em',
                      }}
                    >
                      NAVIGATION & GPS TELEMETRY
                    </span>
                  </div>
                  <span
                    className="tabular-nums"
                    style={{
                      fontSize: 10.5,
                      fontWeight: 800,
                      color: '#38bdf8',
                      backgroundColor: 'rgba(56, 189, 248, 0.14)',
                      padding: '2px 7px',
                      borderRadius: 6,
                    }}
                  >
                    {cardinalDir} • {Math.round(headingDeg)}°
                  </span>
                </div>

                {telemetry.job_active ? (
                  <div
                    style={{
                      backgroundColor: 'var(--surface-card-elevated)',
                      borderRadius: 10,
                      padding: '8px 10px',
                      border: '1px solid rgba(255,255,255,0.06)',
                      display: 'flex',
                      flexDirection: 'column',
                      gap: 4,
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <span style={{ fontSize: 11.5, fontWeight: 700, color: '#ffffff' }}>
                        {telemetry.city_src || 'Origin'} → {telemetry.city_dst || 'Destination'}
                      </span>
                      <span
                        className="tabular-nums"
                        style={{ fontSize: 11, fontWeight: 800, color: '#fbbf24' }}
                      >
                        Sisa {Math.round(telemetry.nav_distance_km || 0)} km
                      </span>
                    </div>
                    <div style={{ fontSize: 10.5, color: '#94a3b8' }}>
                      Muatan: {telemetry.cargo || 'Cargo'} ({Math.round((telemetry.cargo_mass_kg || 0) / 1000)} T)
                    </div>
                  </div>
                ) : (
                  <div
                    style={{
                      backgroundColor: 'var(--surface-card-elevated)',
                      borderRadius: 10,
                      padding: '8px 10px',
                      border: '1px solid rgba(255,255,255,0.06)',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'space-between',
                    }}
                  >
                    <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                      <Navigation size={13} color="#38bdf8" />
                      <span style={{ fontSize: 11.5, fontWeight: 700, color: '#e2e8f0' }}>
                        Standby / Cab Free Roam
                      </span>
                    </div>
                    <span style={{ fontSize: 10, fontWeight: 700, color: '#34d399' }}>
                      {telemetry.server_mode || 'Singleplayer'}
                    </span>
                  </div>
                )}

                {/* World Coordinates */}
                <div
                  className="tabular-nums"
                  style={{
                    fontSize: 10,
                    fontFamily: 'var(--font-mono)',
                    color: '#94a3b8',
                    display: 'flex',
                    justifyContent: 'space-between',
                    paddingTop: 2,
                  }}
                >
                  <span>WORLD POS</span>
                  <span style={{ color: '#cbd5e1', fontWeight: 600 }}>
                    X: {Math.round(telemetry.pos_x || 0)} | Y: {Math.round(telemetry.pos_y || 0)} | Z:{' '}
                    {Math.round(telemetry.pos_z || 0)}
                  </span>
                </div>
              </div>
            </>
          )}

          {tab === 'powertrain' && (
            <>
              {/* 1. Active Vehicle Card (matching tablet_gui.cpp:2048-2075) */}
              <div
                style={{
                  backgroundColor: 'var(--surface-card)',
                  borderRadius: 15,
                  border: '1px solid var(--border-subtle)',
                  padding: '12px 13px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  gap: 10,
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: 10, minWidth: 0 }}>
                  <div
                    style={{
                      width: 36,
                      height: 36,
                      borderRadius: 10,
                      backgroundColor: hasMatchedFleet
                        ? 'rgba(52, 211, 153, 0.16)'
                        : 'rgba(56, 189, 248, 0.16)',
                      border: hasMatchedFleet
                        ? '1px solid rgba(52, 211, 153, 0.35)'
                        : '1px solid rgba(56, 189, 248, 0.35)',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      flexShrink: 0,
                    }}
                  >
                    <Truck size={18} color={hasMatchedFleet ? '#34d399' : '#38bdf8'} />
                  </div>
                  <div style={{ minWidth: 0 }}>
                    <div
                      style={{
                        fontSize: 10,
                        fontWeight: 800,
                        color: hasMatchedFleet ? '#34d399' : '#fbbf24',
                        letterSpacing: '0.04em',
                      }}
                    >
                      {hasMatchedFleet ? `ARMADA TERDAFTAR [${telemetry.fleet_number}]` : 'KENDARAAN SEWA / NON-FLEET'}
                    </div>
                    <div
                      style={{
                        fontSize: 12.5,
                        fontWeight: 800,
                        color: '#ffffff',
                        whiteSpace: 'nowrap',
                        overflow: 'hidden',
                        textOverflow: 'ellipsis',
                      }}
                    >
                      {baseTruck}
                    </div>
                  </div>
                </div>
                <span
                  style={{
                    fontSize: 9.5,
                    fontWeight: 800,
                    padding: '3px 7px',
                    borderRadius: 6,
                    backgroundColor: telemetry.engine_enabled
                      ? 'rgba(52, 211, 153, 0.16)'
                      : 'rgba(248, 113, 113, 0.16)',
                    color: telemetry.engine_enabled ? '#34d399' : '#f87171',
                    flexShrink: 0,
                  }}
                >
                  {telemetry.engine_enabled ? 'RUNNING' : 'ENGINE OFF'}
                </span>
              </div>

              {/* 2. Fuel Tank & Consumption Card (matching tablet_gui.cpp:2077-2114) */}
              <div
                style={{
                  backgroundColor: 'var(--surface-card)',
                  borderRadius: 15,
                  border: '1px solid var(--border-subtle)',
                  padding: '12px 13px',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: 8,
                }}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                    <Fuel size={14} color={fuelLow ? '#f87171' : '#34d399'} />
                    <span
                      style={{
                        fontSize: 10.5,
                        fontWeight: 800,
                        color: fuelLow ? '#f87171' : '#34d399',
                        letterSpacing: '0.05em',
                      }}
                    >
                      TANGKI BBM & KONSUMSI
                    </span>
                  </div>
                  {fuelLow ? (
                    <span
                      style={{
                        fontSize: 9.5,
                        fontWeight: 800,
                        padding: '2px 7px',
                        borderRadius: 6,
                        backgroundColor: 'rgba(248, 113, 113, 0.2)',
                        color: '#f87171',
                      }}
                    >
                      ! LOW FUEL ({fuelPct}%)
                    </span>
                  ) : (
                    <span className="tabular-nums" style={{ fontSize: 11, fontWeight: 800, color: '#34d399' }}>
                      {fuelPct}%
                    </span>
                  )}
                </div>

                <div
                  className="tabular-nums"
                  style={{ display: 'flex', alignItems: 'baseline', justifyContent: 'space-between' }}
                >
                  <div>
                    <span style={{ fontSize: 20, fontWeight: 800, color: '#ffffff' }}>{fuelLiters}</span>
                    <span style={{ fontSize: 11.5, color: '#94a3b8', fontWeight: 600 }}> / {fuelCap} Liter</span>
                  </div>
                  <span style={{ fontSize: 11, color: '#cbd5e1', fontWeight: 600 }}>
                    Est. Range: <strong style={{ color: '#38bdf8' }}>{fuelRangeKm} km</strong>
                  </span>
                </div>

                <div
                  style={{
                    width: '100%',
                    height: 7,
                    borderRadius: 4,
                    backgroundColor: 'rgba(15, 23, 42, 0.9)',
                    overflow: 'hidden',
                  }}
                >
                  <div
                    style={{
                      width: `${fuelPct}%`,
                      height: '100%',
                      borderRadius: 4,
                      backgroundColor: fuelLow ? '#ef4444' : '#10b981',
                    }}
                  />
                </div>

                <div
                  style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    fontSize: 10.5,
                    color: '#94a3b8',
                    paddingTop: 2,
                  }}
                >
                  <span>Rata-Rata Konsumsi:</span>
                  <span className="tabular-nums" style={{ fontWeight: 700, color: '#f8fafc' }}>
                    {fuelAvgL100 > 0 ? `${fuelAvgL100.toFixed(1)} L / 100 km` : 'Menghitung...'}
                  </span>
                </div>
              </div>

              {/* 3. 2x2 Sensor & Electrical Grid (matching tablet_gui.cpp:2116-2156) */}
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 8 }}>
                {[
                  {
                    label: 'OIL TEMP',
                    value: `${Math.round(telemetry.oil_temperature || 0)} °C`,
                    sub: `Press: ${(telemetry.oil_pressure || 0).toFixed(1)} bar`,
                    warn: (telemetry.oil_temperature || 0) > 115,
                    icon: Thermometer,
                    color: '#fbbf24',
                  },
                  {
                    label: 'COOLANT TEMP',
                    value: `${Math.round(telemetry.water_temperature || 0)} °C`,
                    sub: (telemetry.water_temperature || 0) > 102 ? 'OVERHEAT!' : 'Suhu Normal',
                    warn: (telemetry.water_temperature || 0) > 102,
                    icon: Thermometer,
                    color: '#38bdf8',
                  },
                  {
                    label: 'BRAKE AIR',
                    value: `${Math.round(airPsi)} PSI`,
                    sub: airPsi < 65 && airPsi > 0 ? 'TEKANAN RENDAH!' : 'Sistem Rem Aman',
                    warn: airPsi < 65 && airPsi > 0,
                    icon: Gauge,
                    color: '#34d399',
                  },
                  {
                    label: 'BATTERY',
                    value: `${(telemetry.battery_voltage || 24.0).toFixed(1)} V`,
                    sub: 'Sistem Kelistrikan 24V',
                    warn: (telemetry.battery_voltage || 24) < 22,
                    icon: Zap,
                    color: '#c084fc',
                  },
                ].map((sensor) => {
                  const Icon = sensor.icon;
                  return (
                    <div
                      key={sensor.label}
                      style={{
                        backgroundColor: 'var(--surface-card)',
                        borderRadius: 14,
                        border: sensor.warn
                          ? '1px solid rgba(248, 113, 113, 0.5)'
                          : '1px solid var(--border-subtle)',
                        padding: '10px 11px',
                        display: 'flex',
                        flexDirection: 'column',
                        gap: 4,
                      }}
                    >
                      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                        <span
                          style={{
                            fontSize: 9.5,
                            fontWeight: 800,
                            color: sensor.warn ? '#f87171' : sensor.color,
                            letterSpacing: '0.04em',
                          }}
                        >
                          {sensor.label}
                        </span>
                        <Icon size={12} color={sensor.warn ? '#f87171' : sensor.color} />
                      </div>
                      <div
                        className="tabular-nums"
                        style={{
                          fontSize: 16.5,
                          fontWeight: 800,
                          color: sensor.warn ? '#f87171' : '#ffffff',
                        }}
                      >
                        {sensor.value}
                      </div>
                      <div style={{ fontSize: 9.5, color: sensor.warn ? '#fca5a5' : '#94a3b8' }}>
                        {sensor.sub}
                      </div>
                    </div>
                  );
                })}
              </div>
            </>
          )}

          {tab === 'diagnostik' && (
            <>
              {/* 1. Structural Health Hero Card (matching tablet_gui.cpp:2158-2190) */}
              <div
                style={{
                  backgroundColor: 'var(--surface-card)',
                  borderRadius: 16,
                  border: '1px solid var(--border-subtle)',
                  borderLeft: `4px solid ${healthColor}`,
                  padding: '13px 14px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                }}
              >
                <div>
                  <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                    {overallHealthPct >= 90 ? (
                      <ShieldCheck size={14} color={healthColor} />
                    ) : (
                      <ShieldAlert size={14} color={healthColor} />
                    )}
                    <span
                      style={{
                        fontSize: 10,
                        fontWeight: 800,
                        color: healthColor,
                        letterSpacing: '0.05em',
                      }}
                    >
                      STRUCTURAL HEALTH
                    </span>
                  </div>
                  <div style={{ fontSize: 13.5, fontWeight: 800, color: '#ffffff', marginTop: 3 }}>
                    {healthStatus}
                  </div>
                  <div style={{ fontSize: 10.5, color: '#94a3b8', marginTop: 1 }}>
                    Keausan tertinggi komponen: {maxWearPct.toFixed(1)}%
                  </div>
                </div>

                <div
                  className="tabular-nums"
                  style={{
                    fontSize: 28,
                    fontWeight: 900,
                    color: healthColor,
                    letterSpacing: '-0.02em',
                  }}
                >
                  {Math.round(overallHealthPct)}%
                </div>
              </div>

              {/* 2. 5 Mechanical Parts Wear Card */}
              <div
                style={{
                  backgroundColor: 'var(--surface-card)',
                  borderRadius: 16,
                  border: '1px solid var(--border-subtle)',
                  padding: '13px 14px',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: 10,
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                  <Wrench size={13} color="#38bdf8" />
                  <span
                    style={{
                      fontSize: 10,
                      fontWeight: 800,
                      color: '#38bdf8',
                      letterSpacing: '0.05em',
                    }}
                  >
                    DIAGNOSTIK 5 KOMPONEN TRUK (IN-GAME WEAR)
                  </span>
                </div>

                {[
                  { label: 'Mesin (Engine)', wear: wearEngine },
                  { label: 'Transmisi (Gearbox)', wear: wearTrans },
                  { label: 'Kabin (Cabin)', wear: wearCabin },
                  { label: 'Sasis (Chassis)', wear: wearChassis },
                  { label: 'Roda & Ban (Wheels)', wear: wearWheels },
                ].map((part) => {
                  const barColor =
                    part.wear > 15 ? '#ef4444' : part.wear > 5 ? '#fbbf24' : '#34d399';
                  return (
                    <div key={part.label}>
                      <div
                        style={{
                          display: 'flex',
                          justifyContent: 'space-between',
                          fontSize: 11,
                          color: '#cbd5e1',
                          marginBottom: 4,
                        }}
                      >
                        <span style={{ fontWeight: 600 }}>{part.label}</span>
                        <span className="tabular-nums" style={{ fontWeight: 800, color: barColor }}>
                          {part.wear.toFixed(1)}% Wear
                        </span>
                      </div>
                      <div
                        style={{
                          width: '100%',
                          height: 5,
                          borderRadius: 3,
                          backgroundColor: 'rgba(15, 23, 42, 0.9)',
                          overflow: 'hidden',
                        }}
                      >
                        <div
                          style={{
                            height: '100%',
                            borderRadius: 3,
                            width: `${Math.min(100, Math.max(part.wear > 0 ? 2 : 0, part.wear))}%`,
                            backgroundColor: barColor,
                          }}
                        />
                      </div>
                    </div>
                  );
                })}
              </div>

              {/* 3. Trailer & Cargo Integrity Card */}
              <div
                style={{
                  backgroundColor: 'var(--surface-card)',
                  borderRadius: 16,
                  border: '1px solid var(--border-subtle)',
                  padding: '12px 14px',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: 8,
                }}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <span
                    style={{
                      fontSize: 10,
                      fontWeight: 800,
                      color: '#fbbf24',
                      letterSpacing: '0.05em',
                    }}
                  >
                    TRAILER & MUATAN KARGO
                  </span>
                  <span
                    style={{
                      fontSize: 9.5,
                      fontWeight: 800,
                      padding: '2px 7px',
                      borderRadius: 6,
                      backgroundColor: telemetry.trailer_connected
                        ? 'rgba(52, 211, 153, 0.16)'
                        : 'rgba(148, 163, 184, 0.14)',
                      color: telemetry.trailer_connected ? '#34d399' : '#94a3b8',
                    }}
                  >
                    {telemetry.trailer_connected ? 'TRAILER ATTACHED' : 'NO TRAILER'}
                  </span>
                </div>

                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 8 }}>
                  <div
                    style={{
                      backgroundColor: 'var(--surface-card-elevated)',
                      borderRadius: 10,
                      padding: '7px 10px',
                    }}
                  >
                    <div style={{ fontSize: 9.5, color: '#94a3b8', fontWeight: 600 }}>KERUSAKAN TRAILER</div>
                    <div
                      className="tabular-nums"
                      style={{
                        fontSize: 13,
                        fontWeight: 800,
                        color: (telemetry.wear_trailer || 0) > 0.05 ? '#f87171' : '#34d399',
                        marginTop: 1,
                      }}
                    >
                      {((telemetry.wear_trailer || 0) * 100).toFixed(1)}%
                    </div>
                  </div>
                  <div
                    style={{
                      backgroundColor: 'var(--surface-card-elevated)',
                      borderRadius: 10,
                      padding: '7px 10px',
                    }}
                  >
                    <div style={{ fontSize: 9.5, color: '#94a3b8', fontWeight: 600 }}>KERUSAKAN KARGO</div>
                    <div
                      className="tabular-nums"
                      style={{
                        fontSize: 13,
                        fontWeight: 800,
                        color: (telemetry.cargo_damage || 0) > 0.01 ? '#f87171' : '#34d399',
                        marginTop: 1,
                      }}
                    >
                      {((telemetry.cargo_damage || 0) * 100).toFixed(1)}%
                    </div>
                  </div>
                </div>
              </div>
            </>
          )}
        </div>
      </div>
    </div>
  );
};
