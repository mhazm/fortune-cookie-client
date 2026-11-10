import React, { useState } from 'react';
import { TelemetryData } from '../types/telemetry';
import { ShieldAlert, Ticket, Coins, BookOpen, RefreshCw, CheckCircle2, AlertCircle } from 'lucide-react';

interface Props {
  telemetry: TelemetryData;
  onBack: () => void;
}

export const PenaltyApp: React.FC<Props> = ({ telemetry, onBack }) => {
  const [tab, setTab] = useState<'nc' | 'ticket' | 'rules'>('nc');
  const [pointsToPay, setPointsToPay] = useState(1);
  const [ticketsToUse, setTicketsToUse] = useState(1);

  const currentPts = telemetry.penalty_points || 0;
  const basePrice = telemetry.point_price ?? 3000;
  const discountBooster = telemetry.discount_booster ?? (telemetry.is_booster ? 500 : 0);
  const discountNPlus = telemetry.discount_nismara_plus ?? (telemetry.nismara_plus_active ? 500 : 0);
  const totalDiscount = telemetry.total_discount ?? (discountBooster + discountNPlus);
  const finalPointPrice = telemetry.final_point_price ?? Math.max(0, basePrice - totalDiscount);

  const safeboxAvail =
    telemetry.garage?.safebox_stock !== undefined && telemetry.garage.safebox_stock > 0
      ? telemetry.garage.safebox_stock
      : telemetry.safebox_tickets || 0;
  const safeboxCap = telemetry.max_penalty_tickets || 10;

  // Clamp stepper values dynamically against live state
  const maxPayablePts = Math.max(1, currentPts);
  const clampedPtsToPay = Math.min(Math.max(1, pointsToPay), maxPayablePts);
  const totalNcCost = clampedPtsToPay * finalPointPrice;
  const canPayNc = currentPts > 0 && telemetry.total_nc >= totalNcCost;

  const maxUsableTickets = Math.max(1, Math.min(safeboxAvail, currentPts));
  const clampedTicketsToUse = Math.min(Math.max(1, ticketsToUse), maxUsableTickets);
  const canUseTicket = currentPts > 0 && safeboxAvail >= clampedTicketsToUse;

  // 4-Tier Penalty Status (matching tablet_gui.cpp:3030-3041)
  let statusText = 'Aman';
  let statusColor = '#34d399';
  let statusBg = 'rgba(52, 211, 153, 0.16)';
  if (currentPts >= 50) {
    statusText = 'Penalty 3 (Kritis)';
    statusColor = '#f87171';
    statusBg = 'rgba(248, 113, 113, 0.18)';
  } else if (currentPts >= 25) {
    statusText = 'Penalty 2 (Peringatan Keras)';
    statusColor = '#fb923c';
    statusBg = 'rgba(251, 146, 60, 0.18)';
  } else if (currentPts >= 10) {
    statusText = 'Penalty 1 (Peringatan)';
    statusColor = '#fbbf24';
    statusBg = 'rgba(251, 191, 36, 0.18)';
  }

  const progressPct = Math.min(100, Math.max(0, (currentPts / 50) * 100));

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
            fontSize: 12,
            fontWeight: 600,
            color: '#fb7185',
            background: 'none',
            border: 'none',
            cursor: 'pointer',
            padding: '4px 0',
          }}
        >
          ‹ Home
        </button>
        <span style={{ fontSize: 13.5, fontWeight: 700, color: '#ffffff', letterSpacing: '-0.01em' }}>
          Penalty Points
        </span>
        <button
          onClick={() => window.nismara?.syncAccount?.()}
          title="Sinkronkan Poin & Tiket"
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            width: 28,
            height: 28,
            borderRadius: 8,
            backgroundColor: 'rgba(251, 113, 133, 0.15)',
            border: '1px solid rgba(251, 113, 133, 0.32)',
            color: '#fb7185',
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
        {/* 1. Status Utama Poin Penalti Card (matching tablet_gui.cpp:3043-3062) */}
        <div
          style={{
            backgroundColor: 'var(--surface-card)',
            borderRadius: 16,
            border: '1px solid var(--border-subtle)',
            padding: 14,
            display: 'flex',
            flexDirection: 'column',
            gap: 8,
          }}
        >
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 5 }}>
              <ShieldAlert size={13} color="#fb7185" />
              <span
                style={{
                  fontSize: 10,
                  fontWeight: 800,
                  color: '#fb7185',
                  letterSpacing: '0.05em',
                }}
              >
                STATUS POIN PENALTI DRIVER
              </span>
            </div>
            <span
              style={{
                fontSize: 10,
                fontWeight: 800,
                padding: '2px 8px',
                borderRadius: 6,
                backgroundColor: statusBg,
                color: statusColor,
              }}
            >
              {statusText}
            </span>
          </div>

          <div style={{ display: 'flex', alignItems: 'baseline', justifyContent: 'space-between' }}>
            <div className="tabular-nums" style={{ display: 'flex', alignItems: 'baseline', gap: 5 }}>
              <span style={{ fontSize: 28, fontWeight: 800, color: statusColor, lineHeight: 1 }}>
                {currentPts}
              </span>
              <span style={{ fontSize: 12, color: 'var(--text-muted)', fontWeight: 600 }}>
                / 50 Maks
              </span>
            </div>
            <span className="tabular-nums" style={{ fontSize: 11, fontWeight: 700, color: 'var(--text-secondary)' }}>
              {Math.round(progressPct)}% Batas
            </span>
          </div>

          {/* Progress Bar */}
          <div>
            <div
              style={{
                width: '100%',
                height: 6,
                borderRadius: 3,
                backgroundColor: 'rgba(30, 41, 59, 0.9)',
                overflow: 'hidden',
              }}
            >
              <div
                style={{
                  width: `${Math.max(currentPts > 0 ? 4 : 0, progressPct)}%`,
                  height: '100%',
                  borderRadius: 3,
                  backgroundColor: statusColor,
                  transition: 'width 200ms ease',
                }}
              />
            </div>
            <div style={{ fontSize: 10.5, color: 'var(--text-muted)', marginTop: 5 }}>
              Tingkat Akumulasi Pelanggaran (Ideal: 0 Poin)
            </div>
          </div>
        </div>

        {/* 2. Action Toast Notification Card (matching tablet_gui.cpp:3065-3073) */}
        {telemetry.penalty_action_message && (
          <div
            style={{
              backgroundColor: telemetry.penalty_action_is_error
                ? 'rgba(239, 68, 68, 0.16)'
                : 'rgba(16, 185, 129, 0.16)',
              border: `1px solid ${
                telemetry.penalty_action_is_error
                  ? 'rgba(248, 113, 113, 0.4)'
                  : 'rgba(52, 211, 153, 0.4)'
              }`,
              borderRadius: 12,
              padding: '9px 12px',
              display: 'flex',
              alignItems: 'center',
              gap: 8,
              fontSize: 11.5,
              fontWeight: 600,
              color: telemetry.penalty_action_is_error ? '#fca5a5' : '#6ee7b7',
            }}
          >
            {telemetry.penalty_action_is_error ? (
              <AlertCircle size={15} color="#f87171" style={{ flexShrink: 0 }} />
            ) : (
              <CheckCircle2 size={15} color="#34d399" style={{ flexShrink: 0 }} />
            )}
            <span>{telemetry.penalty_action_message}</span>
          </div>
        )}

        {/* 3. Segmented Control: [Tebus NC] [Tiket Garasi] [Aturan] (matching tablet_gui.cpp:3075-3078) */}
        <div
          style={{
            display: 'grid',
            gridTemplateColumns: '1fr 1fr 1fr',
            gap: 4,
            backgroundColor: 'rgba(15, 23, 42, 0.9)',
            padding: 3,
            borderRadius: 10,
            border: '1px solid var(--border-subtle)',
          }}
        >
          {[
            { id: 'nc', label: 'Tebus NC' },
            { id: 'ticket', label: 'Tiket Garasi' },
            { id: 'rules', label: 'Aturan' },
          ].map((item) => {
            const active = tab === item.id;
            return (
              <button
                key={item.id}
                onClick={() => setTab(item.id as 'nc' | 'ticket' | 'rules')}
                style={{
                  padding: '6px 4px',
                  borderRadius: 7,
                  fontSize: 11,
                  fontWeight: 700,
                  border: 'none',
                  cursor: 'pointer',
                  backgroundColor: active ? '#e11d48' : 'transparent',
                  color: active ? '#ffffff' : '#94a3b8',
                  transition: 'all 120ms ease',
                }}
              >
                {item.label}
              </button>
            );
          })}
        </div>

        {/* TAB 0: TEBUS NC (matching tablet_gui.cpp:3080-3155) */}
        {tab === 'nc' && (
          <div
            style={{
              backgroundColor: 'var(--surface-card)',
              borderRadius: 16,
              border: '1px solid var(--border-subtle)',
              padding: 14,
              display: 'flex',
              flexDirection: 'column',
              gap: 10,
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
              <Coins size={14} color="#fbbf24" />
              <span style={{ fontSize: 10.5, fontWeight: 800, color: '#fbbf24', letterSpacing: '0.05em' }}>
                1. TEBUS MENGGUNAKAN NC
              </span>
            </div>

            <div style={{ display: 'flex', flexDirection: 'column', gap: 5, fontSize: 11.5 }}>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: 'var(--text-secondary)' }}>Saldo Dompet:</span>
                <span className="tabular-nums" style={{ fontWeight: 700, color: '#ffffff' }}>
                  {telemetry.total_nc.toLocaleString('id-ID', {
                    minimumFractionDigits: 2,
                    maximumFractionDigits: 2,
                  })}{' '}
                  NC
                </span>
              </div>

              {/* Dynamic Discount Display */}
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: 'var(--text-secondary)' }}>Status Diskon:</span>
                {discountBooster > 0 && discountNPlus > 0 ? (
                  <span className="tabular-nums" style={{ fontWeight: 700, color: '#34d399', fontSize: 11 }}>
                    -{totalDiscount.toLocaleString('id-ID')} NC (Booster + Nismara+)
                  </span>
                ) : discountBooster > 0 ? (
                  <span className="tabular-nums" style={{ fontWeight: 700, color: '#34d399', fontSize: 11 }}>
                    -{discountBooster.toLocaleString('id-ID')} NC (Server Booster)
                  </span>
                ) : discountNPlus > 0 ? (
                  <span className="tabular-nums" style={{ fontWeight: 700, color: '#34d399', fontSize: 11 }}>
                    -{discountNPlus.toLocaleString('id-ID')} NC (Nismara+)
                  </span>
                ) : (
                  <span className="tabular-nums" style={{ color: 'var(--text-muted)', fontSize: 11 }}>
                    Harga Normal ({basePrice.toLocaleString('id-ID')} NC)
                  </span>
                )}
              </div>

              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: 'var(--text-secondary)' }}>Biaya Satuan:</span>
                <span className="tabular-nums" style={{ fontWeight: 700, color: '#fde047' }}>
                  {finalPointPrice.toLocaleString('id-ID')} NC / poin
                </span>
              </div>
            </div>

            {/* Stepper Controls with Max button */}
            <div
              style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                backgroundColor: 'var(--surface-card-elevated)',
                padding: '7px 10px',
                borderRadius: 10,
                border: '1px solid var(--border-subtle)',
              }}
            >
              <span style={{ fontSize: 11.5, fontWeight: 600, color: 'var(--text-secondary)' }}>
                Jumlah Tebus:
              </span>
              <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                <button
                  onClick={() => setPointsToPay(Math.max(1, clampedPtsToPay - 1))}
                  style={{
                    width: 28,
                    height: 26,
                    borderRadius: 6,
                    backgroundColor: '#1e293b',
                    border: '1px solid rgba(255,255,255,0.14)',
                    color: '#ffffff',
                    fontWeight: 800,
                    fontSize: 13,
                    cursor: 'pointer',
                  }}
                >
                  -
                </button>
                <span
                  className="tabular-nums"
                  style={{
                    minWidth: 48,
                    textAlign: 'center',
                    fontSize: 12,
                    fontWeight: 800,
                    color: '#fbbf24',
                  }}
                >
                  {clampedPtsToPay} Poin
                </span>
                <button
                  onClick={() => setPointsToPay(Math.min(maxPayablePts, clampedPtsToPay + 1))}
                  style={{
                    width: 28,
                    height: 26,
                    borderRadius: 6,
                    backgroundColor: '#1e293b',
                    border: '1px solid rgba(255,255,255,0.14)',
                    color: '#ffffff',
                    fontWeight: 800,
                    fontSize: 13,
                    cursor: 'pointer',
                  }}
                >
                  +
                </button>
                <button
                  onClick={() => setPointsToPay(maxPayablePts)}
                  style={{
                    height: 26,
                    padding: '0 8px',
                    borderRadius: 6,
                    backgroundColor: 'rgba(251, 191, 36, 0.16)',
                    border: '1px solid rgba(251, 191, 36, 0.35)',
                    color: '#fde047',
                    fontWeight: 700,
                    fontSize: 10.5,
                    cursor: 'pointer',
                  }}
                >
                  Max
                </button>
              </div>
            </div>

            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <span style={{ fontSize: 11.5, color: 'var(--text-secondary)', fontWeight: 600 }}>
                Total Biaya:
              </span>
              <span className="tabular-nums" style={{ fontSize: 14, fontWeight: 800, color: '#fde047' }}>
                {totalNcCost.toLocaleString('id-ID')} NC
              </span>
            </div>

            <button
              onClick={() => window.nismara?.payPenalty?.(clampedPtsToPay)}
              disabled={!canPayNc}
              style={{
                width: '100%',
                padding: '9px 12px',
                borderRadius: 10,
                backgroundColor: canPayNc ? '#f59e0b' : 'rgba(51, 65, 85, 0.5)',
                color: canPayNc ? '#020617' : '#64748b',
                fontWeight: 800,
                fontSize: 12,
                border: 'none',
                cursor: canPayNc ? 'pointer' : 'not-allowed',
              }}
            >
              Bayar {clampedPtsToPay} Poin ({totalNcCost.toLocaleString('id-ID')} NC)
            </button>

            {!canPayNc && (
              <div
                style={{
                  fontSize: 10.5,
                  textAlign: 'center',
                  color: currentPts === 0 ? '#94a3b8' : '#f87171',
                }}
              >
                {currentPts === 0
                  ? 'Poin penalti Anda sudah 0 (Aman).'
                  : 'Saldo NC Anda tidak mencukupi.'}
              </div>
            )}
          </div>
        )}

        {/* TAB 1: TIKET GARASI (matching tablet_gui.cpp:3156-3210) */}
        {tab === 'ticket' && (
          <div
            style={{
              backgroundColor: 'var(--surface-card)',
              borderRadius: 16,
              border: '1px solid var(--border-subtle)',
              padding: 14,
              display: 'flex',
              flexDirection: 'column',
              gap: 10,
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
              <Ticket size={14} color="#38bdf8" />
              <span style={{ fontSize: 10.5, fontWeight: 800, color: '#38bdf8', letterSpacing: '0.05em' }}>
                2. GUNAKAN TIKET GARASI (SAFEBOX)
              </span>
            </div>

            <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 11.5 }}>
                <span style={{ color: 'var(--text-secondary)' }}>Tiket Safebox:</span>
                <span className="tabular-nums" style={{ fontWeight: 800, color: '#38bdf8' }}>
                  {safeboxAvail.toLocaleString('id-ID')} / {safeboxCap.toLocaleString('id-ID')} Tiket
                </span>
              </div>
              <div style={{ fontSize: 10.5, color: 'var(--text-muted)' }}>
                1 Tiket Safebox = Menghapus 1 Poin Penalti tanpa biaya NC.
              </div>
            </div>

            {/* Stepper Controls with Max button */}
            <div
              style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                backgroundColor: 'var(--surface-card-elevated)',
                padding: '7px 10px',
                borderRadius: 10,
                border: '1px solid var(--border-subtle)',
              }}
            >
              <span style={{ fontSize: 11.5, fontWeight: 600, color: 'var(--text-secondary)' }}>
                Jumlah Tiket:
              </span>
              <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                <button
                  onClick={() => setTicketsToUse(Math.max(1, clampedTicketsToUse - 1))}
                  style={{
                    width: 28,
                    height: 26,
                    borderRadius: 6,
                    backgroundColor: '#1e293b',
                    border: '1px solid rgba(255,255,255,0.14)',
                    color: '#ffffff',
                    fontWeight: 800,
                    fontSize: 13,
                    cursor: 'pointer',
                  }}
                >
                  -
                </button>
                <span
                  className="tabular-nums"
                  style={{
                    minWidth: 50,
                    textAlign: 'center',
                    fontSize: 12,
                    fontWeight: 800,
                    color: '#38bdf8',
                  }}
                >
                  {clampedTicketsToUse} Tiket
                </span>
                <button
                  onClick={() => setTicketsToUse(Math.min(maxUsableTickets, clampedTicketsToUse + 1))}
                  style={{
                    width: 28,
                    height: 26,
                    borderRadius: 6,
                    backgroundColor: '#1e293b',
                    border: '1px solid rgba(255,255,255,0.14)',
                    color: '#ffffff',
                    fontWeight: 800,
                    fontSize: 13,
                    cursor: 'pointer',
                  }}
                >
                  +
                </button>
                <button
                  onClick={() => setTicketsToUse(maxUsableTickets)}
                  style={{
                    height: 26,
                    padding: '0 8px',
                    borderRadius: 6,
                    backgroundColor: 'rgba(56, 189, 248, 0.16)',
                    border: '1px solid rgba(56, 189, 248, 0.35)',
                    color: '#38bdf8',
                    fontWeight: 700,
                    fontSize: 10.5,
                    cursor: 'pointer',
                  }}
                >
                  Max
                </button>
              </div>
            </div>

            <button
              onClick={() => window.nismara?.useTicket?.(clampedTicketsToUse)}
              disabled={!canUseTicket}
              style={{
                width: '100%',
                padding: '9px 12px',
                borderRadius: 10,
                backgroundColor: canUseTicket ? '#0284c7' : 'rgba(51, 65, 85, 0.5)',
                color: canUseTicket ? '#ffffff' : '#64748b',
                fontWeight: 800,
                fontSize: 12,
                border: 'none',
                cursor: canUseTicket ? 'pointer' : 'not-allowed',
              }}
            >
              Gunakan {clampedTicketsToUse} Tiket Hapus Penalti
            </button>

            {!canUseTicket && (
              <div
                style={{
                  fontSize: 10.5,
                  textAlign: 'center',
                  color: currentPts === 0 ? '#94a3b8' : '#f87171',
                }}
              >
                {currentPts === 0
                  ? 'Poin penalti Anda sudah 0 (Aman).'
                  : 'Tiket di Safebox tidak mencukupi.'}
              </div>
            )}
          </div>
        )}

        {/* TAB 2: ATURAN & SANKSI PENALTI (matching tablet_gui.cpp:3212-3236) */}
        {tab === 'rules' && (
          <div
            style={{
              backgroundColor: 'var(--surface-card)',
              borderRadius: 16,
              border: '1px solid var(--border-subtle)',
              padding: 14,
              display: 'flex',
              flexDirection: 'column',
              gap: 10,
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
              <BookOpen size={14} color="#c084fc" />
              <span style={{ fontSize: 10.5, fontWeight: 800, color: '#c084fc', letterSpacing: '0.05em' }}>
                ATURAN & TINGKAT SANKSI VTC
              </span>
            </div>

            {[
              {
                range: '[0 - 9 Poin] Status Aman',
                color: '#34d399',
                desc: 'Driver aktif normal, bebas klaim seluruh reward event & gaji penuh.',
              },
              {
                range: '[10 - 24 Poin] Penalty 1 (Peringatan)',
                color: '#fbbf24',
                desc: 'Peringatan resmi. Pembatasan klaim reward bonus berkala.',
              },
              {
                range: '[25 - 49 Poin] Penalty 2 (Peringatan Keras)',
                color: '#fb923c',
                desc: 'Suspensi sementara dari keikutsertaan convoy resmi VTC.',
              },
              {
                range: '[>= 50 Poin] Penalty 3 (Kritis)',
                color: '#f87171',
                desc: 'Sanksi nonaktif / pemecatan dari keanggotaan Nismara Transport.',
              },
            ].map((rule) => (
              <div
                key={rule.range}
                style={{
                  backgroundColor: 'var(--surface-card-elevated)',
                  padding: '8px 10px',
                  borderRadius: 10,
                  borderLeft: `3px solid ${rule.color}`,
                }}
              >
                <div style={{ fontSize: 11.5, fontWeight: 700, color: rule.color }}>{rule.range}</div>
                <div style={{ fontSize: 10.5, color: 'var(--text-secondary)', marginTop: 2, lineHeight: 1.35 }}>
                  {rule.desc}
                </div>
              </div>
            ))}
          </div>
        )}
        </div>
      </div>
    </div>
  );
};
