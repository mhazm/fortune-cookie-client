import React, { useState } from 'react';
import { TelemetryData } from '../types/telemetry';
import {
  AlertTriangle,
  ArrowRight,
  Award,
  CheckCircle2,
  ChevronLeft,
  ClipboardList,
  Navigation,
  Receipt,
  ShieldAlert,
  ShieldCheck,
  Sparkles,
  Star,
  Truck,
  XCircle,
} from 'lucide-react';

interface Props {
  telemetry: TelemetryData;
  onBack: () => void;
}

export const DispatchApp: React.FC<Props> = ({ telemetry, onBack }) => {
  const [tab, setTab] = useState<'waybill' | 'receipt'>('waybill');

  const plannedKm = telemetry.planned_distance_km || 0;
  const drivenKm = telemetry.distance_km || 0;
  const navRemainKm = telemetry.nav_distance_km || 0;
  const maxCapKm = Math.round(plannedKm * 1.25);
  const effectiveKm =
    plannedKm > 0 ? Math.min(Math.max(drivenKm, plannedKm), maxCapKm) : Math.max(0, drivenKm);

  const lockedRate =
    telemetry.active_job_locked_price && telemetry.active_job_locked_price > 0
      ? telemetry.active_job_locked_price
      : 1.5;
  const specialMult = telemetry.is_special_job ? 2 : 1;
  const estBaseNc = Math.round(effectiveKm * lockedRate * specialMult);

  const routeProgressPct =
    plannedKm > 0
      ? Math.min(100, Math.max(0, ((plannedKm - navRemainKm) / plannedKm) * 100))
      : 0;

  const accumCargoDmg = (telemetry.job_accum_cargo_damage ?? telemetry.cargo_damage ?? 0) * 100;
  const accumTruckDmg = (telemetry.job_accum_vehicle_damage ?? 0) * 100;
  const accumTrailerDmg = (telemetry.job_accum_trailer_damage ?? 0) * 100;

  const isAts = telemetry.game_id === 'ats' || telemetry.game_id === 'amtrucks' || telemetry.game_id === '2';
  const realMilesSpeedLimit = isAts ? 129 : 100; // 80 mph (~128.75 km/h) for ATS, 100 km/h for ETS2
  const invalidSpeedLimit = isAts ? 210 : 180;   // 130 mph (~209.2 km/h) for ATS, 180 km/h for ETS2

  const topSpeedKmh = Math.round(telemetry.job_max_speed_kmh || 0);
  const isInvalidSpeed = topSpeedKmh > invalidSpeedLimit;
  const isRaceMiles = topSpeedKmh > realMilesSpeedLimit && topSpeedKmh <= invalidSpeedLimit;

  const fuelUsedL = telemetry.job_fuel_consumed_liters || 0;
  const avgLPer100Km = drivenKm > 5 ? (fuelUsedL / drivenKm) * 100 : 0;

  const cvarList = [
    telemetry.hc_traffic_ok,
    telemetry.hc_fatigue_ok,
    telemetry.hc_police_ok,
    telemetry.hc_parking_ok,
    telemetry.hc_road_events_ok,
    telemetry.hc_rain_prob_ok,
    telemetry.hc_advanced_coupling_ok,
    telemetry.hc_fuel_simulation_ok,
    telemetry.hc_air_brake_ok,
  ];
  const passedCvars = cvarList.filter(Boolean).length;
  const allCvarsOk = passedCvars === 9 && !telemetry.hc_forbidden_mods_found;

  const hcDisqualified =
    Boolean(telemetry.hardcore_disqualified) ||
    telemetry.hardcore_eligible_at_start === false ||
    Boolean(telemetry.hardcore_auto_park_used) ||
    Boolean(telemetry.hardcore_warp_detected) ||
    Boolean(telemetry.hc_forbidden_mods_found) ||
    isInvalidSpeed;

  const hasReceipt = Boolean(telemetry.has_last_job_receipt);
  const hasMatchedFleet = Boolean(telemetry.has_matched_fleet && telemetry.fleet_number);

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
            color: '#fbbf24',
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
          Jobs & Dispatch
        </span>
        <span
          style={{
            fontSize: 9.5,
            fontWeight: 800,
            padding: '2px 7px',
            borderRadius: 6,
            backgroundColor: telemetry.job_active
              ? 'rgba(251, 191, 36, 0.18)'
              : 'rgba(56, 189, 248, 0.15)',
            color: telemetry.job_active ? '#fbbf24' : '#38bdf8',
          }}
        >
          {telemetry.job_active ? 'ONGOING' : 'STANDBY'}
        </span>
      </div>

      {/* Segmented Control: [Waybill Rute] [Slip Paycheck] */}
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
          onClick={() => setTab('waybill')}
          style={{
            padding: '6px 0',
            borderRadius: 8,
            fontSize: 11,
            fontWeight: tab === 'waybill' ? 700 : 500,
            color: tab === 'waybill' ? '#020617' : '#94a3b8',
            backgroundColor: tab === 'waybill' ? '#fbbf24' : 'transparent',
            border: 'none',
            cursor: 'pointer',
            transition: 'all 0.16s ease',
          }}
        >
          {telemetry.job_active ? 'Kontrak Aktif' : 'Status Dispatch'}
        </button>
        <button
          onClick={() => setTab('receipt')}
          style={{
            padding: '6px 0',
            borderRadius: 8,
            fontSize: 11,
            fontWeight: tab === 'receipt' ? 700 : 500,
            color: tab === 'receipt' ? '#020617' : '#94a3b8',
            backgroundColor: tab === 'receipt' ? '#fbbf24' : 'transparent',
            border: 'none',
            cursor: 'pointer',
            transition: 'all 0.16s ease',
          }}
        >
          Slip Paycheck {hasReceipt ? '•' : ''}
        </button>
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
          {tab === 'waybill' ? (
            !telemetry.job_active ? (
              /* ============================================================
               * STANDBY / CAB FREE ROAM VIEW (RICH PRE-FLIGHT & RULES HUB)
               * ============================================================ */
              <>
                {/* 1. Hero Dispatch Status Card */}
                <div
                  style={{
                    backgroundColor: 'var(--surface-card)',
                    borderRadius: 16,
                    border: '1px solid var(--border-subtle)',
                    padding: '14px',
                    display: 'flex',
                    flexDirection: 'column',
                    gap: 10,
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: 11 }}>
                    <div
                      style={{
                        width: 44,
                        height: 44,
                        borderRadius: 12,
                        backgroundColor: 'rgba(251, 191, 36, 0.14)',
                        border: '1.5px solid rgba(251, 191, 36, 0.35)',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        color: '#fbbf24',
                        flexShrink: 0,
                      }}
                    >
                      <Navigation size={21} />
                    </div>
                    <div style={{ minWidth: 0, flex: 1 }}>
                      <div
                        style={{
                          fontSize: 9.5,
                          fontWeight: 800,
                          color: '#38bdf8',
                          letterSpacing: '0.06em',
                        }}
                      >
                        DISPATCH TELEMETRY READY
                      </div>
                      <div style={{ fontSize: 15, fontWeight: 800, color: '#ffffff', marginTop: 1 }}>
                        Standby / Cab Free Roam
                      </div>
                      <div style={{ fontSize: 10.5, color: '#94a3b8', marginTop: 1 }}>
                        Ambil kontrak di Freight, Cargo, atau External Market.
                      </div>
                    </div>
                  </div>

                  {/* Session & Server Info Grid */}
                  <div
                    className="tabular-nums"
                    style={{
                      display: 'grid',
                      gridTemplateColumns: '1fr 1fr',
                      gap: 7,
                    }}
                  >
                    <div
                      style={{
                        backgroundColor: 'var(--surface-card-elevated)',
                        borderRadius: 9,
                        padding: '7px 9px',
                      }}
                    >
                      <div style={{ fontSize: 9, color: '#94a3b8', fontWeight: 600 }}>MODE SERVER</div>
                      <div style={{ fontSize: 11.5, fontWeight: 800, color: '#34d399', marginTop: 1 }}>
                        {telemetry.server_mode || 'Singleplayer'}
                      </div>
                    </div>
                    <div
                      style={{
                        backgroundColor: 'var(--surface-card-elevated)',
                        borderRadius: 9,
                        padding: '7px 9px',
                      }}
                    >
                      <div style={{ fontSize: 9, color: '#94a3b8', fontWeight: 600 }}>STATUS TRACKER</div>
                      <div style={{ fontSize: 11, fontWeight: 700, color: '#e2e8f0', marginTop: 1 }}>
                        {telemetry.active_job_sync_status || 'Siap Mencatat'}
                      </div>
                    </div>
                  </div>

                  {hasReceipt && (
                    <button
                      onClick={() => setTab('receipt')}
                      style={{
                        width: '100%',
                        padding: '8px 12px',
                        borderRadius: 9,
                        backgroundColor: 'rgba(251, 191, 36, 0.15)',
                        border: '1px solid rgba(251, 191, 36, 0.35)',
                        color: '#fde047',
                        fontSize: 11,
                        fontWeight: 700,
                        cursor: 'pointer',
                      }}
                    >
                      Lihat Slip Paycheck Pengiriman Terakhir →
                    </button>
                  )}
                </div>

                {/* 2. Pre-Trip Vehicle & Hardcore Readiness Card */}
                <div
                  style={{
                    backgroundColor: 'var(--surface-card)',
                    borderRadius: 16,
                    border: '1px solid var(--border-subtle)',
                    padding: '13px 14px',
                    display: 'flex',
                    flexDirection: 'column',
                    gap: 8,
                  }}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                      <Truck size={13} color="#34d399" />
                      <span style={{ fontSize: 10, fontWeight: 800, color: '#34d399', letterSpacing: '0.05em' }}>
                        KESIAPAN ARMADA & SIMULASI
                      </span>
                    </div>
                    <span
                      style={{
                        fontSize: 9,
                        fontWeight: 800,
                        padding: '2px 6px',
                        borderRadius: 5,
                        backgroundColor: hasMatchedFleet
                          ? 'rgba(52, 211, 153, 0.16)'
                          : 'rgba(251, 191, 36, 0.16)',
                        color: hasMatchedFleet ? '#34d399' : '#fbbf24',
                      }}
                    >
                      {hasMatchedFleet ? `UNIT [${telemetry.fleet_number}]` : 'TRUK SEWA'}
                    </span>
                  </div>

                  <div className="tabular-nums" style={{ display: 'flex', flexDirection: 'column', gap: 5, fontSize: 11 }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <span style={{ color: '#94a3b8' }}>Kendaraan Aktif</span>
                      <span style={{ fontWeight: 700, color: '#ffffff' }}>
                        {telemetry.truck_name
                          ? `${telemetry.truck_brand || ''} ${telemetry.truck_name}`
                          : 'Belum Terdeteksi'}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <span style={{ color: '#94a3b8' }}>Sisa BBM Tangki Truk</span>
                      <span style={{ fontWeight: 700, color: '#38bdf8' }}>
                        {Math.round(telemetry.fuel || 0)} / {Math.round(telemetry.fuel_capacity || 0)} L
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <span style={{ color: '#94a3b8' }}>Validasi 9 Parameter Cvars</span>
                      <span
                        style={{
                          fontWeight: 800,
                          color: allCvarsOk ? '#34d399' : '#fbbf24',
                        }}
                      >
                        {passedCvars}/9 Parameter {allCvarsOk ? '(Hardcore Ready)' : '(Mode Reguler)'}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <span style={{ color: '#94a3b8' }}>Bonus Perks Aktif</span>
                      <span style={{ fontWeight: 700, color: '#fde047' }}>
                        {telemetry.nismara_plus_active ? 'Nismara+ (+15%)' : 'Standar'}
                        {telemetry.nc_boost_active ? ' + Voucher' : ''}
                      </span>
                    </div>
                  </div>
                </div>

                {/* 3. VTC Delivery Standards & Penalty-Free Guide Card */}
                <div
                  style={{
                    backgroundColor: 'var(--surface-card)',
                    borderRadius: 16,
                    border: '1px solid var(--border-subtle)',
                    padding: '13px 14px',
                    display: 'flex',
                    flexDirection: 'column',
                    gap: 8,
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                    <ShieldCheck size={13} color="#38bdf8" />
                    <span style={{ fontSize: 10, fontWeight: 800, color: '#38bdf8', letterSpacing: '0.05em' }}>
                      STANDAR PENGIRIMAN BEBAS PENALTI
                    </span>
                  </div>

                  <div className="tabular-nums" style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 6 }}>
                    <div
                      style={{
                        backgroundColor: 'var(--surface-card-elevated)',
                        borderRadius: 9,
                        padding: '7px 9px',
                      }}
                    >
                      <div style={{ fontSize: 9, color: '#94a3b8' }}>JARAK MINIMUM</div>
                      <div style={{ fontSize: 11.5, fontWeight: 800, color: '#34d399', marginTop: 1 }}>
                        &gt; 150 km
                      </div>
                      <div style={{ fontSize: 9, color: '#64748b' }}>Hardcore: ≥ 250 km</div>
                    </div>

                    <div
                      style={{
                        backgroundColor: 'var(--surface-card-elevated)',
                        borderRadius: 9,
                        padding: '7px 9px',
                      }}
                    >
                      <div style={{ fontSize: 9, color: '#94a3b8' }}>BATAS KECEPATAN</div>
                      <div style={{ fontSize: 11.5, fontWeight: 800, color: '#34d399', marginTop: 1 }}>
                        ≤ 100 km/h
                      </div>
                      <div style={{ fontSize: 9, color: '#64748b' }}>Real Miles VTC</div>
                    </div>

                    <div
                      style={{
                        backgroundColor: 'var(--surface-card-elevated)',
                        borderRadius: 9,
                        padding: '7px 9px',
                      }}
                    >
                      <div style={{ fontSize: 9, color: '#94a3b8' }}>BATAS AMAN DAMAGE</div>
                      <div style={{ fontSize: 11, fontWeight: 800, color: '#f8fafc', marginTop: 1 }}>
                        Kargo &lt;6% • Truk &lt;11%
                      </div>
                      <div style={{ fontSize: 9, color: '#64748b' }}>Trailer &lt;8%</div>
                    </div>

                    <div
                      style={{
                        backgroundColor: 'var(--surface-card-elevated)',
                        borderRadius: 9,
                        padding: '7px 9px',
                      }}
                    >
                      <div style={{ fontSize: 9, color: '#94a3b8' }}>ANTI-FARMING CAP</div>
                      <div style={{ fontSize: 11.5, fontWeight: 800, color: '#38bdf8', marginTop: 1 }}>
                        Maks +25% Rute
                      </div>
                      <div style={{ fontSize: 9, color: '#64748b' }}>Dari Planned KM</div>
                    </div>
                  </div>
                </div>
              </>
            ) : (
              /* ============================================================
               * ACTIVE WAYBILL CONTRACT VIEW (RICH LIVE TELEMETRY & PAYOUT)
               * ============================================================ */
              <>
                {/* 1. Cargo Manifest & Live Gross Payout Estimate Card */}
                <div
                  style={{
                    backgroundColor: 'var(--surface-card)',
                    borderRadius: 16,
                    border: '1px solid rgba(251, 191, 36, 0.35)',
                    padding: '13px 14px',
                    display: 'flex',
                    flexDirection: 'column',
                    gap: 9,
                  }}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 5 }}>
                      <ClipboardList size={13} color="#fbbf24" />
                      <span
                        style={{
                          fontSize: 10,
                          fontWeight: 800,
                          color: '#fbbf24',
                          letterSpacing: '0.05em',
                        }}
                      >
                        WAYBILL #{telemetry.active_job_db_id || 'LIVE'}
                      </span>
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 5 }}>
                      {telemetry.is_special_job && (
                        <span
                          style={{
                            fontSize: 9,
                            fontWeight: 800,
                            padding: '2px 6px',
                            borderRadius: 5,
                            backgroundColor: 'rgba(244, 63, 94, 0.2)',
                            color: '#fb7185',
                          }}
                        >
                          2X SPECIAL
                        </span>
                      )}
                      <span
                        style={{
                          fontSize: 9.5,
                          fontWeight: 800,
                          padding: '2px 7px',
                          borderRadius: 6,
                          backgroundColor: 'rgba(52, 211, 153, 0.18)',
                          color: '#34d399',
                        }}
                      >
                        {telemetry.active_job_sync_status || 'ONGOING'}
                      </span>
                    </div>
                  </div>

                  <div>
                    <div style={{ fontSize: 16, fontWeight: 800, color: '#ffffff', lineHeight: 1.2 }}>
                      {telemetry.cargo || 'General Freight'}
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 7, marginTop: 4, flexWrap: 'wrap' }}>
                      <span className="tabular-nums" style={{ fontSize: 11, color: '#cbd5e1', fontWeight: 600 }}>
                        Tonase: {((telemetry.cargo_mass_kg || 0) / 1000).toFixed(1)} Ton
                      </span>
                      <span style={{ color: '#475569' }}>•</span>
                      <span style={{ fontSize: 11, color: '#38bdf8', fontWeight: 700 }}>
                        {telemetry.job_market || 'Freight Market'}
                      </span>
                      <span style={{ color: '#475569' }}>•</span>
                      <span style={{ fontSize: 10.5, color: '#34d399', fontWeight: 700 }}>
                        {hasMatchedFleet ? `[${telemetry.fleet_number}]` : 'Truk Sewa'}
                      </span>
                    </div>
                  </div>

                  {/* Live Gross Estimate + Locked Rate + Cap */}
                  <div
                    className="tabular-nums"
                    style={{
                      backgroundColor: 'var(--surface-card-elevated)',
                      borderRadius: 10,
                      padding: '8px 10px',
                      border: '1px solid rgba(251, 191, 36, 0.25)',
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                    }}
                  >
                    <div>
                      <div style={{ fontSize: 9, color: '#fde68a', fontWeight: 700 }}>
                        ESTIMASI UPAH DASAR KARGO
                      </div>
                      <div style={{ fontSize: 10, color: '#94a3b8', marginTop: 1 }}>
                        Tarif: {lockedRate.toFixed(2)} NC/km • Cap: {maxCapKm.toLocaleString('id-ID')} km
                      </div>
                    </div>
                    <div style={{ textAlign: 'right' }}>
                      <div style={{ fontSize: 15, fontWeight: 900, color: '#fde047' }}>
                        ~{estBaseNc.toLocaleString('id-ID')} NC
                      </div>
                    </div>
                  </div>
                </div>

                {/* 2. Route Timeline & GPS Progress Card */}
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
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <span
                      style={{
                        fontSize: 10,
                        fontWeight: 800,
                        color: '#38bdf8',
                        letterSpacing: '0.05em',
                      }}
                    >
                      RUTE & PROGRES NAVIGASI
                    </span>
                    <span className="tabular-nums" style={{ fontSize: 10.5, fontWeight: 800, color: '#34d399' }}>
                      {routeProgressPct.toFixed(0)}% Selesai
                    </span>
                  </div>

                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', gap: 8 }}>
                    <div style={{ minWidth: 0, flex: 1 }}>
                      <div style={{ fontSize: 9, color: '#34d399', fontWeight: 800 }}>KOTA ASAL (ORIGIN)</div>
                      <div
                        style={{
                          fontSize: 13.5,
                          fontWeight: 800,
                          color: '#ffffff',
                          whiteSpace: 'nowrap',
                          overflow: 'hidden',
                          textOverflow: 'ellipsis',
                        }}
                      >
                        {telemetry.city_src || 'Origin'}
                      </div>
                      <div
                        style={{
                          fontSize: 10.5,
                          color: '#94a3b8',
                          whiteSpace: 'nowrap',
                          overflow: 'hidden',
                          textOverflow: 'ellipsis',
                        }}
                      >
                        {telemetry.company_src || 'Depot'}
                      </div>
                    </div>

                    <ArrowRight size={17} color="#fbbf24" style={{ flexShrink: 0 }} />

                    <div style={{ minWidth: 0, flex: 1, textAlign: 'right' }}>
                      <div style={{ fontSize: 9, color: '#fbbf24', fontWeight: 800 }}>TUJUAN (DESTINATION)</div>
                      <div
                        style={{
                          fontSize: 13.5,
                          fontWeight: 800,
                          color: '#ffffff',
                          whiteSpace: 'nowrap',
                          overflow: 'hidden',
                          textOverflow: 'ellipsis',
                        }}
                      >
                        {telemetry.city_dst || 'Destination'}
                      </div>
                      <div
                        style={{
                          fontSize: 10.5,
                          color: '#94a3b8',
                          whiteSpace: 'nowrap',
                          overflow: 'hidden',
                          textOverflow: 'ellipsis',
                        }}
                      >
                        {telemetry.company_dst || 'Terminal'}
                      </div>
                    </div>
                  </div>

                  {/* Progress Bar & Distance Trio */}
                  <div style={{ display: 'flex', flexDirection: 'column', gap: 5 }}>
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
                          width: `${routeProgressPct}%`,
                          height: '100%',
                          borderRadius: 4,
                          background: 'linear-gradient(90deg, #fbbf24 0%, #34d399 100%)',
                        }}
                      />
                    </div>
                    <div
                      className="tabular-nums"
                      style={{
                        display: 'grid',
                        gridTemplateColumns: 'repeat(3, 1fr)',
                        gap: 6,
                        marginTop: 2,
                      }}
                    >
                      <div
                        style={{
                          backgroundColor: 'var(--surface-card-elevated)',
                          borderRadius: 8,
                          padding: '5px 7px',
                        }}
                      >
                        <div style={{ fontSize: 8.5, color: '#94a3b8' }}>DITEMPUH</div>
                        <div style={{ fontSize: 11, fontWeight: 800, color: '#ffffff' }}>
                          {Math.round(drivenKm).toLocaleString('id-ID')} km
                        </div>
                      </div>
                      <div
                        style={{
                          backgroundColor: 'var(--surface-card-elevated)',
                          borderRadius: 8,
                          padding: '5px 7px',
                        }}
                      >
                        <div style={{ fontSize: 8.5, color: '#94a3b8' }}>SISA GPS</div>
                        <div style={{ fontSize: 11, fontWeight: 800, color: '#fbbf24' }}>
                          {Math.round(navRemainKm).toLocaleString('id-ID')} km
                        </div>
                      </div>
                      <div
                        style={{
                          backgroundColor: 'var(--surface-card-elevated)',
                          borderRadius: 8,
                          padding: '5px 7px',
                        }}
                      >
                        <div style={{ fontSize: 8.5, color: '#94a3b8' }}>RENCANA</div>
                        <div style={{ fontSize: 11, fontWeight: 800, color: '#38bdf8' }}>
                          {Math.round(plannedKm).toLocaleString('id-ID')} km
                        </div>
                      </div>
                    </div>
                  </div>
                </div>

                {/* 3. Trip Logbook, Speed Classification & Cumulative Damage Card */}
                <div
                  style={{
                    backgroundColor: 'var(--surface-card)',
                    borderRadius: 16,
                    border: '1px solid var(--border-subtle)',
                    padding: '13px 14px',
                    display: 'flex',
                    flexDirection: 'column',
                    gap: 9,
                  }}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <span
                      style={{
                        fontSize: 10,
                        fontWeight: 800,
                        color: '#c084fc',
                        letterSpacing: '0.05em',
                      }}
                    >
                      KECEPATAN & AKUMULASI DAMAGE
                    </span>
                    <span
                      style={{
                        fontSize: 9,
                        fontWeight: 800,
                        padding: '2px 7px',
                        borderRadius: 6,
                        backgroundColor: isInvalidSpeed
                          ? 'rgba(239, 68, 68, 0.22)'
                          : isRaceMiles
                          ? 'rgba(251, 191, 36, 0.18)'
                          : 'rgba(52, 211, 153, 0.16)',
                        color: isInvalidSpeed ? '#f87171' : isRaceMiles ? '#fbbf24' : '#34d399',
                      }}
                    >
                      {isInvalidSpeed
                        ? `INVALID (>${isAts ? '130 mph' : '180 km/h'})`
                        : isRaceMiles
                        ? `RACE MILES (>${isAts ? '80 mph' : '100 km/h'})`
                        : `REAL MILES (≤${isAts ? '80 mph' : '100 km/h'})`}
                    </span>
                  </div>

                  {/* 3 Cumulative Damage Pills with VTC Penalty Threshold Hints */}
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 6 }}>
                    {[
                      { label: 'KARGO', val: accumCargoDmg, limit: 6 },
                      { label: 'TRUK', val: accumTruckDmg, limit: 11 },
                      { label: 'TRAILER', val: accumTrailerDmg, limit: 8 },
                    ].map((d) => {
                      const overLimit = d.val >= d.limit;
                      const warnLimit = !overLimit && d.val >= d.limit * 0.6;
                      const color = overLimit ? '#f87171' : warnLimit ? '#fbbf24' : '#34d399';
                      return (
                        <div
                          key={d.label}
                          style={{
                            backgroundColor: 'var(--surface-card-elevated)',
                            borderRadius: 9,
                            padding: '6px 7px',
                            textAlign: 'center',
                            border: overLimit ? '1px solid rgba(248, 113, 113, 0.35)' : '1px solid transparent',
                          }}
                        >
                          <div style={{ fontSize: 8.5, color: '#94a3b8', fontWeight: 700 }}>{d.label}</div>
                          <div
                            className="tabular-nums"
                            style={{
                              fontSize: 12.5,
                              fontWeight: 800,
                              color,
                              marginTop: 1,
                            }}
                          >
                            {d.val.toFixed(1)}%
                          </div>
                          <div style={{ fontSize: 8, color: '#64748b' }}>Batas &lt;{d.limit}%</div>
                        </div>
                      );
                    })}
                  </div>

                  {/* Trip Telemetry Metrics */}
                  <div
                    className="tabular-nums"
                    style={{
                      display: 'grid',
                      gridTemplateColumns: 'repeat(3, 1fr)',
                      gap: 6,
                      fontSize: 10.5,
                    }}
                  >
                    <div
                      style={{
                        backgroundColor: 'var(--surface-card-elevated)',
                        borderRadius: 9,
                        padding: '6px 8px',
                      }}
                    >
                      <div style={{ fontSize: 8.5, color: '#94a3b8' }}>BBM TERPAKAI</div>
                      <div style={{ fontWeight: 800, color: '#ffffff', marginTop: 1 }}>
                        {fuelUsedL.toFixed(1)} L
                      </div>
                      <div style={{ fontSize: 8, color: '#64748b' }}>
                        {avgLPer100Km > 0 ? `${avgLPer100Km.toFixed(1)} L/100km` : 'Menghitung...'}
                      </div>
                    </div>
                    <div
                      style={{
                        backgroundColor: 'var(--surface-card-elevated)',
                        borderRadius: 9,
                        padding: '6px 8px',
                      }}
                    >
                      <div style={{ fontSize: 8.5, color: '#94a3b8' }}>TOP SPEED</div>
                      <div
                        style={{
                          fontWeight: 800,
                          color: isInvalidSpeed || isRaceMiles ? '#f87171' : '#ffffff',
                          marginTop: 1,
                        }}
                      >
                        {topSpeedKmh} km/h {isAts ? `(${Math.round(topSpeedKmh / 1.609344)} mph)` : ''}
                      </div>
                      <div style={{ fontSize: 8, color: '#64748b' }}>
                        Overspeed: {Math.round(telemetry.hardcore_speeding_seconds || 0)}s
                      </div>
                    </div>
                    <div
                      style={{
                        backgroundColor: 'var(--surface-card-elevated)',
                        borderRadius: 9,
                        padding: '6px 8px',
                      }}
                    >
                      <div style={{ fontSize: 8.5, color: '#94a3b8' }}>TILANG POLISI</div>
                      <div
                        style={{
                          fontWeight: 800,
                          color: (telemetry.job_fines_count || 0) > 0 ? '#fbbf24' : '#34d399',
                          marginTop: 1,
                        }}
                      >
                        {telemetry.job_fines_count || 0}x Tilang
                      </div>
                      <div style={{ fontSize: 8, color: '#64748b' }}>Selama Rute</div>
                    </div>
                  </div>

                  {/* Hardcore Live Checklist */}
                  <div
                    style={{
                      display: 'flex',
                      flexDirection: 'column',
                      gap: 5,
                      paddingTop: 7,
                      borderTop: '1px solid var(--border-subtle)',
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', fontSize: 10.5 }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: 5 }}>
                        {hcDisqualified ? (
                          <ShieldAlert size={13} color="#f87171" />
                        ) : (
                          <ShieldCheck size={13} color="#34d399" />
                        )}
                        <span style={{ color: '#cbd5e1', fontWeight: 700 }}>Kelayakan Bonus Hardcore</span>
                      </div>
                      <span
                        style={{
                          fontSize: 10,
                          fontWeight: 800,
                          color: hcDisqualified ? '#f87171' : '#34d399',
                        }}
                      >
                        {hcDisqualified ? 'GUGUR (Disqualified)' : 'ELIGIBLE (+25% NC)'}
                      </span>
                    </div>

                    <div
                      className="tabular-nums"
                      style={{
                        display: 'grid',
                        gridTemplateColumns: '1fr 1fr',
                        gap: 4,
                        fontSize: 9.5,
                        color: '#94a3b8',
                      }}
                    >
                      <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                        {plannedKm >= 250 || drivenKm >= 250 ? (
                          <CheckCircle2 size={11} color="#34d399" />
                        ) : (
                          <AlertTriangle size={11} color="#fbbf24" />
                        )}
                        <span>Jarak Rute ≥ 250 km</span>
                      </div>
                      <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                        {allCvarsOk ? (
                          <CheckCircle2 size={11} color="#34d399" />
                        ) : (
                          <AlertTriangle size={11} color="#fbbf24" />
                        )}
                        <span>9 Parameter ({passedCvars}/9)</span>
                      </div>
                      <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                        {!telemetry.hardcore_auto_park_used && !telemetry.hardcore_warp_detected ? (
                          <CheckCircle2 size={11} color="#34d399" />
                        ) : (
                          <XCircle size={11} color="#f87171" />
                        )}
                        <span>Tanpa AutoPark/Warp</span>
                      </div>
                      <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                        {!isRaceMiles && !isInvalidSpeed ? (
                          <CheckCircle2 size={11} color="#34d399" />
                        ) : (
                          <AlertTriangle size={11} color="#fbbf24" />
                        )}
                        <span>Real Miles ({isAts ? '≤80 mph' : '≤100 km/h'})</span>
                      </div>
                    </div>

                    {hcDisqualified && (
                      <div
                        style={{
                          backgroundColor: 'rgba(239, 68, 68, 0.12)',
                          border: '1px solid rgba(239, 68, 68, 0.3)',
                          borderRadius: 6,
                          padding: '4px 8px',
                          fontSize: 9,
                          color: '#fca5a5',
                          display: 'flex',
                          alignItems: 'center',
                          gap: 5,
                        }}
                      >
                        <AlertTriangle size={12} color="#f87171" style={{ flexShrink: 0 }} />
                        <span>
                          {telemetry.hardcore_disqualify_reason ||
                            (telemetry.hardcore_eligible_at_start === false
                              ? 'Pengaturan awal tidak memenuhi kriteria Hardcore'
                              : isInvalidSpeed
                              ? `Kecepatan melebihi batas Real Miles (${isAts ? '80 mph / 129 km/h' : '100 km/h'})`
                              : 'Diskualifikasi simulasi Hardcore')}
                        </span>
                      </div>
                    )}
                  </div>
                </div>
              </>
            )
          ) : (
            /* ============================================================
             * TAB 2: SLIP PAYCHECK TERAKHIR & 5-STAR DELIVERY BREAKDOWN
             * ============================================================ */
            !hasReceipt ? (
              <div
                style={{
                  backgroundColor: 'var(--surface-card)',
                  borderRadius: 18,
                  border: '1px solid var(--border-subtle)',
                  padding: '26px 16px',
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  textAlign: 'center',
                  gap: 10,
                  marginTop: 6,
                }}
              >
                <Receipt size={32} color="#94a3b8" />
                <div style={{ fontSize: 14, fontWeight: 800, color: '#ffffff' }}>
                  Belum Ada Slip Paycheck
                </div>
                <p style={{ fontSize: 11.5, color: '#94a3b8', lineHeight: 1.4, margin: 0 }}>
                  Selesaikan minimal 1 pengantaran kargo pada sesi ini untuk melihat rincian pendapatan bersih, evaluasi bintang Hardcore, potongan BBM, dan pajak VTC.
                </p>
              </div>
            ) : (
              <>
                {/* 1. Financial Paystub Card */}
                <div
                  style={{
                    backgroundColor: 'var(--surface-card)',
                    borderRadius: 16,
                    border: '1px solid var(--border-subtle)',
                    padding: '14px',
                    display: 'flex',
                    flexDirection: 'column',
                    gap: 10,
                  }}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                      <Sparkles size={13} color="#fbbf24" />
                      <span
                        style={{
                          fontSize: 10,
                          fontWeight: 800,
                          color: '#fbbf24',
                          letterSpacing: '0.05em',
                        }}
                      >
                        SLIP GAJI JOB #{telemetry.last_receipt_job_id || '-'}
                      </span>
                    </div>
                    <span
                      className="tabular-nums"
                      style={{
                        fontSize: 10,
                        fontWeight: 800,
                        color: '#34d399',
                        backgroundColor: 'rgba(52, 211, 153, 0.16)',
                        padding: '2px 7px',
                        borderRadius: 6,
                      }}
                    >
                      +{Math.round(telemetry.last_receipt_xp || 0)} EXP
                    </span>
                  </div>

                  <div>
                    <div style={{ fontSize: 14.5, fontWeight: 800, color: '#ffffff' }}>
                      Kontrak Selesai ({telemetry.last_receipt_game_mode || telemetry.server_mode || 'Singleplayer'})
                    </div>
                    <div className="tabular-nums" style={{ fontSize: 11, color: '#94a3b8', marginTop: 2 }}>
                      Jarak Tempuh: {Math.round(telemetry.last_receipt_distance_km || 0).toLocaleString('id-ID')} km
                    </div>
                  </div>

                  {/* Net Income Highlight */}
                  <div
                    style={{
                      backgroundColor: 'var(--surface-card-elevated)',
                      borderRadius: 12,
                      padding: '10px 12px',
                      border: '1px solid rgba(250, 204, 21, 0.25)',
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                    }}
                  >
                    <span style={{ fontSize: 11, fontWeight: 700, color: '#fde68a' }}>PENDAPATAN BERSIH</span>
                    <span className="tabular-nums" style={{ fontSize: 18, fontWeight: 900, color: '#fde047' }}>
                      +{(telemetry.last_receipt_revenue_nc || 0).toLocaleString('id-ID', {
                        minimumFractionDigits: 0,
                        maximumFractionDigits: 2,
                      })}{' '}
                      NC
                    </span>
                  </div>

                  {/* Financial Breakdown */}
                  <div
                    className="tabular-nums"
                    style={{
                      display: 'flex',
                      flexDirection: 'column',
                      gap: 6,
                      fontSize: 11,
                      paddingTop: 4,
                    }}
                  >
                    {[
                      {
                        label: 'Upah Dasar Kargo',
                        val: telemetry.last_receipt_base_nc || 0,
                        positive: true,
                      },
                      {
                        label: 'Bonus Server Multiplayer',
                        val: telemetry.last_receipt_mp_bonus_nc || 0,
                        positive: true,
                      },
                      {
                        label: 'Bonus Simulasi Hardcore',
                        val: telemetry.last_receipt_hardcore_nc || 0,
                        positive: true,
                      },
                      {
                        label: 'Bonus Perks & Booster',
                        val: telemetry.last_receipt_nplus_nc || 0,
                        positive: true,
                      },
                      {
                        label: 'Potongan Servis & BBM',
                        val: -(telemetry.last_receipt_cost_nc || 0),
                        positive: false,
                      },
                      {
                        label: 'Potongan Pajak VTC',
                        val: -(telemetry.last_receipt_tax_nc || 0),
                        positive: false,
                      },
                    ].map((item) => (
                      <div
                        key={item.label}
                        style={{
                          display: 'flex',
                          justifyContent: 'space-between',
                          color: '#cbd5e1',
                        }}
                      >
                        <span>{item.label}</span>
                        <span
                          style={{
                            fontWeight: 700,
                            color:
                              item.val === 0
                                ? '#64748b'
                                : item.positive
                                ? '#34d399'
                                : '#f87171',
                          }}
                        >
                          {item.val >= 0 ? '+' : ''}
                          {item.val.toLocaleString('id-ID', {
                            minimumFractionDigits: 0,
                            maximumFractionDigits: 2,
                          })}{' '}
                          NC
                        </span>
                      </div>
                    ))}
                  </div>

                  {/* Penalty Summary Row */}
                  <div
                    style={{
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                      paddingTop: 8,
                      borderTop: '1px solid var(--border-subtle)',
                      fontSize: 11,
                    }}
                  >
                    <span style={{ color: '#94a3b8' }}>Poin Penalti Pengiriman</span>
                    <span
                      className="tabular-nums"
                      style={{
                        fontWeight: 800,
                        color:
                          (telemetry.last_receipt_penalty_pts || 0) > 0
                            ? '#f87171'
                            : '#34d399',
                      }}
                    >
                      {(telemetry.last_receipt_penalty_pts || 0) > 0
                        ? `+${telemetry.last_receipt_penalty_pts} Poin Penalti`
                        : '0 Poin (Bersih)'}
                    </span>
                  </div>
                </div>

                {/* 2. Hardcore 5-Star Evaluation Breakdown Card */}
                <div
                  style={{
                    backgroundColor: 'var(--surface-card)',
                    borderRadius: 16,
                    border: '1px solid var(--border-subtle)',
                    padding: '13px 14px',
                    display: 'flex',
                    flexDirection: 'column',
                    gap: 9,
                  }}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                      <Award size={14} color="#fbbf24" />
                      <span style={{ fontSize: 10, fontWeight: 800, color: '#fbbf24', letterSpacing: '0.05em' }}>
                        EVALUASI BINTANG & HARDCORE
                      </span>
                    </div>
                    <span
                      className="tabular-nums"
                      style={{
                        fontSize: 10,
                        fontWeight: 800,
                        color: telemetry.last_receipt_is_hardcore ? '#34d399' : '#94a3b8',
                      }}
                    >
                      {telemetry.last_receipt_stars_total || 0}/25 ★ ({telemetry.last_receipt_hardcore_points || 0} HP)
                    </span>
                  </div>

                  {telemetry.last_receipt_disqualify_reason && (
                    <div
                      style={{
                        backgroundColor: 'rgba(239, 68, 68, 0.14)',
                        border: '1px solid rgba(239, 68, 68, 0.3)',
                        borderRadius: 9,
                        padding: '7px 9px',
                        fontSize: 10.5,
                        color: '#fca5a5',
                      }}
                    >
                      Catatan: {telemetry.last_receipt_disqualify_reason}
                    </div>
                  )}

                  <div className="tabular-nums" style={{ display: 'flex', flexDirection: 'column', gap: 5, fontSize: 11 }}>
                    {[
                      { label: 'Jarak Tempuh Rute', stars: telemetry.last_receipt_stars_distance || 0 },
                      { label: 'Beban Tonase Kargo', stars: telemetry.last_receipt_stars_mass || 0 },
                      { label: 'Keamanan Kargo (Damage)', stars: telemetry.last_receipt_stars_damage || 0 },
                      { label: 'Efisiensi Bahan Bakar', stars: telemetry.last_receipt_stars_fuel || 0 },
                      { label: 'Kepatuhan Lalu Lintas', stars: telemetry.last_receipt_stars_fines || 0 },
                    ].map((row) => (
                      <div
                        key={row.label}
                        style={{
                          display: 'flex',
                          justifyContent: 'space-between',
                          alignItems: 'center',
                          backgroundColor: 'var(--surface-card-elevated)',
                          padding: '6px 9px',
                          borderRadius: 8,
                        }}
                      >
                        <span style={{ color: '#cbd5e1', fontSize: 10.5 }}>{row.label}</span>
                        <div style={{ display: 'flex', alignItems: 'center', gap: 2 }}>
                          {[1, 2, 3, 4, 5].map((idx) => (
                            <Star
                              key={idx}
                              size={11}
                              fill={idx <= row.stars ? '#fbbf24' : 'transparent'}
                              color={idx <= row.stars ? '#fbbf24' : '#475569'}
                            />
                          ))}
                        </div>
                      </div>
                    ))}
                  </div>
                </div>
              </>
            )
          )}
        </div>
      </div>
    </div>
  );
};
