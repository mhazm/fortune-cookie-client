import React, { useState } from 'react';
import { TelemetryData } from '../types/telemetry';
import {
  ShieldCheck,
  Sparkles,
  Award,
  RefreshCw,
  Wallet,
  Zap,
  Users,
  AlertTriangle,
} from 'lucide-react';

interface Props {
  telemetry: TelemetryData;
  onBack: () => void;
  onOpenSettings?: () => void;
}

export const NismaraIdApp: React.FC<Props> = ({ telemetry, onBack, onOpenSettings }) => {
  const [avatarError, setAvatarError] = useState(false);
  const isAuthenticated = telemetry.account_authenticated !== false && Boolean(telemetry.driver_name && telemetry.driver_name !== 'Driver');
  const penaltyPts = telemetry.penalty_points || 0;
  const driverInitials = (telemetry.driver_name || 'ND')
    .split(/\s+/)
    .filter(Boolean)
    .slice(0, 2)
    .map((part) => part[0]?.toUpperCase() || '')
    .join('') || 'ND';

  let penaltyStatusLabel = 'Aman';
  let penaltyBadgeStyle = {
    bg: 'rgba(52, 211, 153, 0.16)',
    border: 'rgba(52, 211, 153, 0.35)',
    text: '#34d399',
  };
  if (penaltyPts >= 50) {
    penaltyStatusLabel = 'P3 Kritis';
    penaltyBadgeStyle = {
      bg: 'rgba(248, 113, 113, 0.18)',
      border: 'rgba(248, 113, 113, 0.4)',
      text: '#f87171',
    };
  } else if (penaltyPts >= 25) {
    penaltyStatusLabel = 'P2 Keras';
    penaltyBadgeStyle = {
      bg: 'rgba(251, 146, 60, 0.18)',
      border: 'rgba(251, 146, 60, 0.4)',
      text: '#fb923c',
    };
  } else if (penaltyPts >= 10) {
    penaltyStatusLabel = 'P1 Peringatan';
    penaltyBadgeStyle = {
      bg: 'rgba(251, 191, 36, 0.18)',
      border: 'rgba(251, 191, 36, 0.4)',
      text: '#fbbf24',
    };
  }

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
        userSelect: 'none',
      }}
    >
      {/* Navigation Header */}
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
            fontSize: 12,
            fontWeight: 600,
            color: '#c084fc',
            background: 'none',
            border: 'none',
            cursor: 'pointer',
            padding: '4px 0',
          }}
        >
          ‹ Home
        </button>
        <span style={{ fontSize: 13.5, fontWeight: 700, color: '#ffffff', letterSpacing: '-0.01em' }}>
          Nismara ID
        </span>
        <button
          onClick={() => window.nismara?.syncAccount?.()}
          title="Sinkronkan Profil & Saldo"
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            width: 28,
            height: 28,
            borderRadius: 8,
            backgroundColor: 'rgba(192, 132, 252, 0.15)',
            border: '1px solid rgba(192, 132, 252, 0.32)',
            color: '#c084fc',
            cursor: 'pointer',
            padding: 0,
          }}
        >
          <RefreshCw size={13} />
        </button>
      </div>

      {/* Main Content */}
      <div data-scroll-container="true" className="scroll-area" style={{ paddingTop: 12 }}>
        <div
          style={{
            display: 'flex',
            flexDirection: 'column',
            gap: 10,
            paddingBottom: 24,
          }}
        >
        {!isAuthenticated ? (
          <div
            style={{
              backgroundColor: 'var(--surface-card)',
              borderRadius: 16,
              border: '1px solid rgba(251, 191, 36, 0.3)',
              padding: 16,
              display: 'flex',
              flexDirection: 'column',
              gap: 10,
            }}
          >
            <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
              <AlertTriangle size={16} color="#fbbf24" />
              <span style={{ fontSize: 11.5, fontWeight: 800, color: '#fbbf24', letterSpacing: '0.04em' }}>
                NISMARA ID BELUM TERHUBUNG
              </span>
            </div>
            <p style={{ fontSize: 11.5, color: '#cbd5e1', lineHeight: 1.45 }}>
              {telemetry.account_status_message ||
                'Akun driver belum disinkronkan. Pastikan Driver Token sudah terpasang pada pengaturan.'}
            </p>
            <div style={{ display: 'flex', flexDirection: 'column', gap: 8, marginTop: 4 }}>
              <button
                onClick={() => window.nismara?.syncAccount?.()}
                style={{
                  width: '100%',
                  padding: '9px 12px',
                  borderRadius: 10,
                  backgroundColor: '#7c3aed',
                  color: '#ffffff',
                  fontSize: 12,
                  fontWeight: 700,
                  border: 'none',
                  cursor: 'pointer',
                }}
              >
                Sinkronkan Sekarang
              </button>
              {onOpenSettings && (
                <button
                  onClick={onOpenSettings}
                  style={{
                    width: '100%',
                    padding: '8px 12px',
                    borderRadius: 10,
                    backgroundColor: 'rgba(255, 255, 255, 0.08)',
                    color: '#e2e8f0',
                    fontSize: 11.5,
                    fontWeight: 600,
                    border: '1px solid var(--border-subtle)',
                    cursor: 'pointer',
                  }}
                >
                  Buka Pengaturan Token
                </button>
              )}
            </div>
          </div>
        ) : (
          <>
            {/* 1. Digital Driver Pass Card (matching tablet_gui.cpp:2525-2565) */}
            <div
              style={{
                position: 'relative',
                borderRadius: 18,
                padding: 15,
                background: 'linear-gradient(135deg, #312e81 0%, #4c1d95 52%, #0f172a 100%)',
                border: '1px solid rgba(255, 255, 255, 0.2)',
                boxShadow: '0 10px 25px rgba(0, 0, 0, 0.45)',
                overflow: 'hidden',
                display: 'flex',
                flexDirection: 'column',
                gap: 10,
              }}
            >
              {/* Pass Header */}
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: 5 }}>
                  <Sparkles size={12} color="#fbbf24" />
                  <span
                    style={{
                      fontSize: 10,
                      fontWeight: 800,
                      letterSpacing: '0.07em',
                      color: '#ddd6fe',
                      textTransform: 'uppercase',
                    }}
                  >
                    Nismara Digital Driver Pass
                  </span>
                </div>
                <span
                  style={{
                    fontSize: 9.5,
                    fontWeight: 800,
                    padding: '2px 7px',
                    borderRadius: 6,
                    backgroundColor: 'rgba(255, 255, 255, 0.14)',
                    border: '1px solid rgba(255, 255, 255, 0.22)',
                    color: '#fde68a',
                  }}
                >
                  VERIFIED
                </span>
              </div>

              {/* Driver Identity with Circular Avatar & Role/Rank */}
              <div style={{ display: 'flex', alignItems: 'center', gap: 11 }}>
                <div
                  style={{
                    width: 46,
                    height: 46,
                    borderRadius: 23,
                    overflow: 'hidden',
                    flexShrink: 0,
                    border: '2px solid rgba(250, 204, 21, 0.75)',
                    boxShadow: '0 0 12px rgba(250, 204, 21, 0.25)',
                    backgroundColor: 'rgba(15, 23, 42, 0.85)',
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
                      style={{
                        width: '100%',
                        height: '100%',
                        objectFit: 'cover',
                        display: 'block',
                      }}
                    />
                  ) : (
                    <span
                      style={{
                        fontSize: 15,
                        fontWeight: 800,
                        color: '#fde047',
                        letterSpacing: '0.03em',
                      }}
                    >
                      {driverInitials}
                    </span>
                  )}
                </div>

                <div style={{ minWidth: 0, flex: 1 }}>
                  <div
                    style={{
                      fontSize: 17.5,
                      fontWeight: 800,
                      color: '#ffffff',
                      letterSpacing: '-0.02em',
                      lineHeight: 1.2,
                      whiteSpace: 'nowrap',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                    }}
                  >
                    {telemetry.driver_name}
                  </div>
                  <div style={{ display: 'flex', alignItems: 'center', gap: 6, marginTop: 4, flexWrap: 'wrap' }}>
                    <span
                      style={{
                        fontSize: 11,
                        fontWeight: 700,
                        color: telemetry.role_color || '#fbbf24',
                      }}
                    >
                      [{telemetry.role || 'Driver'}]
                    </span>
                    <span style={{ fontSize: 11, color: 'rgba(255,255,255,0.35)' }}>|</span>
                    <span
                      style={{
                        fontSize: 11,
                        fontWeight: 700,
                        color: telemetry.rank_color || '#e9d5ff',
                      }}
                    >
                      {telemetry.rank || 'Driver'}
                    </span>
                  </div>
                </div>
              </div>

              {/* Discord ID & SteamID64 */}
              <div
                style={{
                  display: 'grid',
                  gridTemplateColumns: '1fr 1fr',
                  gap: 6,
                  padding: '7px 9px',
                  borderRadius: 10,
                  backgroundColor: 'rgba(2, 6, 23, 0.42)',
                  border: '1px solid rgba(255, 255, 255, 0.1)',
                }}
              >
                <div>
                  <div style={{ fontSize: 9.5, color: '#cbd5e1', fontWeight: 600 }}>DISCORD ID</div>
                  <div
                    className="tabular-nums"
                    style={{
                      fontSize: 10.5,
                      fontFamily: 'var(--font-mono)',
                      color: '#f8fafc',
                      fontWeight: 600,
                      marginTop: 1,
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      whiteSpace: 'nowrap',
                    }}
                  >
                    {telemetry.discord_id || '-'}
                  </div>
                </div>
                <div>
                  <div style={{ fontSize: 9.5, color: '#cbd5e1', fontWeight: 600 }}>STEAMID64</div>
                  <div
                    className="tabular-nums"
                    style={{
                      fontSize: 10.5,
                      fontFamily: 'var(--font-mono)',
                      color: '#f8fafc',
                      fontWeight: 600,
                      marginTop: 1,
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      whiteSpace: 'nowrap',
                    }}
                  >
                    {telemetry.steam_id || 'Not linked'}
                  </div>
                </div>
              </div>

              {/* Integrated Pass Barcode */}
              <div
                style={{
                  height: 24,
                  borderRadius: 5,
                  backgroundColor: 'rgba(255, 255, 255, 0.92)',
                  padding: '3px 10px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                }}
              >
                {[3, 1.5, 2, 3, 2, 1.5, 3, 1.5, 2, 3, 1.5, 2, 3, 2, 1.5, 3, 2, 1.5, 3, 2, 1.5, 3, 2, 1.5, 2.5].map(
                  (w, idx) => (
                    <div
                      key={idx}
                      style={{
                        width: w,
                        height: '100%',
                        backgroundColor: '#0f141e',
                        borderRadius: 0.5,
                      }}
                    />
                  )
                )}
              </div>
            </div>

            {/* 2. Nismara Cash & EXP Card (matching tablet_gui.cpp:2567-2603) */}
            <div
              style={{
                backgroundColor: 'var(--surface-card)',
                borderRadius: 16,
                border: '1px solid var(--border-subtle)',
                padding: 13,
                display: 'flex',
                flexDirection: 'column',
                gap: 9,
              }}
            >
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                  <Wallet size={13} color="#fbbf24" />
                  <span
                    style={{
                      fontSize: 10.5,
                      fontWeight: 800,
                      color: '#fbbf24',
                      letterSpacing: '0.05em',
                    }}
                  >
                    NISMARA CASH & EXP CARD
                  </span>
                </div>
                <button
                  onClick={() => window.nismara?.syncAccount?.()}
                  style={{
                    fontSize: 10.5,
                    fontWeight: 700,
                    color: '#38bdf8',
                    backgroundColor: 'rgba(56, 189, 248, 0.12)',
                    border: '1px solid rgba(56, 189, 248, 0.28)',
                    borderRadius: 6,
                    padding: '2px 8px',
                    cursor: 'pointer',
                  }}
                >
                  Sinkronkan Saldo
                </button>
              </div>

              <div
                className="tabular-nums"
                style={{
                  fontSize: 22,
                  fontWeight: 800,
                  color: '#fde047',
                  letterSpacing: '-0.02em',
                  lineHeight: 1.15,
                }}
              >
                {telemetry.total_nc.toLocaleString('id-ID', {
                  minimumFractionDigits: 2,
                  maximumFractionDigits: 2,
                })}{' '}
                <span style={{ fontSize: 13, fontWeight: 700, color: '#fbbf24' }}>NC</span>
              </div>

              {/* Driver Level, EXP, Convoy */}
              <div
                style={{
                  display: 'grid',
                  gridTemplateColumns: '1fr 1fr 1fr',
                  gap: 6,
                }}
              >
                <div
                  style={{
                    backgroundColor: 'var(--surface-card-elevated)',
                    padding: '6px 8px',
                    borderRadius: 8,
                  }}
                >
                  <div style={{ fontSize: 9.5, color: 'var(--text-muted)', fontWeight: 600 }}>LEVEL</div>
                  <div className="tabular-nums" style={{ fontSize: 12, fontWeight: 700, color: '#ffffff', marginTop: 1 }}>
                    Lv. {telemetry.level}
                  </div>
                </div>
                <div
                  style={{
                    backgroundColor: 'var(--surface-card-elevated)',
                    padding: '6px 8px',
                    borderRadius: 8,
                  }}
                >
                  <div style={{ fontSize: 9.5, color: 'var(--text-muted)', fontWeight: 600 }}>TOTAL EXP</div>
                  <div className="tabular-nums" style={{ fontSize: 12, fontWeight: 700, color: '#ffffff', marginTop: 1 }}>
                    {(telemetry.total_exp || 0).toLocaleString('id-ID')}
                  </div>
                </div>
                <div
                  style={{
                    backgroundColor: 'var(--surface-card-elevated)',
                    padding: '6px 8px',
                    borderRadius: 8,
                  }}
                >
                  <div style={{ fontSize: 9.5, color: 'var(--text-muted)', fontWeight: 600 }}>KONVOI</div>
                  <div
                    className="tabular-nums"
                    style={{
                      fontSize: 12,
                      fontWeight: 700,
                      color: '#38bdf8',
                      marginTop: 1,
                      display: 'flex',
                      alignItems: 'center',
                      gap: 4,
                    }}
                  >
                    <Users size={11} />
                    <span>{telemetry.joined_convoy || 0}</span>
                  </div>
                </div>
              </div>

              {/* Penalty Summary Row */}
              <div
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  paddingTop: 6,
                  borderTop: '1px solid var(--border-subtle)',
                }}
              >
                <span style={{ fontSize: 11, color: 'var(--text-secondary)', fontWeight: 500 }}>
                  Status Poin Penalti
                </span>
                <span
                  className="tabular-nums"
                  style={{
                    fontSize: 10.5,
                    fontWeight: 700,
                    padding: '2px 8px',
                    borderRadius: 6,
                    backgroundColor: penaltyBadgeStyle.bg,
                    border: `1px solid ${penaltyBadgeStyle.border}`,
                    color: penaltyBadgeStyle.text,
                  }}
                >
                  {penaltyPts} / 50 pts ({penaltyStatusLabel})
                </span>
              </div>
            </div>

            {/* 3. Status Perks & Proteksi Card (matching tablet_gui.cpp:2605-2637) */}
            <div
              style={{
                backgroundColor: 'var(--surface-card)',
                borderRadius: 16,
                border: '1px solid var(--border-subtle)',
                padding: 13,
                display: 'flex',
                flexDirection: 'column',
                gap: 8,
              }}
            >
              <div
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: 6,
                  paddingBottom: 6,
                  borderBottom: '1px solid var(--border-subtle)',
                }}
              >
                <ShieldCheck size={13} color="#34d399" />
                <span
                  style={{
                    fontSize: 10.5,
                    fontWeight: 800,
                    color: '#34d399',
                    letterSpacing: '0.05em',
                  }}
                >
                  STATUS PERKS & PROTEKSI
                </span>
              </div>

              {/* Row 1: Nismara+ */}
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <span style={{ fontSize: 11.5, color: 'var(--text-secondary)', fontWeight: 500 }}>
                  Keanggotaan Nismara+
                </span>
                <span
                  style={{
                    fontSize: 10.5,
                    fontWeight: 800,
                    padding: '2px 7px',
                    borderRadius: 6,
                    backgroundColor: telemetry.nismara_plus_active
                      ? 'rgba(52, 211, 153, 0.16)'
                      : 'rgba(148, 163, 184, 0.12)',
                    color: telemetry.nismara_plus_active ? '#34d399' : '#94a3b8',
                  }}
                >
                  {telemetry.nismara_plus_active ? 'AKTIF' : 'NONAKTIF'}
                </span>
              </div>

              {/* Row 2: Cargo Insurance */}
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <span style={{ fontSize: 11.5, color: 'var(--text-secondary)', fontWeight: 500 }}>
                  Asuransi Kargo ({telemetry.insurance_rating || 0}%)
                </span>
                <span
                  style={{
                    fontSize: 10.5,
                    fontWeight: 800,
                    padding: '2px 7px',
                    borderRadius: 6,
                    backgroundColor: telemetry.insurance_active
                      ? 'rgba(52, 211, 153, 0.16)'
                      : 'rgba(248, 113, 113, 0.15)',
                    color: telemetry.insurance_active ? '#34d399' : '#f87171',
                  }}
                >
                  {telemetry.insurance_active ? 'TERLINDUNGI' : 'KADALUARSA'}
                </span>
              </div>

              {/* Row 3: NC Voucher Booster */}
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <span style={{ fontSize: 11.5, color: 'var(--text-secondary)', fontWeight: 500 }}>
                  NC Voucher Booster
                </span>
                {telemetry.nc_boost_active ? (
                  <span
                    className="tabular-nums"
                    style={{
                      fontSize: 10.5,
                      fontWeight: 800,
                      padding: '2px 7px',
                      borderRadius: 6,
                      backgroundColor: 'rgba(56, 189, 248, 0.16)',
                      color: '#38bdf8',
                      display: 'flex',
                      alignItems: 'center',
                      gap: 3,
                    }}
                  >
                    <Zap size={10} />
                    <span>+{Math.round((telemetry.nc_boost_multiplier || 0) * 100)}% NC</span>
                  </span>
                ) : (
                  <span
                    style={{
                      fontSize: 10.5,
                      fontWeight: 700,
                      color: '#94a3b8',
                    }}
                  >
                    TIDAK ADA
                  </span>
                )}
              </div>

              {/* Row 4: Discord Server Booster */}
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <span style={{ fontSize: 11.5, color: 'var(--text-secondary)', fontWeight: 500 }}>
                  Server Booster Discord
                </span>
                <span
                  style={{
                    fontSize: 10.5,
                    fontWeight: 800,
                    padding: '2px 7px',
                    borderRadius: 6,
                    backgroundColor: telemetry.is_booster
                      ? 'rgba(192, 132, 252, 0.16)'
                      : 'rgba(148, 163, 184, 0.12)',
                    color: telemetry.is_booster ? '#c084fc' : '#94a3b8',
                  }}
                >
                  {telemetry.is_booster ? 'AKTIF' : 'NONAKTIF'}
                </span>
              </div>

              {/* Optional Top Manager Award */}
              {telemetry.top_manager_active && (
                <div
                  style={{
                    marginTop: 2,
                    padding: '6px 10px',
                    borderRadius: 8,
                    backgroundColor: 'rgba(251, 191, 36, 0.14)',
                    border: '1px solid rgba(251, 191, 36, 0.35)',
                    display: 'flex',
                    alignItems: 'center',
                    gap: 6,
                    color: '#fde047',
                    fontSize: 11,
                    fontWeight: 700,
                  }}
                >
                  <Award size={13} />
                  <span>Top Manager ({telemetry.top_manager_month})</span>
                </div>
              )}
            </div>
          </>
        )}
        </div>
      </div>
    </div>
  );
};
