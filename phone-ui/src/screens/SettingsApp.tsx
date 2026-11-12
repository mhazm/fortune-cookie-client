import React, { useState } from 'react';
import { TelemetryData } from '../types/telemetry';
import {
  ChevronLeft,
  RefreshCw,
  Eye,
  EyeOff,
  ShieldCheck,
  ShieldAlert,
  CheckCircle2,
  XCircle,
  Smartphone,
  Lock,
  KeyRound,
  Sliders,
  AlertTriangle,
} from 'lucide-react';

interface Props {
  telemetry: TelemetryData;
  onBack: () => void;
}

export const SettingsApp: React.FC<Props> = ({ telemetry, onBack }) => {
  const [tab, setTab] = useState<'system' | 'hardcore'>('system');
  const [showToken, setShowToken] = useState(false);
  const [isSyncing, setIsSyncing] = useState(false);
  const [isScanning, setIsScanning] = useState(false);

  const autoLock = telemetry.auto_lock_on_close !== false;
  const rawToken = telemetry.driver_token || '';
  const hasToken = rawToken.length > 0;
  const maskedToken = hasToken
    ? rawToken.length > 8
      ? `${rawToken.slice(0, 8)}••••••••••••`
      : 'NSM-••••••••••••'
    : 'Belum Dikonfigurasi';

  const isAuth = telemetry.account_authenticated !== false && Boolean(telemetry.driver_name);
  const osVersion = telemetry.os_version || 'Nismara OS v1.0.2';
  const buildVersion = telemetry.build_version || 'Build 1.0.2-x64';
  const pluginVersion = telemetry.plugin_version || '1.0.2';

  // 9 Hardcore Simulation Parameters from ProfileValidator (tablet_gui.cpp:3415-3484)
  const hcChecks = [
    {
      label: 'Traffic Aktif',
      cvar: 'g_traffic >= 1',
      ok: telemetry.hc_traffic_ok !== false,
    },
    {
      label: 'Simulasi Kelelahan',
      cvar: 'g_fatigue = 1',
      ok: telemetry.hc_fatigue_ok !== false,
    },
    {
      label: 'Pelanggaran / Polisi',
      cvar: 'g_police = 1',
      ok: telemetry.hc_police_ok !== false,
    },
    {
      label: 'Kesulitan Parkir',
      cvar: 'Skilled Parking',
      ok: telemetry.hc_parking_ok !== false,
    },
    {
      label: 'Road Events',
      cvar: 'Min. 10%',
      ok: telemetry.hc_road_events_ok !== false,
    },
    {
      label: 'Probabilitas Hujan',
      cvar: 'Min. 10%',
      ok: telemetry.hc_rain_prob_ok !== false,
    },
    {
      label: 'Advanced Trailer Coupling',
      cvar: 'Coupling = 1',
      ok: telemetry.hc_advanced_coupling_ok !== false,
    },
    {
      label: 'Konsumsi BBM Realistis',
      cvar: 'Realistic Fuel = 1',
      ok: telemetry.hc_fuel_simulation_ok !== false,
    },
    {
      label: 'Simulasi Rem Angin',
      cvar: 'Air Brakes = 1',
      ok: telemetry.hc_air_brake_ok !== false,
    },
  ];

  const passedCount = hcChecks.filter((c) => c.ok).length;
  const forbiddenModsFound = Boolean(telemetry.hc_forbidden_mods_found);
  const forbiddenModsList = telemetry.hc_forbidden_mods_list || '';
  const hardcoreEligible = passedCount === hcChecks.length && !forbiddenModsFound;

  const handleToggleAutoLock = () => {
    window.nismara?.setAutoLock?.(!autoLock);
  };

  const handleSyncAccount = () => {
    setIsSyncing(true);
    window.nismara?.syncAccount?.();
    setTimeout(() => setIsSyncing(false), 700);
  };

  const handleRescanProfile = () => {
    setIsScanning(true);
    window.nismara?.rescanProfile?.();
    setTimeout(() => setIsScanning(false), 600);
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
      {/* Navigation Bar */}
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
          Settings
        </span>
        <span
          className="tabular-nums"
          style={{
            fontSize: 10,
            fontWeight: 700,
            fontFamily: 'var(--font-mono)',
            color: '#94a3b8',
            backgroundColor: 'rgba(30, 41, 59, 0.7)',
            padding: '2px 6px',
            borderRadius: 6,
            border: '1px solid rgba(255, 255, 255, 0.08)',
          }}
        >
          v{pluginVersion}
        </span>
      </div>

      {/* Segmented Control: [Sistem & Akun] | [Diagnostik Hardcore] */}
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
          onClick={() => setTab('system')}
          style={{
            padding: '6px 0',
            borderRadius: 8,
            fontSize: 11,
            fontWeight: tab === 'system' ? 700 : 500,
            color: tab === 'system' ? '#020617' : '#94a3b8',
            backgroundColor: tab === 'system' ? '#38bdf8' : 'transparent',
            border: 'none',
            cursor: 'pointer',
            transition: 'all 0.16s ease',
          }}
        >
          Sistem & Akun
        </button>
        <button
          onClick={() => setTab('hardcore')}
          style={{
            padding: '6px 0',
            borderRadius: 8,
            fontSize: 11,
            fontWeight: tab === 'hardcore' ? 700 : 500,
            color: tab === 'hardcore' ? '#020617' : '#94a3b8',
            backgroundColor: tab === 'hardcore' ? '#38bdf8' : 'transparent',
            border: 'none',
            cursor: 'pointer',
            transition: 'all 0.16s ease',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: 4,
          }}
        >
          <span>Hardcore ({passedCount}/9)</span>
        </button>
      </div>

      {/* Scrollable Content */}
      <div data-scroll-container="true" className="scroll-area">
        <div
          style={{
            display: 'flex',
            flexDirection: 'column',
            gap: 11,
            paddingBottom: 24,
          }}
        >
        {tab === 'system' ? (
          <>
            {/* 1. PERILAKU & SISTEM (tablet_gui.cpp:3400-3413) */}
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
              <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                <Lock size={13} style={{ color: '#38bdf8' }} />
                <span
                  style={{
                    fontSize: 10.5,
                    fontWeight: 800,
                    color: '#38bdf8',
                    letterSpacing: '0.05em',
                  }}
                >
                  PERILAKU & SISTEM
                </span>
              </div>

              <div
                onClick={handleToggleAutoLock}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  gap: 10,
                  cursor: 'pointer',
                  paddingTop: 2,
                }}
              >
                <div style={{ flex: 1 }}>
                  <div style={{ fontSize: 12, fontWeight: 700, color: '#f8fafc' }}>
                    Selalu buka di Lock Screen (F11)
                  </div>
                  <div style={{ fontSize: 10.5, color: '#94a3b8', marginTop: 2, lineHeight: 1.3 }}>
                    Kunci otomatis layar Companion Phone saat ditutup dengan F11.
                  </div>
                </div>

                {/* Native Toggle Switch */}
                <button
                  type="button"
                  aria-label="Toggle Auto Lock"
                  style={{
                    width: 42,
                    height: 24,
                    borderRadius: 12,
                    backgroundColor: autoLock ? '#10b981' : '#334155',
                    border: '1px solid rgba(255,255,255,0.14)',
                    padding: 2,
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: autoLock ? 'flex-end' : 'flex-start',
                    cursor: 'pointer',
                    flexShrink: 0,
                    transition: 'background-color 0.18s ease',
                  }}
                >
                  <div
                    style={{
                      width: 18,
                      height: 18,
                      borderRadius: 9,
                      backgroundColor: '#ffffff',
                      boxShadow: '0 2px 5px rgba(0,0,0,0.35)',
                    }}
                  />
                </button>
              </div>
            </div>

            {/* 2. AUTENTIKASI & AKUN DRIVER (tablet_gui.cpp:3486-3530) */}
            <div
              style={{
                backgroundColor: 'var(--surface-card)',
                borderRadius: 15,
                border: '1px solid var(--border-subtle)',
                padding: '12px 13px',
                display: 'flex',
                flexDirection: 'column',
                gap: 9,
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                  <KeyRound size={13} style={{ color: '#facc15' }} />
                  <span
                    style={{
                      fontSize: 10.5,
                      fontWeight: 800,
                      color: '#facc15',
                      letterSpacing: '0.05em',
                    }}
                  >
                    AUTENTIKASI & AKUN DRIVER
                  </span>
                </div>
                <span
                  style={{
                    fontSize: 9.5,
                    fontWeight: 800,
                    padding: '2px 7px',
                    borderRadius: 6,
                    backgroundColor: isAuth ? 'rgba(16, 185, 129, 0.16)' : 'rgba(245, 158, 11, 0.16)',
                    color: isAuth ? '#34d399' : '#fbbf24',
                    border: isAuth
                      ? '1px solid rgba(52, 211, 153, 0.35)'
                      : '1px solid rgba(251, 191, 36, 0.35)',
                  }}
                >
                  {isAuth ? 'TERHUBUNG' : 'BELUM SINKRON'}
                </span>
              </div>

              {/* Driver & Status Row */}
              <div
                style={{
                  backgroundColor: 'var(--surface-card-elevated)',
                  borderRadius: 10,
                  padding: '8px 10px',
                  border: '1px solid rgba(255, 255, 255, 0.06)',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: 5,
                }}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <span style={{ fontSize: 10.5, color: '#94a3b8' }}>Akun Driver</span>
                  <span style={{ fontSize: 11.5, fontWeight: 700, color: '#ffffff' }}>
                    {telemetry.driver_name || 'Belum Terhubung'}
                  </span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <span style={{ fontSize: 10.5, color: '#94a3b8' }}>Status Sinkronisasi</span>
                  <span
                    className="tabular-nums"
                    style={{
                      fontSize: 10.5,
                      fontWeight: 600,
                      color: isAuth ? '#34d399' : '#fbbf24',
                    }}
                  >
                    {telemetry.last_sync_timestamp
                      ? `Sinkron (${telemetry.last_sync_timestamp})`
                      : telemetry.account_status_message || 'Menunggu Sinkronisasi'}
                  </span>
                </div>
              </div>

              {/* Token Row with Show/Hide */}
              <div>
                <div
                  style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    marginBottom: 4,
                  }}
                >
                  <span style={{ fontSize: 10.5, color: '#94a3b8', fontWeight: 600 }}>
                    Driver Token (nismara_config.dat)
                  </span>
                  {hasToken && (
                    <button
                      onClick={() => setShowToken(!showToken)}
                      style={{
                        display: 'inline-flex',
                        alignItems: 'center',
                        gap: 3,
                        fontSize: 10,
                        fontWeight: 700,
                        color: '#38bdf8',
                        background: 'none',
                        border: 'none',
                        cursor: 'pointer',
                        padding: 0,
                      }}
                    >
                      {showToken ? <EyeOff size={11} /> : <Eye size={11} />}
                      <span>{showToken ? 'Hide' : 'Show'}</span>
                    </button>
                  )}
                </div>
                <div
                  style={{
                    backgroundColor: 'rgba(2, 6, 23, 0.75)',
                    borderRadius: 8,
                    padding: '6px 9px',
                    border: '1px solid rgba(255, 255, 255, 0.08)',
                    fontFamily: 'var(--font-mono)',
                    fontSize: 10.5,
                    color: hasToken ? '#e2e8f0' : '#f87171',
                    wordBreak: 'break-all',
                  }}
                >
                  {showToken && hasToken ? rawToken : maskedToken}
                </div>
              </div>

              {/* Action Button: Sinkronkan Ulang Akun */}
              <button
                onClick={handleSyncAccount}
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  borderRadius: 10,
                  backgroundColor: 'rgba(56, 189, 248, 0.16)',
                  border: '1px solid rgba(56, 189, 248, 0.35)',
                  color: '#38bdf8',
                  fontSize: 11.5,
                  fontWeight: 700,
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: 6,
                }}
              >
                <RefreshCw size={13} className={isSyncing ? 'animate-spin' : ''} />
                <span>{isSyncing ? 'Menyinkronkan Akun...' : 'Sinkronkan Ulang Akun'}</span>
              </button>
            </div>

            {/* 3. TENTANG COMPANION PHONE (tablet_gui.cpp:3532-3550) */}
            <div
              style={{
                backgroundColor: 'var(--surface-card)',
                borderRadius: 15,
                border: '1px solid var(--border-subtle)',
                padding: '12px 13px',
                display: 'flex',
                flexDirection: 'column',
                gap: 7,
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: 6, marginBottom: 1 }}>
                <Smartphone size={13} style={{ color: '#a78bfa' }} />
                <span
                  style={{
                    fontSize: 10.5,
                    fontWeight: 800,
                    color: '#a78bfa',
                    letterSpacing: '0.05em',
                  }}
                >
                  TENTANG COMPANION PHONE
                </span>
              </div>

              {[
                { k: 'Perangkat', v: 'Companion Phone', mono: false },
                { k: 'Sistem Operasi', v: osVersion, mono: false },
                { k: 'Versi Build', v: buildVersion, mono: true },
                {
                  k: 'Game Simulasi',
                  v: telemetry.game_name || 'Euro Truck Simulator 2',
                  mono: false,
                },
                {
                  k: 'Mode Sesi',
                  v: telemetry.server_mode || 'Singleplayer',
                  mono: false,
                },
                { k: 'Telemetry SDK', v: 'SCS Telemetry SDK v1.01', mono: true },
              ].map((row, idx) => (
                <div
                  key={row.k}
                  style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    paddingTop: idx === 0 ? 0 : 5,
                    borderTop: idx === 0 ? 'none' : '1px solid rgba(255, 255, 255, 0.06)',
                    fontSize: 11,
                  }}
                >
                  <span style={{ color: '#94a3b8' }}>{row.k}</span>
                  <span
                    className={row.mono ? 'tabular-nums' : undefined}
                    style={{
                      fontWeight: 700,
                      color: '#f8fafc',
                      fontFamily: row.mono ? 'var(--font-mono)' : 'var(--font-sans)',
                      fontSize: row.mono ? 10.5 : 11,
                    }}
                  >
                    {row.v}
                  </span>
                </div>
              ))}
            </div>
          </>
        ) : (
          <>
            {/* TAB 2: DIAGNOSTIK SIMULASI HARDCORE (tablet_gui.cpp:3415-3484) */}
            <div
              style={{
                backgroundColor: 'var(--surface-card)',
                borderRadius: 15,
                border: '1px solid var(--border-subtle)',
                borderLeft: hardcoreEligible ? '4px solid #10b981' : '4px solid #f43f5e',
                padding: '12px 13px',
                display: 'flex',
                flexDirection: 'column',
                gap: 8,
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                  {hardcoreEligible ? (
                    <ShieldCheck size={15} style={{ color: '#34d399' }} />
                  ) : (
                    <ShieldAlert size={15} style={{ color: '#fb7185' }} />
                  )}
                  <span
                    style={{
                      fontSize: 10.5,
                      fontWeight: 800,
                      color: hardcoreEligible ? '#34d399' : '#fb7185',
                      letterSpacing: '0.05em',
                    }}
                  >
                    STATUS SIMULASI HARDCORE
                  </span>
                </div>

                <span
                  className="tabular-nums"
                  style={{
                    fontSize: 9.5,
                    fontWeight: 800,
                    padding: '2px 7px',
                    borderRadius: 6,
                    backgroundColor: hardcoreEligible
                      ? 'rgba(16, 185, 129, 0.18)'
                      : 'rgba(244, 63, 94, 0.18)',
                    color: hardcoreEligible ? '#34d399' : '#fb7185',
                    border: hardcoreEligible
                      ? '1px solid rgba(52, 211, 153, 0.35)'
                      : '1px solid rgba(251, 113, 133, 0.35)',
                  }}
                >
                  {hardcoreEligible ? 'ELIGIBLE' : 'TIDAK VALID'}
                </span>
              </div>

              <div style={{ fontSize: 11, color: '#cbd5e1', lineHeight: 1.35 }}>
                {hardcoreEligible
                  ? 'Seluruh 9 parameter konfigurasi profil game memenuhi standar Hardcore Nismara.'
                  : `Terpenuhi ${passedCount} dari 9 parameter wajib. Sesuaikan pengaturan gameplay atau config.cfg lalu pindai ulang.`}
              </div>

              <button
                onClick={handleRescanProfile}
                style={{
                  width: '100%',
                  padding: '7px 10px',
                  borderRadius: 9,
                  backgroundColor: 'rgba(56, 189, 248, 0.15)',
                  border: '1px solid rgba(56, 189, 248, 0.35)',
                  color: '#38bdf8',
                  fontSize: 11,
                  fontWeight: 700,
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: 6,
                }}
              >
                <RefreshCw size={12} className={isScanning ? 'animate-spin' : ''} />
                <span>
                  {isScanning ? 'Memindai Profil Game...' : 'Pindai Ulang Profil Game (Rescan)'}
                </span>
              </button>
            </div>

            {/* Forbidden Mods Status */}
            <div
              style={{
                backgroundColor: 'var(--surface-card)',
                borderRadius: 13,
                border: '1px solid var(--border-subtle)',
                padding: '10px 12px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                gap: 8,
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                {forbiddenModsFound ? (
                  <AlertTriangle size={15} style={{ color: '#fb7185', flexShrink: 0 }} />
                ) : (
                  <CheckCircle2 size={15} style={{ color: '#34d399', flexShrink: 0 }} />
                )}
                <div>
                  <div style={{ fontSize: 11.5, fontWeight: 700, color: '#ffffff' }}>
                    Validasi Mod Game
                  </div>
                  <div
                    style={{
                      fontSize: 10.5,
                      color: forbiddenModsFound ? '#fb7185' : '#34d399',
                      marginTop: 1,
                    }}
                  >
                    {forbiddenModsFound
                      ? `Mod Terlarang: ${forbiddenModsList || 'Terdeteksi'}`
                      : 'Bebas dari mod terlarang'}
                  </div>
                </div>
              </div>
              <span
                style={{
                  fontSize: 9.5,
                  fontWeight: 800,
                  color: forbiddenModsFound ? '#fb7185' : '#34d399',
                }}
              >
                {forbiddenModsFound ? 'FAIL' : 'PASS'}
              </span>
            </div>

            {/* 9 Parameter Checklist */}
            <div
              style={{
                backgroundColor: 'var(--surface-card)',
                borderRadius: 15,
                border: '1px solid var(--border-subtle)',
                padding: '11px 12px',
                display: 'flex',
                flexDirection: 'column',
                gap: 6,
              }}
            >
              <div
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  marginBottom: 2,
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: 5 }}>
                  <Sliders size={12} style={{ color: '#38bdf8' }} />
                  <span
                    style={{
                      fontSize: 10,
                      fontWeight: 800,
                      color: '#38bdf8',
                      letterSpacing: '0.05em',
                    }}
                  >
                    PARAMETER SIMULASI (9 CVARS)
                  </span>
                </div>
                <span
                  className="tabular-nums"
                  style={{ fontSize: 10, fontWeight: 700, color: '#94a3b8' }}
                >
                  {passedCount}/9 OK
                </span>
              </div>

              {hcChecks.map((item, idx) => (
                <div
                  key={item.label}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    paddingTop: idx === 0 ? 2 : 6,
                    borderTop: idx === 0 ? 'none' : '1px solid rgba(255, 255, 255, 0.06)',
                  }}
                >
                  <div>
                    <div style={{ fontSize: 11, fontWeight: 600, color: '#f1f5f9' }}>
                      {item.label}
                    </div>
                    <div
                      style={{
                        fontSize: 9.5,
                        color: '#64748b',
                        fontFamily: 'var(--font-mono)',
                      }}
                    >
                      {item.cvar}
                    </div>
                  </div>

                  <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                    {item.ok ? (
                      <>
                        <CheckCircle2 size={13} style={{ color: '#34d399' }} />
                        <span style={{ fontSize: 10, fontWeight: 700, color: '#34d399' }}>OK</span>
                      </>
                    ) : (
                      <>
                        <XCircle size={13} style={{ color: '#fb7185' }} />
                        <span style={{ fontSize: 10, fontWeight: 700, color: '#fb7185' }}>
                          OFF
                        </span>
                      </>
                    )}
                  </div>
                </div>
              ))}
            </div>
          </>
        )}
        </div>
      </div>
    </div>
  );
};
