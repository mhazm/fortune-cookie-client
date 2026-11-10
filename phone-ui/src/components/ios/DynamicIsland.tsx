import React, { useState, useEffect, useRef } from 'react';
import { TelemetryData } from '../../types/telemetry';

interface Props {
  telemetry: TelemetryData;
  screenState?: string;
  activeApp?: string;
}

export const DynamicIsland: React.FC<Props> = ({ telemetry, screenState, activeApp }) => {
  const [isExpanded, setIsExpanded] = useState(false);
  const timeoutRef = useRef<any>(null);

  // Auto-collapse when switching screens or opening/closing apps
  useEffect(() => {
    setIsExpanded(false);
  }, [screenState, activeApp]);

  // Auto-collapse after 6 seconds of inactivity
  useEffect(() => {
    if (timeoutRef.current) {
      clearTimeout(timeoutRef.current);
      timeoutRef.current = null;
    }

    if (isExpanded) {
      timeoutRef.current = setTimeout(() => {
        setIsExpanded(false);
      }, 6000);
    }

    return () => {
      if (timeoutRef.current) {
        clearTimeout(timeoutRef.current);
      }
    };
  }, [isExpanded]);

  const handleToggle = (e: React.MouseEvent) => {
    e.stopPropagation();
    setIsExpanded((prev) => !prev);
  };

  const hasMatchedFleet = Boolean(telemetry.has_matched_fleet && telemetry.fleet_number);
  const gearText = telemetry.gear_text
    ? telemetry.gear_text
    : telemetry.gear > 0
    ? `D${telemetry.gear}`
    : telemetry.gear < 0
    ? 'R'
    : 'N';
  const speedKmh = Math.round(telemetry.speed || 0);

  // Fuel percentage matching ImGui calculation
  const fuelPct =
    telemetry.fuel_capacity && telemetry.fuel_capacity > 0
      ? Math.round((telemetry.fuel / telemetry.fuel_capacity) * 100)
      : 0;

  // Exact ImGui island dimensions (tablet_gui.cpp:1199-1200)
  const islandW = isExpanded ? 232 : (hasMatchedFleet ? 150 : 116);
  const islandH = isExpanded ? 52 : 30;

  return (
    <>
      {/* Click-outside transparent backdrop to close expanded notch */}
      {isExpanded && (
        <div
          onClick={(e) => {
            e.stopPropagation();
            setIsExpanded(false);
          }}
          style={{
            position: 'absolute',
            inset: 0,
            zIndex: 49,
            backgroundColor: 'transparent',
            cursor: 'default',
          }}
        />
      )}

      {/* Centered Dynamic Island Container */}
      <div
        style={{
          position: 'absolute',
          top: 7,
          left: 0,
          right: 0,
          display: 'flex',
          justifyContent: 'center',
          zIndex: 50,
          pointerEvents: 'none',
        }}
      >
        <div
          onClick={handleToggle}
          style={{
            width: islandW,
            height: islandH,
            minWidth: islandW,
            maxWidth: islandW,
            backgroundColor: '#000000',
            borderRadius: isExpanded ? 20 : 15,
            border: '1px solid rgba(65, 75, 95, 0.45)',
            boxShadow: isExpanded
              ? '0 10px 25px rgba(0,0,0,0.8), 0 0 0 1px rgba(255,255,255,0.12)'
              : '0 4px 12px rgba(0,0,0,0.5)',
            boxSizing: 'border-box',
            position: 'relative',
            cursor: 'pointer',
            pointerEvents: 'auto',
            transition: 'width 0.24s cubic-bezier(0.4, 0, 0.2, 1), height 0.24s cubic-bezier(0.4, 0, 0.2, 1), border-radius 0.24s cubic-bezier(0.4, 0, 0.2, 1)',
            overflow: 'hidden',
            userSelect: 'none',
          }}
        >
          {/* Front Camera Lens (Always positioned top-right inside island) */}
          <div
            style={{
              position: 'absolute',
              right: 12,
              top: isExpanded ? 11 : 8,
              width: 14,
              height: 14,
              borderRadius: 7,
              backgroundColor: '#0a0a0c',
              border: '1px solid #141c2c',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              pointerEvents: 'none',
            }}
          >
            <div
              style={{
                width: 6,
                height: 6,
                borderRadius: 3,
                backgroundColor: '#0c1a34',
                position: 'relative',
              }}
            >
              <div
                style={{
                  position: 'absolute',
                  top: 0.5,
                  left: 0.5,
                  width: 2.2,
                  height: 2.2,
                  borderRadius: 1.1,
                  backgroundColor: 'rgba(56, 138, 220, 0.75)',
                }}
              />
            </div>
          </div>

          {!isExpanded ? (
            /* ============================================================
               1. COMPACT / COLLAPSED MODE (Exact ImGui tablet_gui.cpp:1220-1238)
               - If matched fleet: "NL-074 | 0 km/h" (150px wide)
               - If no matched fleet: "0 km/h" (116px wide)
               - Live status dot (green if active, amber if paused)
               ============================================================ */
            <div
              style={{
                width: '100%',
                height: '100%',
                display: 'flex',
                alignItems: 'center',
                paddingLeft: 12,
                paddingRight: 28, // Leave space for camera lens
                boxSizing: 'border-box',
                gap: 7,
              }}
            >
              {/* Telemetry SDK Live Status Dot */}
              <div
                style={{
                  width: 7,
                  height: 7,
                  borderRadius: 3.5,
                  backgroundColor: telemetry.sdk_active !== false ? '#34d399' : '#fbbf24',
                  boxShadow: telemetry.sdk_active !== false
                    ? '0 0 6px rgba(52, 211, 153, 0.6)'
                    : '0 0 6px rgba(251, 191, 36, 0.6)',
                  flexShrink: 0,
                }}
              />

              {/* Compact Pill Text */}
              <span
                style={{
                  fontSize: 12,
                  fontWeight: 700,
                  color: '#e6f0ff',
                  letterSpacing: '-0.01em',
                  whiteSpace: 'nowrap',
                  overflow: 'hidden',
                  textOverflow: 'ellipsis',
                  lineHeight: '30px',
                }}
              >
                {hasMatchedFleet
                  ? `${telemetry.fleet_number} | ${speedKmh} km/h`
                  : `${speedKmh} km/h`}
              </span>
            </div>
          ) : (
            /* ============================================================
               2. EXPANDED MODE (Exact ImGui tablet_gui.cpp:1239-1268)
               - Line 1: [NL-074] 0 km/h [N] OR LIVE: 0 km/h [N]
               - Line 2: If job active: "-> DestinationCity (X km)"
                         If no job: "Fuel: X%  |  X RPM"
               ============================================================ */
            <div
              style={{
                width: '100%',
                height: '100%',
                display: 'flex',
                flexDirection: 'column',
                justifyContent: 'center',
                paddingLeft: 14,
                paddingRight: 28, // Space for camera lens
                boxSizing: 'border-box',
                gap: 3,
              }}
            >
              {/* Line 1: Fleet / Speed / Gear */}
              <div
                style={{
                  fontSize: 13,
                  fontWeight: 700,
                  color: '#38bdf8',
                  lineHeight: 1.15,
                  whiteSpace: 'nowrap',
                  overflow: 'hidden',
                  textOverflow: 'ellipsis',
                }}
              >
                {hasMatchedFleet
                  ? `[${telemetry.fleet_number}] ${speedKmh} km/h [${gearText}]`
                  : `LIVE: ${speedKmh} km/h [${gearText}]`}
              </div>

              {/* Line 2: Route Destination OR Fuel & RPM */}
              <div
                style={{
                  fontSize: 12,
                  fontWeight: 500,
                  color: '#d7e1f0',
                  lineHeight: 1.15,
                  whiteSpace: 'nowrap',
                  overflow: 'hidden',
                  textOverflow: 'ellipsis',
                }}
              >
                {telemetry.job_active ? (
                  `-> ${telemetry.city_dst || 'Destination'} (${Math.round(telemetry.nav_distance_km || 0)} km)`
                ) : (
                  `Fuel: ${fuelPct}%  |  ${Math.round(telemetry.rpm || 0)} RPM`
                )}
              </div>
            </div>
          )}
        </div>
      </div>
    </>
  );
};
