import React, { useState } from 'react';
import { TelemetryData, FleetVehicle } from '../types/telemetry';
import {
  AlertTriangle,
  ArrowLeft,
  CalendarClock,
  CheckCircle2,
  ChevronLeft,
  ChevronRight,
  Coins,
  Fuel,
  RefreshCw,
  ShieldCheck,
  Truck,
  Warehouse,
  Wrench,
} from 'lucide-react';

interface Props {
  telemetry: TelemetryData;
  onBack: () => void;
  onSelectFleet?: (fleetNumber: string) => void;
}

function isFleetForCurrentGame(fleet: FleetVehicle, gameId?: string, gameName?: string): boolean {
  const isAtsSession =
    (gameId && gameId.toLowerCase().includes('ats')) ||
    (gameName && gameName.toLowerCase().includes('american'));

  const idLower = `${fleet.in_game_id || ''} ${fleet.brand_name || ''} ${fleet.model_name || ''}`.toLowerCase();
  const isKnownAtsTruck =
    idLower.includes('westernstar') ||
    idLower.includes('western star') ||
    idLower.includes('mack') ||
    idLower.includes('kenworth') ||
    idLower.includes('peterbilt') ||
    idLower.includes('freightliner') ||
    idLower.includes('international') ||
    idLower.includes('volvo.vnl') ||
    idLower.includes('volvo vnl');

  const fleetGame = (fleet.game_id || '').toLowerCase();
  const fleetMarkedAts = fleetGame === '2' || fleetGame === 'ats' || isKnownAtsTruck;

  return isAtsSession ? fleetMarkedAts : !fleetMarkedAts;
}

function formatDateTimeId(iso?: string | null): string {
  if (!iso) return '-';
  const d = new Date(iso);
  if (Number.isNaN(d.getTime())) return '-';
  return d.toLocaleString('id-ID', {
    day: '2-digit',
    month: 'short',
    year: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
  });
}

function formatDateOnlyId(iso?: string | null): string {
  if (!iso) return '-';
  const d = new Date(iso);
  if (Number.isNaN(d.getTime())) return '-';
  return d.toLocaleDateString('id-ID', {
    day: '2-digit',
    month: 'short',
    year: 'numeric',
  });
}

function getFleetStatusMeta(fl: FleetVehicle) {
  const rawStatus = (fl.status || 'active').toLowerCase();
  const odo = fl.odometer || 0;
  const overdueByOdo =
    (fl.maint_tires && odo >= fl.maint_tires) ||
    (fl.maint_brakes && odo >= fl.maint_brakes) ||
    (fl.maint_engine && odo >= fl.maint_engine) ||
    (fl.maint_transmission && odo >= fl.maint_transmission);

  if (rawStatus === 'onservice' || rawStatus === 'maintenance') {
    return {
      code: 'onservice',
      label: 'SEDANG DISERVIS',
      shortLabel: 'ON SERVICE',
      bg: 'rgba(56, 189, 248, 0.18)',
      border: 'rgba(56, 189, 248, 0.38)',
      color: '#38bdf8',
      needsMaint: true,
    };
  }

  if (rawStatus === 'need_maintenance' || fl.maintenance_needed || overdueByOdo) {
    return {
      code: 'need_maintenance',
      label: 'PERLU SERVIS (NEED MAINT)',
      shortLabel: 'WAJIB SERVIS',
      bg: 'rgba(248, 113, 113, 0.18)',
      border: 'rgba(248, 113, 113, 0.38)',
      color: '#f87171',
      needsMaint: true,
    };
  }

  if (rawStatus === 'inactive') {
    return {
      code: 'inactive',
      label: 'NONAKTIF',
      shortLabel: 'INACTIVE',
      bg: 'rgba(148, 163, 184, 0.16)',
      border: 'rgba(148, 163, 184, 0.3)',
      color: '#94a3b8',
      needsMaint: false,
    };
  }

  return {
    code: 'active',
    label: 'AKTIF NORMAL',
    shortLabel: 'AKTIF NORMAL',
    bg: 'rgba(52, 211, 153, 0.16)',
    border: 'rgba(52, 211, 153, 0.32)',
    color: '#34d399',
    needsMaint: false,
  };
}

export const GarageApp: React.FC<Props> = ({ telemetry, onBack, onSelectFleet }) => {
  const [tab, setTab] = useState<'overview' | 'armada' | 'fasilitas'>('overview');
  const [selectedFleet, setSelectedFleet] = useState<FleetVehicle | null>(null);
  const [failedImages, setFailedImages] = useState<Record<string, boolean>>({});

  const isAts =
    (telemetry.game_id && telemetry.game_id.toLowerCase().includes('ats')) ||
    (telemetry.game_name && telemetry.game_name.toLowerCase().includes('american'));
  const gameBadge = isAts ? 'ATS FLEET' : 'ETS2 FLEET';

  const allFleets = telemetry.fleets || [];
  const fleets = allFleets.filter((f) =>
    isFleetForCurrentGame(f, telemetry.game_id, telemetry.game_name)
  );

  const rawGarage = telemetry.garage;
  const fleetSlot = rawGarage?.fleet_slot || 1;
  const fleetSlotUsed = rawGarage?.fleet_slot_used ?? allFleets.length;
  const fleetSlotLevel = rawGarage?.fleet_slot_level || rawGarage?.fleet_level || fleetSlot;
  const fuelStock = rawGarage?.fuel_stock || 0;
  const fuelCapacity = rawGarage?.fuel_capacity || 2000;
  const fuelTankLevel =
    rawGarage?.fuel_tank_level ||
    rawGarage?.fuel_level ||
    Math.max(1, Math.round((fuelCapacity - 1000) / 1000));
  const safeboxStock = rawGarage?.safebox_stock ?? telemetry.safebox_tickets ?? 0;
  const safeboxLevel = rawGarage?.safebox_level || 1;
  const maxPenaltyTickets = rawGarage?.max_penalty_tickets || telemetry.max_penalty_tickets || 10 + (safeboxLevel - 1) * 5;

  const fleetOpCost = rawGarage?.fleet_operational_cost ?? 0;
  const fuelOpCost = rawGarage?.fuel_operational_cost ?? 0;
  const safeboxOpCost = rawGarage?.safebox_operational_cost ?? 0;
  const totalOpCost =
    rawGarage?.operational_cost && rawGarage.operational_cost > 0
      ? rawGarage.operational_cost
      : fleetOpCost + fuelOpCost + safeboxOpCost;
  const nextPaymentDate = rawGarage?.next_payment_date || '';

  const hasMatchedFleet = Boolean(telemetry.has_matched_fleet && telemetry.fleet_number);
  const activeFleet = hasMatchedFleet
    ? fleets.find((f) => f.fleet_number === telemetry.fleet_number) ||
      allFleets.find((f) => f.fleet_number === telemetry.fleet_number) ||
      null
    : null;

  const markImgFailed = (key: string) => {
    setFailedImages((prev) => ({ ...prev, [key]: true }));
  };

  // ============================================================
  // VEHICLE DETAIL INSPECTOR VIEW
  // ============================================================
  if (selectedFleet) {
    const isThisActive = telemetry.fleet_number === selectedFleet.fleet_number;
    const showPhoto = Boolean(selectedFleet.photo_url && !failedImages[selectedFleet.fleet_number]);
    const statusMeta = getFleetStatusMeta(selectedFleet);
    const currentOdo =
      isThisActive && telemetry.odometer > (selectedFleet.odometer || 0)
        ? telemetry.odometer
        : selectedFleet.odometer || 0;

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
        {/* Detail Header */}
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
            onClick={() => setSelectedFleet(null)}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: 4,
              fontSize: 12,
              fontWeight: 600,
              color: '#34d399',
              background: 'none',
              border: 'none',
              cursor: 'pointer',
              padding: 0,
            }}
          >
            <ArrowLeft size={14} />
            <span>Armada ({fleets.length})</span>
          </button>
          <span style={{ fontSize: 13.5, fontWeight: 700, color: '#ffffff' }}>Inspector Armada</span>
          <span
            style={{
              fontSize: 9.5,
              fontWeight: 800,
              padding: '2px 7px',
              borderRadius: 6,
              backgroundColor: isThisActive ? 'rgba(52, 211, 153, 0.18)' : statusMeta.bg,
              color: isThisActive ? '#34d399' : statusMeta.color,
            }}
          >
            {isThisActive ? 'IN USE' : statusMeta.shortLabel}
          </span>
        </div>

        <div data-scroll-container="true" className="scroll-area" style={{ paddingTop: 12 }}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: 11, paddingBottom: 24 }}>
            {/* Vehicle Identity Card with Photo */}
            <div
              style={{
                backgroundColor: 'var(--surface-card)',
                borderRadius: 16,
                border: '1px solid var(--border-subtle)',
                overflow: 'hidden',
                display: 'flex',
                flexDirection: 'column',
              }}
            >
              {showPhoto && (
                <div
                  style={{
                    width: '100%',
                    height: 110,
                    backgroundColor: '#090d16',
                    position: 'relative',
                    overflow: 'hidden',
                  }}
                >
                  <img
                    src={selectedFleet.photo_url}
                    alt={`${selectedFleet.brand_name} ${selectedFleet.model_name}`}
                    onError={() => markImgFailed(selectedFleet.fleet_number)}
                    style={{
                      width: '100%',
                      height: '100%',
                      objectFit: 'cover',
                      display: 'block',
                    }}
                  />
                  <div
                    style={{
                      position: 'absolute',
                      inset: 0,
                      background: 'linear-gradient(180deg, transparent 35%, rgba(15,23,42,0.95) 100%)',
                    }}
                  />
                </div>
              )}

              <div style={{ padding: '12px 14px', display: 'flex', flexDirection: 'column', gap: 10 }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', gap: 8 }}>
                  <div style={{ minWidth: 0, flex: 1 }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 6, flexWrap: 'wrap' }}>
                      <span
                        className="tabular-nums"
                        style={{
                          fontSize: 11,
                          fontWeight: 800,
                          color: '#38bdf8',
                          letterSpacing: '0.04em',
                        }}
                      >
                        [{selectedFleet.fleet_number}]
                      </span>
                      <span
                        style={{
                          fontSize: 9,
                          fontWeight: 800,
                          padding: '1px 6px',
                          borderRadius: 5,
                          backgroundColor: statusMeta.bg,
                          border: `1px solid ${statusMeta.border}`,
                          color: statusMeta.color,
                        }}
                      >
                        {statusMeta.label}
                      </span>
                      {selectedFleet.has_insurance && (
                        <span
                          style={{
                            fontSize: 9,
                            fontWeight: 800,
                            padding: '1px 6px',
                            borderRadius: 5,
                            backgroundColor: 'rgba(52, 211, 153, 0.16)',
                            color: '#34d399',
                          }}
                        >
                          INSURED
                        </span>
                      )}
                    </div>
                    <div style={{ fontSize: 15.5, fontWeight: 800, color: '#ffffff', marginTop: 3 }}>
                      {selectedFleet.brand_name} {selectedFleet.model_name}
                    </div>
                    {selectedFleet.custom_name && (
                      <div style={{ fontSize: 11, color: '#94a3b8', fontStyle: 'italic', marginTop: 1 }}>
                        "{selectedFleet.custom_name}"
                      </div>
                    )}
                  </div>
                </div>

                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 7 }}>
                  <div
                    style={{
                      backgroundColor: 'var(--surface-card-elevated)',
                      padding: '6px 9px',
                      borderRadius: 9,
                    }}
                  >
                    <div style={{ fontSize: 9.5, color: '#94a3b8', fontWeight: 600 }}>ODOMETER SAAT INI</div>
                    <div className="tabular-nums" style={{ fontSize: 12, fontWeight: 800, color: '#ffffff', marginTop: 1 }}>
                      {Math.round(currentOdo).toLocaleString('id-ID')} km
                    </div>
                  </div>
                  <div
                    style={{
                      backgroundColor: 'var(--surface-card-elevated)',
                      padding: '6px 9px',
                      borderRadius: 9,
                    }}
                  >
                    <div style={{ fontSize: 9.5, color: '#94a3b8', fontWeight: 600 }}>CHASSIS / TIPE</div>
                    <div style={{ fontSize: 12, fontWeight: 800, color: '#ffffff', marginTop: 1 }}>
                      {selectedFleet.wheels || '4x2'} • {selectedFleet.model_type || 'truck'}
                    </div>
                  </div>
                </div>
              </div>
            </div>

            {/* 1. Status & Jadwal Maintenance Armada Card (models/Fleet.ts) */}
            <div
              style={{
                backgroundColor: 'var(--surface-card)',
                borderRadius: 16,
                border: `1px solid ${statusMeta.needsMaint ? statusMeta.border : 'var(--border-subtle)'}`,
                padding: '12px 14px',
                display: 'flex',
                flexDirection: 'column',
                gap: 8,
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                  <CalendarClock size={13} color={statusMeta.color} />
                  <span style={{ fontSize: 10, fontWeight: 800, color: statusMeta.color, letterSpacing: '0.05em' }}>
                    STATUS & JADWAL MAINTENANCE
                  </span>
                </div>
                {selectedFleet.maintenance_needed ? (
                  <span
                    style={{
                      fontSize: 9,
                      fontWeight: 800,
                      padding: '2px 6px',
                      borderRadius: 5,
                      backgroundColor: 'rgba(248, 113, 113, 0.2)',
                      color: '#f87171',
                    }}
                  >
                    WAJIB SERVIS
                  </span>
                ) : (
                  <span
                    style={{
                      fontSize: 9,
                      fontWeight: 800,
                      padding: '2px 6px',
                      borderRadius: 5,
                      backgroundColor: 'rgba(52, 211, 153, 0.16)',
                      color: '#34d399',
                    }}
                  >
                    SIAP JALAN
                  </span>
                )}
              </div>

              <div className="tabular-nums" style={{ display: 'flex', flexDirection: 'column', gap: 5, fontSize: 11 }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <span style={{ color: '#94a3b8' }}>Status Unit (`status`)</span>
                  <span style={{ fontWeight: 800, color: statusMeta.color }}>{statusMeta.label}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <span style={{ color: '#94a3b8' }}>Perlu Servis (`maintenance_needed`)</span>
                  <span
                    style={{
                      fontWeight: 700,
                      color: selectedFleet.maintenance_needed ? '#f87171' : '#34d399',
                    }}
                  >
                    {selectedFleet.maintenance_needed ? 'Ya (Harus Servis)' : 'Tidak (Normal)'}
                  </span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <span style={{ color: '#94a3b8' }}>Mulai Servis</span>
                  <span style={{ fontWeight: 700, color: '#e2e8f0' }}>
                    {formatDateTimeId(selectedFleet.maintenance_start_date)}
                  </span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <span style={{ color: '#94a3b8' }}>Estimasi Selesai</span>
                  <span style={{ fontWeight: 700, color: '#38bdf8' }}>
                    {formatDateTimeId(selectedFleet.maintenance_end_date)}
                  </span>
                </div>
              </div>
            </div>

            {/* 2. Permanent Component Wear Card (Unfixable Wear: ALREADY 0-100% in MongoDB!) */}
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
              <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                <Wrench size={13} color="#34d399" />
                <span style={{ fontSize: 10, fontWeight: 800, color: '#34d399', letterSpacing: '0.05em' }}>
                  KEAUSAN KOMPONEN PERMANEN (UNFIXABLE WEAR)
                </span>
              </div>

              {[
                { label: 'Mesin (Engine)', wear: Number(selectedFleet.unfix_engine || 0) },
                { label: 'Transmisi (Gearbox)', wear: Number(selectedFleet.unfix_transmission || 0) },
                { label: 'Sistem Rem (Brakes)', wear: Number(selectedFleet.unfix_brakes || 0) },
                { label: 'Ban (Tires)', wear: Number(selectedFleet.unfix_tires || 0) },
              ].map((part) => {
                const wearPct = Math.max(0, Math.min(100, part.wear));
                const barColor =
                  wearPct >= 15 ? '#ef4444' : wearPct >= 5 ? '#fbbf24' : '#34d399';
                return (
                  <div key={part.label}>
                    <div
                      style={{
                        display: 'flex',
                        justifyContent: 'space-between',
                        fontSize: 11,
                        color: '#cbd5e1',
                        marginBottom: 3,
                      }}
                    >
                      <span>{part.label}</span>
                      <span className="tabular-nums" style={{ fontWeight: 800, color: barColor }}>
                        {wearPct.toFixed(2)}%
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
                          width: `${Math.min(100, Math.max(wearPct > 0 ? 2 : 0, wearPct))}%`,
                          backgroundColor: barColor,
                        }}
                      />
                    </div>
                  </div>
                );
              })}
            </div>

            {/* 3. Next Periodic Maintenance Target Odometer Card (Service Berkala Berikutnya) */}
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
                <span style={{ fontSize: 10, fontWeight: 800, color: '#38bdf8', letterSpacing: '0.05em' }}>
                  SERVIS BERKALA BERIKUTNYA
                </span>
                <span className="tabular-nums" style={{ fontSize: 9.5, color: '#94a3b8', fontWeight: 600 }}>
                  Odo: {Math.round(currentOdo).toLocaleString('id-ID')} km
                </span>
              </div>

              <div className="tabular-nums" style={{ display: 'flex', flexDirection: 'column', gap: 7 }}>
                {[
                  {
                    label: 'Rotasi / Ganti Ban',
                    targetKm: selectedFleet.maint_tires || 20000,
                    lastKm: selectedFleet.last_maint_tires || 0,
                    intervalKm: 20000,
                  },
                  {
                    label: 'Kampas Sistem Rem',
                    targetKm: selectedFleet.maint_brakes || 35000,
                    lastKm: selectedFleet.last_maint_brakes || 0,
                    intervalKm: 35000,
                  },
                  {
                    label: 'Oli & Komponen Mesin',
                    targetKm: selectedFleet.maint_engine || 50000,
                    lastKm: selectedFleet.last_maint_engine || 0,
                    intervalKm: 50000,
                  },
                  {
                    label: 'Oli & Gearbox Transmisi',
                    targetKm: selectedFleet.maint_transmission || 70000,
                    lastKm: selectedFleet.last_maint_transmission || 0,
                    intervalKm: 70000,
                  },
                ].map((m) => {
                  const remainKm = m.targetKm - currentOdo;
                  const isOverdue = remainKm <= 0;
                  const isSoon = !isOverdue && remainKm <= 3000;
                  const baseStartKm = m.lastKm > 0 ? m.lastKm : Math.max(0, m.targetKm - m.intervalKm);
                  const spanKm = Math.max(1000, m.targetKm - baseStartKm);
                  const usedProgress = Math.min(100, Math.max(0, ((currentOdo - baseStartKm) / spanKm) * 100));
                  const statusColor = isOverdue ? '#f87171' : isSoon ? '#fbbf24' : '#34d399';

                  return (
                    <div
                      key={m.label}
                      style={{
                        backgroundColor: 'var(--surface-card-elevated)',
                        borderRadius: 10,
                        padding: '8px 10px',
                        border: isOverdue ? '1px solid rgba(248, 113, 113, 0.35)' : '1px solid transparent',
                        display: 'flex',
                        flexDirection: 'column',
                        gap: 4,
                      }}
                    >
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                        <span style={{ fontSize: 11, fontWeight: 700, color: '#f8fafc' }}>{m.label}</span>
                        <span style={{ fontSize: 10.5, fontWeight: 800, color: statusColor }}>
                          {isOverdue
                            ? `LEWAT ${Math.round(Math.abs(remainKm)).toLocaleString('id-ID')} KM!`
                            : `Sisa ${Math.round(remainKm).toLocaleString('id-ID')} km`}
                        </span>
                      </div>

                      <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 9.5, color: '#94a3b8' }}>
                        <span>Target Servis di Odometer:</span>
                        <span style={{ fontWeight: 700, color: '#cbd5e1' }}>
                          {Math.round(m.targetKm).toLocaleString('id-ID')} km
                        </span>
                      </div>

                      <div
                        style={{
                          width: '100%',
                          height: 4,
                          borderRadius: 2,
                          backgroundColor: 'rgba(15, 23, 42, 0.9)',
                          overflow: 'hidden',
                          marginTop: 1,
                        }}
                      >
                        <div
                          style={{
                            width: `${usedProgress}%`,
                            height: '100%',
                            borderRadius: 2,
                            backgroundColor: statusColor,
                          }}
                        />
                      </div>
                    </div>
                  );
                })}
              </div>
            </div>

            {/* Manual Link Override Button */}
            {!isThisActive && onSelectFleet && (
              <button
                onClick={() => {
                  onSelectFleet(selectedFleet.fleet_number);
                  setSelectedFleet(null);
                }}
                style={{
                  width: '100%',
                  padding: '10px 0',
                  borderRadius: 11,
                  backgroundColor: '#10b981',
                  color: '#020617',
                  fontWeight: 800,
                  fontSize: 12,
                  border: 'none',
                  cursor: 'pointer',
                }}
              >
                Gunakan Armada [{selectedFleet.fleet_number}] Ini
              </button>
            )}
          </div>
        </div>
      </div>
    );
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
            color: '#34d399',
            background: 'none',
            border: 'none',
            cursor: 'pointer',
            padding: 0,
          }}
        >
          <ChevronLeft size={15} />
          <span>Home</span>
        </button>

        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
          <span style={{ fontSize: 13.5, fontWeight: 700, color: '#ffffff' }}>My Garage</span>
          <span
            style={{
              fontSize: 9,
              fontWeight: 800,
              padding: '2px 6px',
              borderRadius: 5,
              backgroundColor: 'rgba(52, 211, 153, 0.16)',
              color: '#34d399',
              border: '1px solid rgba(52, 211, 153, 0.3)',
            }}
          >
            {gameBadge}
          </span>
        </div>

        <button
          onClick={() => window.nismara?.syncAccount?.()}
          title="Sinkronisasi Ulang Garasi"
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            width: 26,
            height: 26,
            borderRadius: 7,
            backgroundColor: 'rgba(52, 211, 153, 0.15)',
            border: '1px solid rgba(52, 211, 153, 0.3)',
            color: '#34d399',
            cursor: 'pointer',
            padding: 0,
          }}
        >
          <RefreshCw size={13} />
        </button>
      </div>

      {/* Segmented Control: [Overview] [Armada (X)] [Fasilitas] */}
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
        {(['overview', 'armada', 'fasilitas'] as const).map((t) => {
          const label =
            t === 'overview' ? 'Overview' : t === 'armada' ? `Armada (${fleets.length})` : 'Fasilitas';
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
                backgroundColor: active ? '#34d399' : 'transparent',
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
          {tab === 'overview' && (
            <>
              {/* Active In-Game Vehicle Hero Card */}
              <div
                style={{
                  backgroundColor: 'var(--surface-card)',
                  borderRadius: 16,
                  border: '1px solid var(--border-subtle)',
                  borderLeft: hasMatchedFleet ? '4px solid #34d399' : '4px solid #fbbf24',
                  padding: '13px 14px',
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
                      color: hasMatchedFleet ? '#34d399' : '#fbbf24',
                      letterSpacing: '0.05em',
                    }}
                  >
                    ACTIVE IN-GAME VEHICLE
                  </span>
                  <span
                    style={{
                      fontSize: 9.5,
                      fontWeight: 800,
                      padding: '2px 7px',
                      borderRadius: 6,
                      backgroundColor: hasMatchedFleet
                        ? 'rgba(52, 211, 153, 0.16)'
                        : 'rgba(251, 191, 36, 0.16)',
                      color: hasMatchedFleet ? '#34d399' : '#fbbf24',
                    }}
                  >
                    {hasMatchedFleet ? 'MATCHED FLEET' : 'TRUK SEWA'}
                  </span>
                </div>

                {hasMatchedFleet && activeFleet ? (
                  <>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
                      <div
                        style={{
                          width: 48,
                          height: 38,
                          borderRadius: 8,
                          overflow: 'hidden',
                          backgroundColor: '#090d16',
                          border: '1px solid rgba(255,255,255,0.1)',
                          flexShrink: 0,
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'center',
                        }}
                      >
                        {activeFleet.photo_url && !failedImages[activeFleet.fleet_number] ? (
                          <img
                            src={activeFleet.photo_url}
                            alt={activeFleet.fleet_number}
                            onError={() => markImgFailed(activeFleet.fleet_number)}
                            style={{ width: '100%', height: '100%', objectFit: 'cover' }}
                          />
                        ) : (
                          <Truck size={18} color="#34d399" />
                        )}
                      </div>
                      <div style={{ minWidth: 0, flex: 1 }}>
                        <div
                          style={{
                            fontSize: 14,
                            fontWeight: 800,
                            color: '#ffffff',
                            whiteSpace: 'nowrap',
                            overflow: 'hidden',
                            textOverflow: 'ellipsis',
                          }}
                        >
                          [{activeFleet.fleet_number}] {activeFleet.brand_name} {activeFleet.model_name}
                        </div>
                        <div className="tabular-nums" style={{ fontSize: 10.5, color: '#94a3b8', marginTop: 2 }}>
                          Odo: {Math.round(telemetry.odometer || activeFleet.odometer || 0).toLocaleString('id-ID')} km •{' '}
                          {getFleetStatusMeta(activeFleet).shortLabel}
                        </div>
                      </div>
                    </div>

                    <button
                      onClick={() => setSelectedFleet(activeFleet)}
                      style={{
                        width: '100%',
                        padding: '7px 0',
                        borderRadius: 9,
                        backgroundColor: 'rgba(52, 211, 153, 0.15)',
                        color: '#34d399',
                        border: '1px solid rgba(52, 211, 153, 0.3)',
                        fontSize: 11,
                        fontWeight: 700,
                        cursor: 'pointer',
                      }}
                    >
                      Buka Inspector Armada Ini →
                    </button>
                  </>
                ) : (
                  <>
                    <div style={{ fontSize: 13.5, fontWeight: 800, color: '#fbbf24' }}>
                      {telemetry.truck_name
                        ? `[UNREGISTERED] ${telemetry.truck_brand || ''} ${telemetry.truck_name}`
                        : 'Tidak Ada Truk Terdeteksi'}
                    </div>
                    <div style={{ fontSize: 10.5, color: '#94a3b8', fontFamily: 'var(--font-mono)' }}>
                      ID: {telemetry.truck_in_game_id || 'N/A'}
                    </div>
                    <div style={{ fontSize: 11, color: '#cbd5e1', lineHeight: 1.35 }}>
                      Truk ini berstatus sewa (Quick Job) atau belum terdaftar di garasi {isAts ? 'ATS' : 'ETS2'} kamu.
                    </div>
                  </>
                )}
              </div>

              {/* Garage Capacity & Facilities Summary Card */}
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
                <div style={{ fontSize: 10, fontWeight: 800, color: '#38bdf8', letterSpacing: '0.05em' }}>
                  KAPASITAS & LEVEL GARASI ({isAts ? 'ATS' : 'ETS2'})
                </div>

                <div className="tabular-nums" style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 8 }}>
                  <div
                    style={{
                      backgroundColor: 'var(--surface-card-elevated)',
                      padding: '8px 10px',
                      borderRadius: 10,
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <span style={{ fontSize: 9.5, color: '#94a3b8', fontWeight: 600 }}>SLOT ARMADA</span>
                      <span style={{ fontSize: 9, fontWeight: 800, color: '#38bdf8' }}>Lv.{fleetSlotLevel}</span>
                    </div>
                    <div style={{ fontSize: 13.5, fontWeight: 800, color: '#ffffff', marginTop: 2 }}>
                      {fleets.length} Unit {isAts ? 'ATS' : 'ETS2'}
                    </div>
                    <div style={{ fontSize: 9.5, color: '#64748b', marginTop: 1 }}>
                      Terpakai VTC: {fleetSlotUsed} / {fleetSlot} Slot
                    </div>
                  </div>
                  <div
                    style={{
                      backgroundColor: 'var(--surface-card-elevated)',
                      padding: '8px 10px',
                      borderRadius: 10,
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <span style={{ fontSize: 9.5, color: '#94a3b8', fontWeight: 600 }}>TANGKI SOLAR</span>
                      <span style={{ fontSize: 9, fontWeight: 800, color: '#34d399' }}>Lv.{fuelTankLevel}</span>
                    </div>
                    <div style={{ fontSize: 13.5, fontWeight: 800, color: '#34d399', marginTop: 2 }}>
                      {Math.round(fuelStock).toLocaleString('id-ID')} L
                    </div>
                    <div style={{ fontSize: 9.5, color: '#64748b', marginTop: 1 }}>
                      Maks {Math.round(fuelCapacity).toLocaleString('id-ID')} L
                    </div>
                  </div>
                </div>

                <div
                  className="tabular-nums"
                  style={{
                    backgroundColor: 'var(--surface-card-elevated)',
                    padding: '8px 10px',
                    borderRadius: 10,
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                  }}
                >
                  <div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                      <span style={{ fontSize: 9.5, color: '#94a3b8', fontWeight: 600 }}>BRANKAS SAFEBOX</span>
                      <span style={{ fontSize: 9, fontWeight: 800, color: '#facc15' }}>Lv.{safeboxLevel}</span>
                    </div>
                    <div style={{ fontSize: 13, fontWeight: 800, color: '#facc15', marginTop: 1 }}>
                      {safeboxStock.toLocaleString('id-ID')} Tiket Hapus Penalti
                    </div>
                  </div>
                  <ShieldCheck size={18} color="#facc15" />
                </div>
              </div>

              {/* Operational Cost Summary Card */}
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
                    <Coins size={13} color="#fbbf24" />
                    <span style={{ fontSize: 10, fontWeight: 800, color: '#fbbf24', letterSpacing: '0.05em' }}>
                      BIAYA OPERASIONAL GARASI
                    </span>
                  </div>
                  {nextPaymentDate && (
                    <span className="tabular-nums" style={{ fontSize: 9.5, color: '#94a3b8' }}>
                      Tagihan: {formatDateOnlyId(nextPaymentDate)}
                    </span>
                  )}
                </div>

                <div
                  className="tabular-nums"
                  style={{
                    backgroundColor: 'var(--surface-card-elevated)',
                    borderRadius: 10,
                    padding: '8px 10px',
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    border: '1px solid rgba(251, 191, 36, 0.25)',
                  }}
                >
                  <span style={{ fontSize: 11, fontWeight: 700, color: '#fde68a' }}>Total Operasional</span>
                  <span style={{ fontSize: 14, fontWeight: 900, color: '#fde047' }}>
                    {Math.round(totalOpCost).toLocaleString('id-ID')} NC
                  </span>
                </div>

                <div className="tabular-nums" style={{ display: 'flex', flexDirection: 'column', gap: 4, fontSize: 10.5 }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', color: '#cbd5e1' }}>
                    <span>• Slot Armada (Lv.{fleetSlotLevel})</span>
                    <span style={{ fontWeight: 700, color: '#ffffff' }}>
                      {Math.round(fleetOpCost).toLocaleString('id-ID')} NC
                    </span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', color: '#cbd5e1' }}>
                    <span>• Tangki Solar (Lv.{fuelTankLevel})</span>
                    <span style={{ fontWeight: 700, color: '#ffffff' }}>
                      {Math.round(fuelOpCost).toLocaleString('id-ID')} NC
                    </span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', color: '#cbd5e1' }}>
                    <span>• Brankas Safebox (Lv.{safeboxLevel})</span>
                    <span style={{ fontWeight: 700, color: '#ffffff' }}>
                      {Math.round(safeboxOpCost).toLocaleString('id-ID')} NC
                    </span>
                  </div>
                </div>
              </div>
            </>
          )}

          {tab === 'armada' && (
            <>
              {fleets.length === 0 ? (
                <div
                  style={{
                    backgroundColor: 'var(--surface-card)',
                    borderRadius: 16,
                    border: '1px solid var(--border-subtle)',
                    padding: '28px 14px',
                    textAlign: 'center',
                    color: '#94a3b8',
                    fontSize: 11.5,
                  }}
                >
                  Tidak ada armada untuk game {isAts ? 'American Truck Simulator' : 'Euro Truck Simulator 2'} di garasi Anda.
                </div>
              ) : (
                fleets.map((fl) => {
                  const isThisActive = telemetry.fleet_number === fl.fleet_number;
                  const showThumb = Boolean(fl.photo_url && !failedImages[fl.fleet_number]);
                  const st = getFleetStatusMeta(fl);

                  return (
                    <div
                      key={fl.fleet_number}
                      onClick={() => setSelectedFleet(fl)}
                      style={{
                        backgroundColor: 'var(--surface-card)',
                        borderRadius: 14,
                        border: isThisActive
                          ? '1.5px solid #34d399'
                          : st.needsMaint
                          ? `1px solid ${st.border}`
                          : '1px solid var(--border-subtle)',
                        padding: '10px 11px',
                        cursor: 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'space-between',
                        gap: 10,
                      }}
                    >
                      <div style={{ display: 'flex', alignItems: 'center', gap: 10, minWidth: 0, flex: 1 }}>
                        <div
                          style={{
                            width: 44,
                            height: 36,
                            borderRadius: 8,
                            overflow: 'hidden',
                            backgroundColor: '#090d16',
                            border: '1px solid rgba(255,255,255,0.1)',
                            flexShrink: 0,
                            display: 'flex',
                            alignItems: 'center',
                            justifyContent: 'center',
                          }}
                        >
                          {showThumb ? (
                            <img
                              src={fl.photo_url}
                              alt={fl.fleet_number}
                              onError={() => markImgFailed(fl.fleet_number)}
                              style={{ width: '100%', height: '100%', objectFit: 'cover' }}
                            />
                          ) : (
                            <Truck size={16} color={isThisActive ? '#34d399' : '#38bdf8'} />
                          )}
                        </div>

                        <div style={{ minWidth: 0, flex: 1 }}>
                          <div style={{ display: 'flex', alignItems: 'center', gap: 5 }}>
                            <span
                              className="tabular-nums"
                              style={{
                                fontSize: 11.5,
                                fontWeight: 800,
                                color: isThisActive ? '#34d399' : '#38bdf8',
                              }}
                            >
                              [{fl.fleet_number}]
                            </span>
                            <span
                              style={{
                                fontSize: 12,
                                fontWeight: 700,
                                color: '#ffffff',
                                whiteSpace: 'nowrap',
                                overflow: 'hidden',
                                textOverflow: 'ellipsis',
                              }}
                            >
                              {fl.brand_name} {fl.model_name}
                            </span>
                          </div>
                          <div className="tabular-nums" style={{ fontSize: 10, color: '#94a3b8', marginTop: 2 }}>
                            {Math.round(fl.odometer || 0).toLocaleString('id-ID')} km • {fl.wheels || '4x2'}
                            {fl.custom_name ? ` • "${fl.custom_name}"` : ''}
                          </div>
                        </div>
                      </div>

                      <div style={{ display: 'flex', alignItems: 'center', gap: 5, flexShrink: 0 }}>
                        {isThisActive ? (
                          <span
                            style={{
                              fontSize: 9,
                              fontWeight: 800,
                              padding: '2px 6px',
                              borderRadius: 5,
                              backgroundColor: 'rgba(52, 211, 153, 0.2)',
                              color: '#34d399',
                            }}
                          >
                            IN USE
                          </span>
                        ) : (
                          <span
                            style={{
                              fontSize: 8.5,
                              fontWeight: 800,
                              padding: '2px 5px',
                              borderRadius: 5,
                              backgroundColor: st.bg,
                              color: st.color,
                              display: 'flex',
                              alignItems: 'center',
                              gap: 3,
                            }}
                          >
                            {st.needsMaint ? <AlertTriangle size={9} /> : <CheckCircle2 size={9} />}
                            {st.shortLabel}
                          </span>
                        )}
                        <ChevronRight size={15} style={{ color: '#94a3b8' }} />
                      </div>
                    </div>
                  );
                })
              )}
            </>
          )}

          {tab === 'fasilitas' && (
            <>
              {/* 1. Rincian Biaya Operasional Garasi (NC) */}
              <div
                style={{
                  backgroundColor: 'var(--surface-card)',
                  borderRadius: 15,
                  border: '1px solid rgba(251, 191, 36, 0.3)',
                  padding: '13px 14px',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: 9,
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                    <Coins size={14} style={{ color: '#fbbf24' }} />
                    <span style={{ fontSize: 10.5, fontWeight: 800, color: '#fbbf24', letterSpacing: '0.04em' }}>
                      RINCIAN OPERASIONAL GARASI
                    </span>
                  </div>
                  <span
                    style={{
                      fontSize: 9,
                      fontWeight: 800,
                      padding: '2px 6px',
                      borderRadius: 5,
                      backgroundColor: 'rgba(52, 211, 153, 0.16)',
                      color: '#34d399',
                    }}
                  >
                    {(rawGarage?.status || 'OPERATIONAL').toUpperCase()}
                  </span>
                </div>

                <div
                  className="tabular-nums"
                  style={{
                    backgroundColor: 'var(--surface-card-elevated)',
                    borderRadius: 11,
                    padding: '9px 11px',
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                  }}
                >
                  <div>
                    <div style={{ fontSize: 9.5, color: '#94a3b8', fontWeight: 600 }}>TOTAL OPERATIONAL COST</div>
                    <div style={{ fontSize: 17, fontWeight: 900, color: '#fde047', marginTop: 1 }}>
                      {Math.round(totalOpCost).toLocaleString('id-ID')} NC
                    </div>
                  </div>
                  {nextPaymentDate && (
                    <div style={{ textAlign: 'right' }}>
                      <div style={{ fontSize: 9, color: '#94a3b8' }}>Jatuh Tempo</div>
                      <div style={{ fontSize: 10.5, fontWeight: 700, color: '#e2e8f0', marginTop: 1 }}>
                        {formatDateOnlyId(nextPaymentDate)}
                      </div>
                    </div>
                  )}
                </div>

                <div className="tabular-nums" style={{ display: 'flex', flexDirection: 'column', gap: 5, fontSize: 11 }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', color: '#cbd5e1' }}>
                    <span>Slot Armada (Lv.{fleetSlotLevel})</span>
                    <span style={{ fontWeight: 700, color: '#38bdf8' }}>
                      {Math.round(fleetOpCost).toLocaleString('id-ID')} NC
                    </span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', color: '#cbd5e1' }}>
                    <span>Tangki Solar (Lv.{fuelTankLevel})</span>
                    <span style={{ fontWeight: 700, color: '#34d399' }}>
                      {Math.round(fuelOpCost).toLocaleString('id-ID')} NC
                    </span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', color: '#cbd5e1' }}>
                    <span>Brankas Safebox (Lv.{safeboxLevel})</span>
                    <span style={{ fontWeight: 700, color: '#facc15' }}>
                      {Math.round(safeboxOpCost).toLocaleString('id-ID')} NC
                    </span>
                  </div>
                </div>
              </div>

              {/* 2. Kapasitas Slot Armada (fleetSlotLevel & fleet_operational_cost) */}
              <div
                style={{
                  backgroundColor: 'var(--surface-card)',
                  borderRadius: 15,
                  border: '1px solid var(--border-subtle)',
                  padding: '13px 14px',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: 7,
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                    <Truck size={14} style={{ color: '#38bdf8' }} />
                    <span style={{ fontSize: 10.5, fontWeight: 800, color: '#38bdf8', letterSpacing: '0.04em' }}>
                      SLOT ARMADA GARASI (Lv.{fleetSlotLevel})
                    </span>
                  </div>
                  <span className="tabular-nums" style={{ fontSize: 10, fontWeight: 700, color: '#94a3b8' }}>
                    Op: {Math.round(fleetOpCost).toLocaleString('id-ID')} NC
                  </span>
                </div>

                <div className="tabular-nums" style={{ fontSize: 17, fontWeight: 800, color: '#ffffff' }}>
                  {fleetSlotUsed}{' '}
                  <span style={{ fontSize: 11.5, color: '#94a3b8', fontWeight: 600 }}>
                    / {fleetSlot} Slot Kapasitas VTC ({fleets.length} Unit {isAts ? 'ATS' : 'ETS2'})
                  </span>
                </div>
              </div>

              {/* 3. Tangki Bahan Bakar (fuelTankLevel & fuel_operational_cost) */}
              <div
                style={{
                  backgroundColor: 'var(--surface-card)',
                  borderRadius: 15,
                  border: '1px solid var(--border-subtle)',
                  padding: '13px 14px',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: 8,
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                    <Fuel size={14} style={{ color: '#34d399' }} />
                    <span style={{ fontSize: 10.5, fontWeight: 800, color: '#34d399', letterSpacing: '0.04em' }}>
                      TANGKI SOLAR GARASI (Lv.{fuelTankLevel})
                    </span>
                  </div>
                  <span className="tabular-nums" style={{ fontSize: 10, fontWeight: 700, color: '#94a3b8' }}>
                    Op: {Math.round(fuelOpCost).toLocaleString('id-ID')} NC
                  </span>
                </div>
                <div className="tabular-nums" style={{ fontSize: 17, fontWeight: 800, color: '#ffffff' }}>
                  {Math.round(fuelStock).toLocaleString('id-ID')}{' '}
                  <span style={{ fontSize: 11.5, color: '#94a3b8', fontWeight: 600 }}>
                    / {Math.round(fuelCapacity).toLocaleString('id-ID')} Liter
                  </span>
                </div>
                <div
                  style={{
                    width: '100%',
                    height: 6,
                    borderRadius: 3,
                    backgroundColor: 'rgba(15, 23, 42, 0.9)',
                    overflow: 'hidden',
                  }}
                >
                  <div
                    style={{
                      height: '100%',
                      borderRadius: 3,
                      backgroundColor: '#34d399',
                      width: `${Math.min(100, (fuelStock / Math.max(1, fuelCapacity)) * 100)}%`,
                    }}
                  />
                </div>
              </div>

              {/* 4. Brankas Safebox (safeboxLevel & safebox_operational_cost) */}
              <div
                style={{
                  backgroundColor: 'var(--surface-card)',
                  borderRadius: 15,
                  border: '1px solid var(--border-subtle)',
                  padding: '13px 14px',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: 6,
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                    <Warehouse size={14} style={{ color: '#facc15' }} />
                    <span style={{ fontSize: 10.5, fontWeight: 800, color: '#facc15', letterSpacing: '0.04em' }}>
                      BRANKAS SAFEBOX (Lv.{safeboxLevel})
                    </span>
                  </div>
                  <span className="tabular-nums" style={{ fontSize: 10, fontWeight: 700, color: '#94a3b8' }}>
                    Op: {Math.round(safeboxOpCost).toLocaleString('id-ID')} NC
                  </span>
                </div>
                <div className="tabular-nums" style={{ fontSize: 17, fontWeight: 800, color: '#facc15' }}>
                  {safeboxStock.toLocaleString('id-ID')}{' '}
                  <span style={{ fontSize: 11.5, color: '#94a3b8', fontWeight: 600 }}>
                    Tiket (Kapasitas Reguler {maxPenaltyTickets})
                  </span>
                </div>
                <div style={{ fontSize: 11, color: '#94a3b8', lineHeight: 1.35 }}>
                  1 Tiket Safebox menghapus 1 Poin Penalti secara gratis melalui aplikasi Penalty Points.
                </div>
              </div>
            </>
          )}
        </div>
      </div>
    </div>
  );
};
