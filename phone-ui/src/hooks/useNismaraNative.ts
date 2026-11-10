import { useState, useEffect } from 'react';
import { TelemetryData } from '../types/telemetry';

declare global {
  interface Window {
    nismara?: {
      version?: string;
      lockPhone?: () => void;
      unlockPhone?: () => void;
      closePhone?: () => void;
      openApp?: (appId: number) => void;
      syncAccount?: () => void;
      selectFleet?: (fleetNumber: string) => void;
      payPenalty?: (points: number) => void;
      useTicket?: (tickets: number) => void;
      rescanProfile?: () => void;
      setAutoLock?: (enabled: boolean) => void;
      setFrameColor?: (colorIdx: number) => void;
      setWallpaper?: (presetIdx: number) => void;
      on?: (event: string, callback: (data: string) => void) => void;
    };
  }
}

const defaultTelemetry: TelemetryData = {
  speed: 0.0,
  gear: 0,
  gear_text: 'N',
  rpm: 0,
  max_rpm: 2500,
  fuel: 720.0,
  fuel_capacity: 1200.0,
  odometer: 142850.2,
  cruise_control: 0.0,
  park_brake: true,
  motor_brake: false,
  electric_enabled: true,
  engine_enabled: false,

  job_active: false,
  cargo: 'Special Medical Equipment',
  cargo_mass_kg: 18500,
  city_src: 'Berlin',
  company_src: 'LKW Logistik',
  city_dst: 'Warszawa',
  company_dst: 'Transinet',
  distance_km: 0.0,
  planned_distance_km: 560,
  nav_distance_km: 0.0,

  wear_engine: 0.0,
  wear_transmission: 0.0,
  wear_cabin: 0.0,
  wear_chassis: 0.0,
  wear_wheels: 0.0,
  cargo_damage: 0.0,

  driver_name: 'Driver',
  rank: 'Driver',
  total_nc: 0,
  total_exp: 0,
  level: 1,
  penalty_points: 0,
  safebox_tickets: 0,
  server_name: 'Singleplayer',
  server_mode: 'Singleplayer',
  has_matched_fleet: false,
  fleets: [],
};

export function useNismaraNative() {
  const [telemetry, setTelemetry] = useState<TelemetryData>(defaultTelemetry);
  const isUltralight = typeof window !== 'undefined' && !!window.nismara;

  useEffect(() => {
    let bound = false;

    const handleTelemetry = (payload: any) => {
      try {
        const parsed = typeof payload === 'string' ? JSON.parse(payload) : payload;
        if (parsed && typeof parsed === 'object') {
          setTelemetry((prev) => ({ ...prev, ...parsed }));
        }
      } catch (e) {
        console.error('[WebUI] Failed to parse telemetry event', e);
      }
    };

    const handleScroll = (payload: any) => {
      try {
        const parsed = typeof payload === 'string' ? JSON.parse(payload) : payload;
        const x = Number(parsed?.x ?? 175);
        const y = Number(parsed?.y ?? 379);
        const deltaY = Number(parsed?.delta_y ?? 0);
        if (!deltaY) return;

        let el = document.elementFromPoint(x, y) as HTMLElement | null;
        while (el && el !== document.body && el !== document.documentElement) {
          if (el.scrollHeight > el.clientHeight + 1) {
            const style = window.getComputedStyle(el);
            if (style.overflowY === 'auto' || style.overflowY === 'scroll') {
              el.scrollTop -= deltaY;
              return;
            }
          }
          el = el.parentElement;
        }

        const fallback = document.querySelector('[data-scroll-container="true"]') as HTMLElement | null;
        if (fallback && fallback.scrollHeight > fallback.clientHeight + 1) {
          fallback.scrollTop -= deltaY;
        }
      } catch (e) {
        console.error('[WebUI] Failed to process scroll event', e);
      }
    };

    (window as any).__onNismaraTelemetry = handleTelemetry;

    if (window.nismara?.on) {
      window.nismara.on('telemetry', handleTelemetry);
      window.nismara.on('scroll', handleScroll);
      bound = true;
    }

    const checkInterval = setInterval(() => {
      if (!bound && window.nismara?.on) {
        window.nismara.on('telemetry', handleTelemetry);
        window.nismara.on('scroll', handleScroll);
        bound = true;
        clearInterval(checkInterval);
      }
    }, 100);

    // Mock live simulation only if running in external browser without Ultralight
    let mockInterval: any = null;
    if (!window.nismara) {
      mockInterval = setInterval(() => {
        if (!window.nismara) {
          setTelemetry((prev) => ({
            ...prev,
            speed: Math.max(0, prev.speed + (Math.random() * 2 - 1)),
            rpm: Math.max(800, Math.min(2000, prev.rpm + (Math.random() * 40 - 20))),
          }));
        }
      }, 500);
    }

    return () => {
      clearInterval(checkInterval);
      if (mockInterval) clearInterval(mockInterval);
    };
  }, []);

  const lockPhone = () => {
    if (window.nismara?.lockPhone) window.nismara.lockPhone();
  };

  const unlockPhone = () => {
    if (window.nismara?.unlockPhone) window.nismara.unlockPhone();
  };

  const closePhone = () => {
    if (window.nismara?.closePhone) window.nismara.closePhone();
  };

  const syncAccount = () => {
    if (window.nismara?.syncAccount) window.nismara.syncAccount();
  };

  const selectFleet = (fleetNumber: string) => {
    if (window.nismara?.selectFleet) window.nismara.selectFleet(fleetNumber);
  };

  const payPenalty = (points: number) => {
    if (window.nismara?.payPenalty) window.nismara.payPenalty(points);
  };

  const useTicket = (tickets: number) => {
    if (window.nismara?.useTicket) window.nismara.useTicket(tickets);
  };

  const setWallpaper = (presetIdx: number) => {
    if (window.nismara?.setWallpaper) window.nismara.setWallpaper(presetIdx);
  };

  return {
    telemetry,
    isUltralight,
    lockPhone,
    unlockPhone,
    closePhone,
    syncAccount,
    selectFleet,
    payPenalty,
    useTicket,
    setWallpaper,
  };
}
