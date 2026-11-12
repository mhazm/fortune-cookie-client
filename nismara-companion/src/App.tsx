import { useState, useEffect, useMemo } from "react";
import { invoke } from "@tauri-apps/api/core";
import { check, Update } from "@tauri-apps/plugin-updater";
import { relaunch } from "@tauri-apps/plugin-process";
import {
  Gamepad2,
  Truck,
  Newspaper,
  Trophy,
  Settings as SettingsIcon,
  CheckCircle2,
  AlertCircle,
  ExternalLink,
  Radio,
  X,
  ShieldCheck,
  FolderSync,
  FolderOpen,
  Save,
  RefreshCw,
  ShieldAlert,
  ArrowUpCircle,
  Calendar,
  User,
  Copy,
  Check,
  EyeOff,
  Compass,
  Coins,
  Clock,
  RotateCcw,
  Play,
  CheckCircle,
} from "lucide-react";

interface GameInfo {
  id: string;
  name: string;
  steam_app_id: number;
  is_installed: boolean;
  game_path: string;
  exe_path: string;
  plugin_dir: string;
  plugin_installed: boolean;
  plugin_path: string;
  user_data_dir: string;
  config_path: string;
  is_running: boolean;
  plugin_outdated: boolean;
}

interface SystemStatus {
  companion_version: string;
  os_platform: string;
  steam_installed: boolean;
  steam_path: string;
  games: GameInfo[];
  gatekeeper_active: boolean;
}

interface PluginConfigPayload {
  driver_token: string;
  api_base_url: string;
  ets2_config_path: string;
  ats_config_path: string;
}

interface DriverProfileData {
  id: string;
  name: string;
  image?: string;
  discordId?: string;
  level: number;
  xp: number;
  totalNC: number;
  totalNc: number;
  points: number;
  truckyRank: string;
  truckyRankColor: string;
  ongoingJob?: {
    jobId: string;
    game: string;
    sourceCity: string;
    destinationCity: string;
    cargoName: string;
    checkpoint?: {
      drivenDistanceKm: number;
      fuelUsedLiters: number;
      maxSpeedKmh: number;
      accumVehicleDamage: number;
      accumTrailerDamage: number;
      accumCargoDamage: number;
      savedAt: string | null;
    } | null;
  } | null;
  lastCompletedJob?: {
    jobId: string;
    game: string;
    sourceCity: string;
    destinationCity: string;
    cargoName: string;
    distanceKm: number;
    revenue: number;
    damagePercentage: number;
    completedAt: string | null;
  } | null;
}

interface SanityArticle {
  _id: string;
  title: string;
  slug?: { current: string };
  publishedAt: string;
  mainImage?: {
    asset?: {
      _ref?: string;
    };
  };
  author?: {
    name?: string;
    image?: any;
  } | null;
  categories?: Array<{ title: string }>;
  seoDescription?: string;
  body?: any[];
}

function getSanityImageUrl(ref?: string): string | null {
  if (!ref) return null;
  const parts = ref.split("-");
  if (parts.length < 4) return null;
  const id = parts[1];
  const dimensions = parts[2];
  const ext = parts[3];
  return `https://cdn.sanity.io/images/yi1d1k89/production/${id}-${dimensions}.${ext}`;
}

function extractBlockText(body?: any[]): string {
  if (!body || !Array.isArray(body)) return "";
  return body
    .filter((b) => b && b._type === "block" && Array.isArray(b.children))
    .map((b) => b.children.map((c: any) => c.text || "").join(""))
    .filter(Boolean)
    .join("\n\n");
}

function formatNewsDate(dateStr: string): string {
  try {
    const d = new Date(dateStr);
    if (isNaN(d.getTime())) return dateStr;
    return d.toLocaleDateString("id-ID", {
      day: "numeric",
      month: "short",
      year: "numeric",
    });
  } catch {
    return dateStr;
  }
}

function getCategoryColor(category?: string): string {
  const cat = (category || "").toUpperCase();
  if (cat.includes("UPDATE") || cat.includes("PATCH")) {
    return "bg-blue-500/20 text-blue-300 border-blue-500/30";
  }
  if (cat.includes("CONVOY") || cat.includes("EVENT")) {
    return "bg-emerald-500/20 text-emerald-300 border-emerald-500/30";
  }
  if (cat.includes("TRANSPORT") || cat.includes("LOGISTICS")) {
    return "bg-purple-500/20 text-purple-300 border-purple-500/30";
  }
  if (cat.includes("POLICY") || cat.includes("RULES") || cat.includes("ATURAN")) {
    return "bg-amber-500/20 text-amber-300 border-amber-500/30";
  }
  return "bg-sky-500/20 text-sky-300 border-sky-500/30";
}

interface EventQuest {
  questId: string;
  title: string;
  description: string;
  type: string;
  target: number;
  rewardTickets: number;
}

interface EventPrize {
  tier: number;
  tierTitle: string;
  winnerCount?: number;
  rewards: Array<{
    type: string;
    title: string;
    amount?: number;
  }>;
}

interface EventFeedItem {
  id: string;
  type: "CONTRACT" | "COUPON" | "GIVEAWAY" | "NC_EVENT";
  typeLabel: string;
  title: string;
  description?: string;
  company?: string;
  couponCode?: string;
  rewardType?: string;
  minAmount?: number;
  maxAmount?: number;
  multiplierText?: string;
  multiplierValue?: number;
  startDate?: string | null;
  endDate?: string | null;
  drawDate?: string | null;
  status?: string;
  isActive: boolean;
  isScheduled?: boolean;
  imageUrl?: string | null;
  badgeColor: string;
  claimsCount?: number;
  participantsCount?: number;
  ticketPriceNC?: number;
  stats?: {
    completedContracts?: number;
    totalNCEarned?: number;
    totalDistanceKm?: number;
    totalMassTons?: number;
    contributorsCount?: number;
  };
  quests?: EventQuest[];
  prizes?: EventPrize[];
}

interface EventsApiResponse {
  success: boolean;
  summary: {
    totalEvents: number;
    activeCount: number;
    contractsCount: number;
    couponsCount: number;
    giveawaysCount: number;
    nceventsCount: number;
  };
  data: {
    feed: EventFeedItem[];
    contracts: EventFeedItem[];
    coupons: EventFeedItem[];
    giveaways: EventFeedItem[];
    ncevents: EventFeedItem[];
  };
}

const APP_VERSION = "1.0.3";
const PRODUCTION_API_URL = "http://103.150.101.154:3100";

export default function App() {
  const [activeTab, setActiveTab] = useState<
    "games" | "news" | "events" | "settings"
  >("games");
  const [systemStatus, setSystemStatus] = useState<SystemStatus | null>(null);
  const [loadingAction, setLoadingAction] = useState<string | null>(null);
  const [statusToast, setStatusToast] = useState<{
    message: string;
    isError: boolean;
  } | null>(null);

  // Driver Authentication & Profile State
  const [driverToken, setDriverToken] = useState(
    () => localStorage.getItem("nsm_driver_token") || "",
  );
  const [driverData, setDriverData] = useState<DriverProfileData | null>(null);
  const [resolvedConfigPath, setResolvedConfigPath] = useState<string>("");

  // Custom paths for all 4 clients
  const [customEts2Path, setCustomEts2Path] = useState(
    () => localStorage.getItem("nsm_ets2_path") || "",
  );
  const [customAtsPath, setCustomAtsPath] = useState(
    () => localStorage.getItem("nsm_ats_path") || "",
  );
  const [customTruckersmpPath, setCustomTruckersmpPath] = useState(
    () => localStorage.getItem("nsm_truckersmp_path") || "",
  );
  const [customHaulmpPath, setCustomHaulmpPath] = useState(
    () => localStorage.getItem("nsm_haulmp_path") || "",
  );

  // Preferences
  const [autoMinimizeOnLaunch, setAutoMinimizeOnLaunch] = useState<boolean>(() => {
    return localStorage.getItem("nsm_auto_minimize") !== "0";
  });
  const [autoSyncOnStartup, setAutoSyncOnStartup] = useState<boolean>(() => {
    return localStorage.getItem("nsm_auto_sync") !== "0";
  });

  // News from Sanity State
  const [articles, setArticles] = useState<SanityArticle[]>([]);
  const [newsLoading, setNewsLoading] = useState(false);
  const [selectedCategory, setSelectedCategory] = useState<string>("ALL");
  const [readingArticle, setReadingArticle] = useState<SanityArticle | null>(null);

  // Events from Tracker Server
  const [eventsFeed, setEventsFeed] = useState<EventFeedItem[]>([]);
  const [eventsLoading, setEventsLoading] = useState(false);
  const [selectedEventType, setSelectedEventType] = useState<string>("ALL");
  const [selectedEventDetail, setSelectedEventDetail] = useState<EventFeedItem | null>(null);
  const [copiedCouponCode, setCopiedCouponCode] = useState<string | null>(null);

  // Auto-Updater State
  const [availableUpdate, setAvailableUpdate] = useState<Update | null>(null);
  const [showUpdateModal, setShowUpdateModal] = useState(false);
  const [updateDownloading, setUpdateDownloading] = useState(false);
  const [downloadProgress, setDownloadProgress] = useState<{
    downloaded: number;
    total: number;
  } | null>(null);
  const [serverVersionAlert, setServerVersionAlert] = useState<{
    minVersion: string;
    latestVersion: string;
    downloadUrl: string;
  } | null>(null);

  const notify = (message: string, isError = false) => {
    setStatusToast({ message, isError });
    setTimeout(() => {
      setStatusToast(null);
    }, 4500);
  };

  // Safe fetch system status from Tauri backend
  const refreshSystemStatus = async (
    ets2Override?: string,
    atsOverride?: string,
    tmpOverride?: string,
    hmpOverride?: string,
  ) => {
    const ePath = ets2Override !== undefined ? ets2Override : customEts2Path;
    const aPath = atsOverride !== undefined ? atsOverride : customAtsPath;
    const tPath = tmpOverride !== undefined ? tmpOverride : customTruckersmpPath;
    const hPath = hmpOverride !== undefined ? hmpOverride : customHaulmpPath;

    try {
      const res = await invoke<SystemStatus>("get_system_status", {
        customEts2Path: ePath.trim() || null,
        customAtsPath: aPath.trim() || null,
        customTruckersmpPath: tPath.trim() || null,
        customHaulmpPath: hPath.trim() || null,
      });
      setSystemStatus(res);
    } catch (err: any) {
      console.error("Gagal memeriksa status sistem:", err);
    }
  };

  // Fetch driver profile, NC wallet, ongoing job, and last completed job
  const fetchDriverData = async (token = driverToken) => {
    if (!token.trim()) {
      setDriverData(null);
      return;
    }
    try {
      const resp = await fetch(`${PRODUCTION_API_URL}/api/tracker/me`, {
        headers: {
          Authorization: `Bearer ${token.trim()}`,
          "x-tracker-token": token.trim(),
        },
      });
      if (resp.ok) {
        const json = await resp.json();
        if (json.success && json.driver) {
          setDriverData(json.driver);
        }
      }
    } catch (err) {
      console.warn("Gagal mengambil profil driver dari server:", err);
    }
  };

  // Check Tauri Updater against GitHub Releases
  const checkForUpdates = async (manual = false) => {
    try {
      const update = await check();
      if (update?.available) {
        setAvailableUpdate(update);
        setShowUpdateModal(true);
        if (manual) {
          notify(`Pembaruan v${update.version} ditemukan!`);
        }
      } else {
        setAvailableUpdate(null);
        if (manual) {
          notify(`Nismara Companion sudah versi terbaru (v${APP_VERSION}).`);
        }
      }
    } catch (err: any) {
      console.warn("Updater check:", err);
      if (manual) {
        notify(`Pemeriksaan pembaruan selesai (versi terkini).`);
      }
    }
  };

  // Download & Install App Update
  const handleApplyUpdate = async () => {
    if (!availableUpdate) return;
    setUpdateDownloading(true);
    try {
      let downloaded = 0;
      let total = 0;
      await availableUpdate.downloadAndInstall((event) => {
        switch (event.event) {
          case "Started":
            total = event.data.contentLength || 0;
            setDownloadProgress({ downloaded: 0, total });
            break;
          case "Progress":
            downloaded += event.data.chunkLength;
            setDownloadProgress({ downloaded, total });
            break;
          case "Finished":
            setDownloadProgress({ downloaded: total, total });
            break;
        }
      });
      notify("Pembaruan berhasil dipasang! Memulai ulang aplikasi...");
      setTimeout(async () => {
        await relaunch();
      }, 1200);
    } catch (err: any) {
      notify(`Gagal mengunduh pembaruan: ${err?.message || err}`, true);
      setUpdateDownloading(false);
    }
  };

  // Check backend server version requirements
  const checkServerVersion = async () => {
    try {
      const resp = await fetch(`${PRODUCTION_API_URL}/api/tracker/version`);
      if (resp.ok) {
        const data = await resp.json();
        if (data.success && data.minVersion) {
          const parseVer = (v: string) =>
            v.split(".").map((n) => parseInt(n, 10) || 0);
          const [sMaj, sMin, sPat] = parseVer(data.minVersion);
          const [cMaj, cMin, cPat] = parseVer(APP_VERSION);
          const isOutdated =
            cMaj < sMaj ||
            (cMaj === sMaj && cMin < sMin) ||
            (cMaj === sMaj && cMin === sMin && cPat < sPat);
          if (isOutdated) {
            setServerVersionAlert({
              minVersion: data.minVersion,
              latestVersion: data.latestVersion || data.minVersion,
              downloadUrl:
                data.downloadUrl ||
                "https://github.com/mhazm/fortune-cookie-client/releases/latest",
            });
          } else {
            setServerVersionAlert(null);
          }
        }
      }
    } catch {
      // Ignore if offline
    }
  };

  // Auto-sync in-game DLLs to detected games
  const handleAutoSyncPlugins = async (silent = false) => {
    try {
      const reports = await invoke<string[]>("auto_sync_plugins", {
        customEts2Path: customEts2Path.trim() || null,
        customAtsPath: customAtsPath.trim() || null,
      });
      if (reports && reports.length > 0) {
        if (!silent) {
          notify(reports.join(" | "));
        }
        refreshSystemStatus();
      } else if (!silent) {
        notify("Plugin telemetri game sudah versi terbaru.");
      }
    } catch (err: any) {
      if (!silent) {
        notify(`Auto-sync plugin: ${err}`, true);
      }
    }
  };

  // Launch Client (auto-hides to tray)
  const handleLaunchClient = async (clientId: string) => {
    setLoadingAction(`launch_${clientId}`);
    try {
      const customPath =
        clientId === "ets2"
          ? customEts2Path
          : clientId === "ats"
          ? customAtsPath
          : clientId === "truckersmp"
          ? customTruckersmpPath
          : customHaulmpPath;

      await invoke<string>("launch_client", {
        clientId,
        customPath: customPath.trim() || null,
      });

      notify(
        `Meluncurkan ${clientId.toUpperCase()}... Companion disembunyikan ke System Tray.`,
      );
      refreshSystemStatus();
    } catch (err: any) {
      notify(`Gagal meluncurkan ${clientId.toUpperCase()}: ${err}`, true);
    } finally {
      setLoadingAction(null);
    }
  };

  // Browse executable (.exe) file natively via Windows Explorer
  const handleBrowseExe = async (
    clientId: "ets2" | "ats" | "truckersmp" | "haulmp",
  ) => {
    const titles = {
      ets2: "Pilih File Executable eurotrucks2.exe",
      ats: "Pilih File Executable amtrucks.exe",
      truckersmp: "Pilih File Executable TruckersMP-Launcher.exe",
      haulmp: "Pilih File Executable HaulMP Launcher.exe",
    };
    try {
      const selected = await invoke<string | null>("select_executable_file", {
        title: titles[clientId],
      });
      if (selected) {
        if (clientId === "ets2") {
          setCustomEts2Path(selected);
          localStorage.setItem("nsm_ets2_path", selected);
          refreshSystemStatus(selected, customAtsPath, customTruckersmpPath, customHaulmpPath);
        } else if (clientId === "ats") {
          setCustomAtsPath(selected);
          localStorage.setItem("nsm_ats_path", selected);
          refreshSystemStatus(customEts2Path, selected, customTruckersmpPath, customHaulmpPath);
        } else if (clientId === "truckersmp") {
          setCustomTruckersmpPath(selected);
          localStorage.setItem("nsm_truckersmp_path", selected);
          refreshSystemStatus(customEts2Path, customAtsPath, selected, customHaulmpPath);
        } else if (clientId === "haulmp") {
          setCustomHaulmpPath(selected);
          localStorage.setItem("nsm_haulmp_path", selected);
          refreshSystemStatus(customEts2Path, customAtsPath, customTruckersmpPath, selected);
        }
        notify(`Lokasi ${clientId.toUpperCase()} diperbarui: ${selected}`);
      }
    } catch (err: any) {
      notify(`Gagal memilih file: ${err}`, true);
    }
  };

  // Reset executable path to auto-detect
  const handleResetExe = (clientId: "ets2" | "ats" | "truckersmp" | "haulmp") => {
    if (clientId === "ets2") {
      setCustomEts2Path("");
      localStorage.removeItem("nsm_ets2_path");
      refreshSystemStatus("", customAtsPath, customTruckersmpPath, customHaulmpPath);
    } else if (clientId === "ats") {
      setCustomAtsPath("");
      localStorage.removeItem("nsm_ats_path");
      refreshSystemStatus(customEts2Path, "", customTruckersmpPath, customHaulmpPath);
    } else if (clientId === "truckersmp") {
      setCustomTruckersmpPath("");
      localStorage.removeItem("nsm_truckersmp_path");
      refreshSystemStatus(customEts2Path, customAtsPath, "", customHaulmpPath);
    } else if (clientId === "haulmp") {
      setCustomHaulmpPath("");
      localStorage.removeItem("nsm_haulmp_path");
      refreshSystemStatus(customEts2Path, customAtsPath, customTruckersmpPath, "");
    }
    notify(`Pendeteksian ${clientId.toUpperCase()} dikembalikan ke mode otomatis.`);
  };

  // Install Plugin DLL to Game
  const handleInstallPlugin = async (gameId: string) => {
    setLoadingAction(`install_${gameId}`);
    const customPath = gameId === "ets2" ? customEts2Path : customAtsPath;
    try {
      const msg = await invoke<string>("install_plugin", {
        gameId,
        customGamePath: customPath.trim() || null,
      });
      await invoke<PluginConfigPayload>("save_plugin_config", {
        driverToken: driverToken.trim(),
        apiBaseUrl: PRODUCTION_API_URL,
      }).catch(() => {});
      notify(msg || `Plugin berhasil dipasang ke folder game!`);
      refreshSystemStatus();
    } catch (err: any) {
      notify(`Gagal memasang plugin: ${err}`, true);
    } finally {
      setLoadingAction(null);
    }
  };

  // Save Plugin Token & Config
  const handleSaveDriverToken = async () => {
    const cleanToken = driverToken.trim();
    localStorage.setItem("nsm_driver_token", cleanToken);
    try {
      const saved = await invoke<PluginConfigPayload>("save_plugin_config", {
        driverToken: cleanToken,
        apiBaseUrl: PRODUCTION_API_URL,
      });
      setDriverToken(saved.driver_token);
      setResolvedConfigPath(saved.ets2_config_path);
      fetchDriverData(cleanToken);
      notify("Token driver berhasil diverifikasi dan disinkronkan ke game config!");
    } catch (err: any) {
      notify(`Gagal menyimpan token: ${err}`, true);
    }
  };

  // Minimize to System Tray
  const handleMinimizeToTray = async () => {
    try {
      await invoke("hide_to_tray");
    } catch (err) {
      console.warn("hide_to_tray error:", err);
    }
  };

  // Fetch News from Sanity
  const fetchSanityArticles = async (forceRefresh = false) => {
    setNewsLoading(true);
    try {
      const qs = forceRefresh ? "?refresh=1" : "";
      const res = await fetch(`${PRODUCTION_API_URL}/api/tracker/news${qs}`);
      if (res.ok) {
        const json = await res.json();
        const list = Array.isArray(json.articles)
          ? json.articles
          : Array.isArray(json.data)
          ? json.data
          : [];
        setArticles(list);
      }
    } catch {
      notify("Gagal memuat warta dari server.", true);
    } finally {
      setNewsLoading(false);
    }
  };

  // Fetch Events from Tracker Server
  const fetchEvents = async () => {
    setEventsLoading(true);
    try {
      const res = await fetch(`${PRODUCTION_API_URL}/api/tracker/events`);
      if (res.ok) {
        const json: EventsApiResponse = await res.json();
        if (json.success && json.data) {
          setEventsFeed(json.data.feed || []);
        }
      }
    } catch {
      notify("Gagal memuat event dari server.", true);
    } finally {
      setEventsLoading(false);
    }
  };

  // Initial Load
  useEffect(() => {
    refreshSystemStatus();
    if (driverToken) {
      fetchDriverData(driverToken);
    }
    invoke<PluginConfigPayload>("get_plugin_config")
      .then((cfg) => {
        if (cfg.driver_token && !driverToken) {
          setDriverToken(cfg.driver_token);
          localStorage.setItem("nsm_driver_token", cfg.driver_token);
          fetchDriverData(cfg.driver_token);
        }
        setResolvedConfigPath(cfg.ets2_config_path);
      })
      .catch(() => {});

    checkForUpdates();
    checkServerVersion();

    if (autoSyncOnStartup) {
      handleAutoSyncPlugins(true);
    }

    fetchSanityArticles();
    fetchEvents();

    // Poll status & driver profile every 12 seconds
    const interval = setInterval(() => {
      refreshSystemStatus();
      if (driverToken) {
        fetchDriverData(driverToken);
      }
    }, 12000);

    return () => clearInterval(interval);
  }, []);

  // Filtered Installed Games for Dynamic Cards
  const installedGames = useMemo(() => {
    return (systemStatus?.games || []).filter((g) => g.is_installed);
  }, [systemStatus?.games]);

  // Categories for News
  const availableCategories = useMemo(() => {
    const set = new Set<string>();
    articles.forEach((a) => {
      a.categories?.forEach((c) => {
        if (c.title) set.add(c.title);
      });
    });
    return ["ALL", ...Array.from(set)];
  }, [articles]);

  const filteredArticles = useMemo(() => {
    if (selectedCategory === "ALL") return articles;
    return articles.filter((a) =>
      a.categories?.some((c) => c.title === selectedCategory),
    );
  }, [articles, selectedCategory]);

  // Filtered Events
  const filteredEvents = useMemo(() => {
    if (selectedEventType === "ALL") return eventsFeed;
    return eventsFeed.filter((e) => e.type === selectedEventType);
  }, [eventsFeed, selectedEventType]);

  const handleCopyCouponCode = (code: string) => {
    navigator.clipboard.writeText(code);
    setCopiedCouponCode(code);
    notify(`Kode kupon ${code} berhasil disalin ke clipboard!`);
    setTimeout(() => setCopiedCouponCode(null), 3000);
  };

  // Determine active simulator running
  const runningGame = systemStatus?.games.find((g) => g.is_running);

  return (
    <div className="flex flex-col h-screen w-screen select-none overflow-hidden bg-[#070a10] text-slate-100 font-sans">
      {/* Toast Notification */}
      {statusToast && (
        <div
          className={`fixed top-16 right-6 z-50 flex items-center gap-2.5 px-4 py-2.5 rounded-xl shadow-2xl border text-xs backdrop-blur-md transition-all animate-in fade-in duration-200 ${
            statusToast.isError
              ? "bg-rose-950/90 border-rose-600/60 text-rose-200 shadow-rose-950/50"
              : "bg-slate-900/95 border-amber-500/60 text-amber-200 shadow-amber-950/50"
          }`}
        >
          {statusToast.isError ? (
            <AlertCircle className="w-4 h-4 text-rose-400 shrink-0" />
          ) : (
            <CheckCircle2 className="w-4 h-4 text-amber-400 shrink-0" />
          )}
          <span className="font-medium">{statusToast.message}</span>
        </div>
      )}

      {/* TOP COMMAND BAR (REPLACES OLD LEFT SIDEBAR) */}
      <header className="h-14 shrink-0 bg-[#090d15]/95 border-b border-slate-800/80 px-5 flex items-center justify-between z-30 backdrop-blur-md">
        {/* Left: Brand Identity & Live Indicators */}
        <div className="flex items-center gap-4">
          <div
            className="flex items-center gap-3 cursor-pointer group"
            onClick={() => setActiveTab("games")}
          >
            <div className="w-8 h-8 rounded-lg bg-gradient-to-br from-amber-500/20 to-slate-900 border border-amber-500/40 flex items-center justify-center p-1 shadow-md shadow-amber-500/10 group-hover:border-amber-400 transition-all">
              <img
                src="/images/logo_nismara.png"
                alt="Nismara"
                className="w-full h-full object-contain filter drop-shadow"
                onError={(e) => {
                  (e.target as HTMLElement).style.display = "none";
                }}
              />
            </div>
            <div className="flex flex-col">
              <div className="flex items-center gap-2">
                <span className="text-xs font-black tracking-widest text-white uppercase">
                  NISMARA COMPANION
                </span>
                <span className="text-[9px] font-bold px-1.5 py-0.5 rounded bg-amber-500/20 text-amber-300 border border-amber-500/30">
                  v{APP_VERSION}
                </span>
              </div>
            </div>
          </div>

          <div className="h-4 w-px bg-slate-800 hidden sm:block" />

          {/* Running Game Badge or Gatekeeper Status */}
          {runningGame ? (
            <div className="flex items-center gap-1.5 px-2.5 py-1 rounded-full bg-emerald-500/20 border border-emerald-500/40 text-[11px] text-emerald-300 font-bold backdrop-blur-md">
              <Radio className="w-3 h-3 text-emerald-400 animate-pulse" />
              <span>Sedang Berjalan: {runningGame.name}</span>
            </div>
          ) : (
            <div className="hidden md:flex items-center gap-2 px-2.5 py-1 rounded-full bg-emerald-500/10 border border-emerald-500/30 text-[11px] text-emerald-400 font-medium">
              <span className="w-2 h-2 rounded-full bg-emerald-400 animate-pulse" />
              <span>Gatekeeper Aktif</span>
            </div>
          )}
        </div>

        {/* Center: Frosted Glass Segmented Navigation */}
        <nav className="flex items-center gap-1 bg-slate-950/70 p-1 rounded-xl border border-slate-800/80 shadow-inner">
          <button
            onClick={() => setActiveTab("games")}
            className={`flex items-center gap-2 px-3.5 py-1.5 rounded-lg text-xs font-semibold transition-all cursor-pointer ${
              activeTab === "games"
                ? "bg-gradient-to-r from-amber-500 to-amber-600 text-slate-950 shadow-md shadow-amber-500/20 font-bold"
                : "text-slate-400 hover:text-slate-200 hover:bg-slate-800/40"
            }`}
          >
            <Truck className="w-3.5 h-3.5" />
            <span>Armada & Game</span>
          </button>

          <button
            onClick={() => setActiveTab("news")}
            className={`flex items-center gap-2 px-3.5 py-1.5 rounded-lg text-xs font-semibold transition-all cursor-pointer ${
              activeTab === "news"
                ? "bg-gradient-to-r from-amber-500 to-amber-600 text-slate-950 shadow-md shadow-amber-500/20 font-bold"
                : "text-slate-400 hover:text-slate-200 hover:bg-slate-800/40"
            }`}
          >
            <Newspaper className="w-3.5 h-3.5" />
            <span>Warta</span>
          </button>

          <button
            onClick={() => setActiveTab("events")}
            className={`flex items-center gap-2 px-3.5 py-1.5 rounded-lg text-xs font-semibold transition-all cursor-pointer ${
              activeTab === "events"
                ? "bg-gradient-to-r from-amber-500 to-amber-600 text-slate-950 shadow-md shadow-amber-500/20 font-bold"
                : "text-slate-400 hover:text-slate-200 hover:bg-slate-800/40"
            }`}
          >
            <Trophy className="w-3.5 h-3.5" />
            <span>Event & Misi</span>
          </button>

          <button
            onClick={() => setActiveTab("settings")}
            className={`flex items-center gap-2 px-3.5 py-1.5 rounded-lg text-xs font-semibold transition-all cursor-pointer ${
              activeTab === "settings"
                ? "bg-gradient-to-r from-amber-500 to-amber-600 text-slate-950 shadow-md shadow-amber-500/20 font-bold"
                : "text-slate-400 hover:text-slate-200 hover:bg-slate-800/40"
            }`}
          >
            <SettingsIcon className="w-3.5 h-3.5" />
            <span>Pengaturan</span>
          </button>
        </nav>

        {/* Right: Driver Identity Pill & Window Tray Controls */}
        <div className="flex items-center gap-3">
          {driverData ? (
            <div
              className="flex items-center gap-2.5 px-3 py-1 rounded-xl bg-slate-900/90 border border-slate-700/80 hover:border-amber-500/50 transition-all cursor-pointer"
              onClick={() => setActiveTab("settings")}
              title="Kelola Akun Driver"
            >
              <div className="w-6 h-6 rounded-full bg-amber-500/20 border border-amber-500/40 flex items-center justify-center text-[10px] font-bold text-amber-300 overflow-hidden">
                {driverData.image ? (
                  <img
                    src={driverData.image}
                    alt={driverData.name}
                    className="w-full h-full object-cover"
                  />
                ) : (
                  driverData.name.slice(0, 2).toUpperCase()
                )}
              </div>
              <div className="flex flex-col text-left">
                <span className="text-[11px] font-bold text-white leading-tight">
                  {driverData.name}
                </span>
                <div className="flex items-center gap-1.5 text-[9px] text-amber-400 font-mono">
                  <Coins className="w-2.5 h-2.5" />
                  <span>NC {(driverData.totalNC || 0).toLocaleString()}</span>
                </div>
              </div>
            </div>
          ) : (
            <button
              onClick={() => setActiveTab("settings")}
              className="px-3 py-1.5 rounded-lg bg-amber-500/15 hover:bg-amber-500/25 border border-amber-500/30 text-amber-300 text-xs font-semibold flex items-center gap-1.5 transition-all cursor-pointer"
            >
              <User className="w-3.5 h-3.5" />
              <span>Tautkan Akun</span>
            </button>
          )}

          <div className="h-4 w-px bg-slate-800" />

          {/* Minimize to Tray Button */}
          <button
            onClick={handleMinimizeToTray}
            className="p-1.5 rounded-lg bg-slate-900/80 hover:bg-slate-800 border border-slate-800 text-slate-400 hover:text-amber-300 transition-all cursor-pointer"
            title="Sembunyikan ke System Tray (Tetap aktif di latar belakang)"
          >
            <EyeOff className="w-4 h-4" />
          </button>
        </div>
      </header>

      {/* SERVER VERSION ALERT (GATEKEEPER) */}
      {serverVersionAlert && (
        <div className="bg-gradient-to-r from-rose-950 via-rose-900 to-rose-950 border-b border-rose-500/50 px-6 py-2.5 flex items-center justify-between text-xs text-rose-100 backdrop-blur-md z-40 shadow-lg">
          <div className="flex items-center gap-2.5">
            <ShieldAlert className="w-4 h-4 text-rose-400 shrink-0 animate-pulse" />
            <span>
              <strong>VERSI KLIEN WAJIB DIPERBARUI:</strong> Server Nismara mewajibkan
              versi minimum <strong>v{serverVersionAlert.minVersion}</strong>.
            </span>
          </div>
          <a
            href={serverVersionAlert.downloadUrl}
            target="_blank"
            rel="noreferrer"
            className="px-3.5 py-1 rounded-lg bg-rose-600 hover:bg-rose-500 text-white font-bold text-[11px] flex items-center gap-1.5 transition-all shadow-md shrink-0 ml-4 cursor-pointer"
          >
            <span>Unduh Versi v{serverVersionAlert.latestVersion}</span>
            <ExternalLink className="w-3 h-3" />
          </a>
        </div>
      )}

      {/* MAIN VIEWPORT */}
      <main className="flex-1 overflow-y-auto relative bg-[#070a10]">
        {/* VIEW: GAMES & OPERATIONS DASHBOARD */}
        {activeTab === "games" && (
          <div className="p-6 max-w-6xl mx-auto space-y-6">
            {/* INTEGRATED OPERATIONS DASHBOARD (WIDGET OPERASIONAL TERPADU) */}
            {driverData?.ongoingJob ? (
              /* CASE 1: ONGOING JOB ACTIVE */
              <div className="rounded-2xl bg-gradient-to-br from-slate-900/90 via-[#0d1422] to-slate-950 border border-amber-500/40 p-5 shadow-2xl relative overflow-hidden">
                <div className="absolute top-0 right-0 w-80 h-80 bg-amber-500/5 rounded-full blur-3xl pointer-events-none" />

                <div className="relative z-10 space-y-4">
                  {/* Top Bar Manifest */}
                  <div className="flex items-center justify-between border-b border-slate-800/80 pb-3">
                    <div className="flex items-center gap-2.5">
                      <span className="px-2.5 py-0.5 rounded-full bg-emerald-500/20 border border-emerald-500/40 text-emerald-300 text-[11px] font-bold flex items-center gap-1.5">
                        <span className="w-2 h-2 rounded-full bg-emerald-400 animate-ping" />
                        <span>PENGIRIMAN AKTIF</span>
                      </span>
                      <span className="text-xs text-slate-400 font-mono">
                        Job ID #{driverData.ongoingJob.jobId}
                      </span>
                      <span className="text-xs text-slate-500">•</span>
                      <span className="text-xs text-slate-300 font-medium">
                        {driverData.ongoingJob.game}
                      </span>
                    </div>

                    <div className="flex items-center gap-2 text-[11px] text-slate-400">
                      <Clock className="w-3.5 h-3.5 text-amber-400" />
                      <span>
                        Checkpoint Terakhir:{" "}
                        {driverData.ongoingJob.checkpoint?.savedAt
                          ? new Date(
                              driverData.ongoingJob.checkpoint.savedAt,
                            ).toLocaleTimeString("id-ID", {
                              hour: "2-digit",
                              minute: "2-digit",
                              second: "2-digit",
                            })
                          : "Sinkronisasi Berjalan"}
                      </span>
                    </div>
                  </div>

                  {/* Route & Cargo Header */}
                  <div className="grid grid-cols-1 md:grid-cols-3 gap-4 items-center">
                    <div className="md:col-span-2 flex items-center gap-3">
                      <div className="w-10 h-10 rounded-xl bg-amber-500/10 border border-amber-500/30 flex items-center justify-center text-amber-400 shrink-0">
                        <Compass className="w-5 h-5" />
                      </div>
                      <div className="flex flex-col">
                        <div className="flex items-center gap-2 text-base font-black text-white">
                          <span>{driverData.ongoingJob.sourceCity}</span>
                          <span className="text-amber-400">➜</span>
                          <span>{driverData.ongoingJob.destinationCity}</span>
                        </div>
                        <span className="text-xs text-slate-400">
                          Muatan:{" "}
                          <strong className="text-slate-200">
                            {driverData.ongoingJob.cargoName}
                          </strong>
                        </span>
                      </div>
                    </div>

                    {/* Progress Stats Box */}
                    <div className="p-3 rounded-xl bg-slate-950/70 border border-slate-800 flex items-center justify-between">
                      <div>
                        <span className="text-[10px] text-slate-400 block uppercase font-mono">
                          JARAK TERCATAT
                        </span>
                        <span className="text-sm font-black text-amber-300 font-mono">
                          {Math.round(
                            driverData.ongoingJob.checkpoint?.drivenDistanceKm || 0,
                          )}{" "}
                          km
                        </span>
                      </div>
                      <div className="text-right">
                        <span className="text-[10px] text-slate-400 block uppercase font-mono">
                          KERUSAKAN ARMADA
                        </span>
                        <span className="text-sm font-black text-emerald-400 font-mono">
                          {(
                            (driverData.ongoingJob.checkpoint?.accumVehicleDamage || 0) * 100
                          ).toFixed(1)}
                          %
                        </span>
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            ) : driverData?.lastCompletedJob ? (
              /* CASE 2: LAST COMPLETED JOB RECEIPT */
              <div className="rounded-2xl bg-gradient-to-br from-slate-900/80 via-slate-900/50 to-slate-950 border border-slate-800/90 p-5 shadow-xl relative overflow-hidden">
                <div className="flex items-center justify-between border-b border-slate-800/80 pb-3 mb-3">
                  <div className="flex items-center gap-2">
                    <span className="px-2.5 py-0.5 rounded-full bg-slate-800 border border-slate-700 text-slate-300 text-[11px] font-bold">
                      REKAP PENGIRIMAN TERAKHIR
                    </span>
                    <span className="text-xs text-slate-400 font-mono">
                      Job #{driverData.lastCompletedJob.jobId}
                    </span>
                  </div>
                  {driverData.lastCompletedJob.completedAt && (
                    <span className="text-[11px] text-slate-400">
                      Diselesaikan:{" "}
                      {new Date(
                        driverData.lastCompletedJob.completedAt,
                      ).toLocaleDateString("id-ID", {
                        day: "numeric",
                        month: "short",
                        year: "numeric",
                      })}
                    </span>
                  )}
                </div>

                <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
                  <div className="flex items-center gap-3">
                    <div className="w-9 h-9 rounded-xl bg-slate-800 border border-slate-700 flex items-center justify-center text-slate-300 shrink-0">
                      <CheckCircle className="w-4 h-4 text-emerald-400" />
                    </div>
                    <div>
                      <div className="text-sm font-bold text-white flex items-center gap-2">
                        <span>{driverData.lastCompletedJob.sourceCity}</span>
                        <span className="text-slate-500">➜</span>
                        <span>{driverData.lastCompletedJob.destinationCity}</span>
                      </div>
                      <span className="text-xs text-slate-400">
                        {driverData.lastCompletedJob.cargoName} •{" "}
                        {driverData.lastCompletedJob.distanceKm} km
                      </span>
                    </div>
                  </div>

                  <div className="flex items-center gap-3">
                    <div className="px-3 py-1.5 rounded-xl bg-amber-500/10 border border-amber-500/20 text-right">
                      <span className="text-[10px] text-slate-400 block">PENDAPATAN</span>
                      <span className="text-xs font-mono font-bold text-amber-300">
                        +NC {driverData.lastCompletedJob.revenue.toLocaleString()}
                      </span>
                    </div>
                    <div className="px-3 py-1.5 rounded-xl bg-emerald-500/10 border border-emerald-500/20 text-right">
                      <span className="text-[10px] text-slate-400 block">KERUSAKAN</span>
                      <span className="text-xs font-mono font-bold text-emerald-400">
                        {driverData.lastCompletedJob.damagePercentage}%
                      </span>
                    </div>
                  </div>
                </div>
              </div>
            ) : (
              /* CASE 3: READY TO DISPATCH */
              <div className="rounded-2xl bg-slate-900/60 border border-slate-800/80 p-5 flex items-center justify-between">
                <div className="flex items-center gap-3.5">
                  <div className="w-10 h-10 rounded-xl bg-slate-800 border border-slate-700 flex items-center justify-center text-amber-400 shrink-0">
                    <Truck className="w-5 h-5" />
                  </div>
                  <div>
                    <h3 className="text-sm font-bold text-white">
                      Hub Operasional Nismara Transport
                    </h3>
                    <p className="text-xs text-slate-400 mt-0.5">
                      Pilih salah satu simulator atau klien multiplayer di bawah untuk
                      memulai pencatatan telemetri in-game otomatis.
                    </p>
                  </div>
                </div>
                <div className="px-3 py-1 rounded-full bg-slate-800 border border-slate-700 text-slate-300 text-xs font-mono">
                  Siap Meluncur
                </div>
              </div>
            )}

            {/* DYNAMIC LAUNCHER SECTION (HANYA SIMULATOR & MULTIPLAYER YANG TERDETEKSI) */}
            <div className="space-y-4">
              <div className="flex items-center justify-between">
                <div>
                  <h2 className="text-lg font-black text-white uppercase tracking-wider flex items-center gap-2">
                    <Gamepad2 className="w-5 h-5 text-amber-400" />
                    <span>Simulator & Klien Terpasang</span>
                  </h2>
                  <p className="text-xs text-slate-400 mt-0.5">
                    Hanya game dan klien multiplayer yang terdeteksi di komputer Anda
                    yang ditampilkan di sini.
                  </p>
                </div>

                <button
                  onClick={() => handleAutoSyncPlugins(false)}
                  className="px-3 py-1.5 rounded-xl bg-slate-900 hover:bg-slate-800 border border-slate-700/80 text-xs font-semibold text-slate-300 hover:text-white flex items-center gap-1.5 transition-all cursor-pointer"
                  title="Sinkronkan binary plugin NismaraTracker.dll ke seluruh folder game"
                >
                  <FolderSync className="w-3.5 h-3.5 text-amber-400" />
                  <span>Sinkronkan Plugin</span>
                </button>
              </div>

              {/* Dynamic Grid: If no game is detected */}
              {installedGames.length === 0 ? (
                <div className="p-8 rounded-2xl bg-slate-900/40 border border-dashed border-slate-800 text-center space-y-3">
                  <AlertCircle className="w-8 h-8 text-amber-400 mx-auto" />
                  <h3 className="text-sm font-bold text-white">
                    Simulator Belum Terdeteksi
                  </h3>
                  <p className="text-xs text-slate-400 max-w-md mx-auto">
                    ETS2, ATS, TruckersMP, atau HaulMP belum ditemukan di lokasi
                    standar. Anda dapat menentukan file executable (.exe) secara manual
                    di menu Pengaturan.
                  </p>
                  <button
                    onClick={() => setActiveTab("settings")}
                    className="px-4 py-2 rounded-xl bg-amber-500 hover:bg-amber-400 text-slate-950 font-bold text-xs inline-flex items-center gap-1.5 transition-all cursor-pointer"
                  >
                    <FolderOpen className="w-3.5 h-3.5" />
                    <span>Buka Pengaturan Path Executable</span>
                  </button>
                </div>
              ) : (
                /* Dynamic Cards Grid */
                <div
                  className={`grid gap-5 ${
                    installedGames.length === 1
                      ? "grid-cols-1"
                      : "grid-cols-1 md:grid-cols-2"
                  }`}
                >
                  {installedGames.map((game) => {
                    const isEts2 = game.id === "ets2";
                    const isAts = game.id === "ats";
                    const isTmp = game.id === "truckersmp";
                    const isHmp = game.id === "haulmp";

                    return (
                      <div
                        key={game.id}
                        className="rounded-2xl border border-slate-800/90 bg-[#0c101a] overflow-hidden flex flex-col justify-between group hover:border-slate-700 transition-all shadow-xl relative min-h-[220px]"
                      >
                        {/* Artwork Background for ETS2 & ATS */}
                        {isEts2 && (
                          <div
                            className="absolute inset-0 bg-cover bg-center opacity-30 group-hover:scale-105 transition-transform duration-700"
                            style={{
                              backgroundImage: "url('/images/ets2_truck_images.webp')",
                            }}
                          />
                        )}
                        {isAts && (
                          <div
                            className="absolute inset-0 bg-cover bg-center opacity-30 group-hover:scale-105 transition-transform duration-700"
                            style={{
                              backgroundImage: "url('/images/ats_truck_images.webp')",
                            }}
                          />
                        )}
                        {isTmp && (
                          <div className="absolute inset-0 bg-gradient-to-br from-red-950/20 via-slate-950/60 to-slate-950 opacity-80" />
                        )}
                        {isHmp && (
                          <div className="absolute inset-0 bg-gradient-to-br from-cyan-950/20 via-slate-950/60 to-slate-950 opacity-80" />
                        )}

                        <div className="absolute inset-0 bg-gradient-to-t from-[#0c101a] via-[#0c101a]/70 to-transparent pointer-events-none" />

                        {/* Card Content Header */}
                        <div className="relative z-10 p-5 flex items-start justify-between">
                          <div className="flex items-center gap-3">
                            {isEts2 && (
                              <img
                                src="/images/ets2-logo.webp"
                                alt="ETS2"
                                className="h-10 w-auto object-contain filter drop-shadow"
                              />
                            )}
                            {isAts && (
                              <img
                                src="/images/ats-logo.webp"
                                alt="ATS"
                                className="h-10 w-auto object-contain filter drop-shadow"
                              />
                            )}
                            {isTmp && (
                              <div className="flex items-center gap-2">
                                <div className="w-8 h-8 rounded-lg bg-red-600/20 border border-red-500/40 flex items-center justify-center font-black text-red-400 text-xs font-mono">
                                  TMP
                                </div>
                                <span className="text-sm font-black text-white tracking-wider">
                                  TruckersMP Launcher
                                </span>
                              </div>
                            )}
                            {isHmp && (
                              <div className="flex items-center gap-2">
                                <div className="w-8 h-8 rounded-lg bg-cyan-600/20 border border-cyan-500/40 flex items-center justify-center font-black text-cyan-400 text-xs font-mono">
                                  HMP
                                </div>
                                <span className="text-sm font-black text-white tracking-wider">
                                  HaulMP Launcher
                                </span>
                              </div>
                            )}
                          </div>

                          {/* Running State Badge */}
                          {game.is_running ? (
                            <div className="flex items-center gap-1.5 px-2.5 py-1 rounded-full bg-emerald-500/20 border border-emerald-500/40 text-emerald-300 text-xs font-bold backdrop-blur-md">
                              <Radio className="w-3 h-3 text-emerald-400 animate-pulse" />
                              <span>Sedang Berjalan</span>
                            </div>
                          ) : (
                            <span className="text-[11px] text-slate-400 font-mono">
                              {isTmp || isHmp ? "Multiplayer Client" : "Simulator Native"}
                            </span>
                          )}
                        </div>

                        {/* Card Bottom Controls */}
                        <div className="relative z-10 p-5 pt-0 space-y-3">
                          {/* Plugin State for ETS2 & ATS */}
                          {(isEts2 || isAts) && (
                            <div className="flex items-center justify-between text-xs">
                              <span className="text-slate-400">Status Plugin:</span>
                              {!game.plugin_installed ? (
                                <span className="text-amber-400 font-semibold flex items-center gap-1">
                                  <AlertCircle className="w-3.5 h-3.5" />
                                  <span>Belum Terpasang</span>
                                </span>
                              ) : game.plugin_outdated ? (
                                <span className="text-amber-400 font-semibold flex items-center gap-1">
                                  <ArrowUpCircle className="w-3.5 h-3.5 animate-pulse" />
                                  <span>Update Tersedia</span>
                                </span>
                              ) : (
                                <span className="text-emerald-400 font-semibold flex items-center gap-1">
                                  <CheckCircle2 className="w-3.5 h-3.5" />
                                  <span>Plugin Terpasang</span>
                                </span>
                              )}
                            </div>
                          )}

                          {/* Action Buttons */}
                          <div className="flex items-center gap-2 pt-1">
                            <button
                              onClick={() => handleLaunchClient(game.id)}
                              disabled={loadingAction === `launch_${game.id}`}
                              className="flex-1 py-3 px-4 rounded-xl bg-gradient-to-r from-amber-500 to-amber-600 hover:from-amber-400 hover:to-amber-500 text-slate-950 font-bold text-xs shadow-lg shadow-amber-500/20 flex items-center justify-center gap-2 transition-all cursor-pointer active:scale-98 disabled:opacity-50"
                            >
                              <Play className="w-3.5 h-3.5 fill-current" />
                              <span>
                                {loadingAction === `launch_${game.id}`
                                  ? "Meluncurkan..."
                                  : `Luncurkan ${game.name}`}
                              </span>
                            </button>

                            {(isEts2 || isAts) && (
                              <button
                                onClick={() => handleInstallPlugin(game.id)}
                                disabled={loadingAction === `install_${game.id}`}
                                className="p-3 rounded-xl bg-slate-900/90 hover:bg-slate-800 border border-slate-700/80 text-slate-300 hover:text-white transition-all cursor-pointer"
                                title="Pasang / Verifikasi file NismaraTracker.dll"
                              >
                                <FolderSync className="w-4 h-4 text-amber-400" />
                              </button>
                            )}
                          </div>
                        </div>
                      </div>
                    );
                  })}
                </div>
              )}
            </div>
          </div>
        )}

        {/* VIEW: NEWS (SANITY CMS) */}
        {activeTab === "news" && (
          <div className="p-6 max-w-5xl mx-auto space-y-6">
            <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
              <div>
                <h2 className="text-xl font-black text-white flex items-center gap-2">
                  <Newspaper className="w-5 h-5 text-amber-400" />
                  <span>Warta & Pengumuman Nismara</span>
                </h2>
                <p className="text-xs text-slate-400 mt-0.5">
                  Publikasi resmi dan warta internal pengemudi Nismara Transport.
                </p>
              </div>

              <button
                onClick={() => fetchSanityArticles(true)}
                disabled={newsLoading}
                className="px-3.5 py-1.5 rounded-xl bg-slate-900 hover:bg-slate-800 border border-slate-800 text-xs font-semibold text-slate-300 flex items-center gap-1.5 cursor-pointer disabled:opacity-50"
              >
                <RefreshCw
                  className={`w-3.5 h-3.5 ${newsLoading ? "animate-spin text-amber-400" : ""}`}
                />
                <span>Segarkan</span>
              </button>
            </div>

            {/* Category Pills */}
            <div className="flex items-center gap-2 overflow-x-auto pb-2 scrollbar-none">
              {availableCategories.map((cat) => (
                <button
                  key={cat}
                  onClick={() => setSelectedCategory(cat)}
                  className={`px-3 py-1 rounded-full text-xs font-semibold transition-all cursor-pointer ${
                    selectedCategory === cat
                      ? "bg-amber-500 text-slate-950 font-bold"
                      : "bg-slate-900 text-slate-400 hover:text-slate-200 border border-slate-800"
                  }`}
                >
                  {cat === "ALL" ? "Semua Kategori" : cat}
                </button>
              ))}
            </div>

            {/* Articles Grid */}
            {newsLoading && articles.length === 0 ? (
              <div className="p-12 text-center text-xs text-slate-400 animate-pulse">
                Memuat publikasi warta...
              </div>
            ) : filteredArticles.length === 0 ? (
              <div className="p-12 rounded-2xl bg-slate-900/40 border border-slate-800 text-center text-xs text-slate-400">
                Belum ada warta dalam kategori ini.
              </div>
            ) : (
              <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                {filteredArticles.map((article) => {
                  const imgUrl = getSanityImageUrl(article.mainImage?.asset?._ref);
                  return (
                    <div
                      key={article._id}
                      onClick={() => setReadingArticle(article)}
                      className="rounded-2xl bg-slate-900/70 border border-slate-800/80 p-4 hover:border-slate-700 transition-all cursor-pointer flex flex-col justify-between group space-y-3"
                    >
                      {imgUrl && (
                        <div className="h-40 w-full rounded-xl overflow-hidden bg-slate-950">
                          <img
                            src={imgUrl}
                            alt={article.title}
                            className="w-full h-full object-cover group-hover:scale-105 transition-transform duration-500"
                          />
                        </div>
                      )}
                      <div>
                        <div className="flex items-center gap-2 text-[10px] text-slate-400 mb-1">
                          <Calendar className="w-3 h-3" />
                          <span>{formatNewsDate(article.publishedAt)}</span>
                          {article.categories?.[0]?.title && (
                            <span
                              className={`px-2 py-0.5 rounded-full border text-[9px] font-bold ${getCategoryColor(
                                article.categories[0].title,
                              )}`}
                            >
                              {article.categories[0].title}
                            </span>
                          )}
                        </div>
                        <h3 className="text-sm font-bold text-white group-hover:text-amber-400 transition-colors line-clamp-2">
                          {article.title}
                        </h3>
                        {article.seoDescription && (
                          <p className="text-xs text-slate-400 line-clamp-2 mt-1">
                            {article.seoDescription}
                          </p>
                        )}
                      </div>

                      <div className="pt-2 border-t border-slate-800/80 flex items-center justify-between text-[11px] text-amber-400 font-semibold">
                        <span>Baca Selengkapnya</span>
                        <span>➜</span>
                      </div>
                    </div>
                  );
                })}
              </div>
            )}
          </div>
        )}

        {/* VIEW: EVENTS & MISSIONS */}
        {activeTab === "events" && (
          <div className="p-6 max-w-5xl mx-auto space-y-6">
            <div className="flex items-center justify-between">
              <div>
                <h2 className="text-xl font-black text-white flex items-center gap-2">
                  <Trophy className="w-5 h-5 text-amber-400" />
                  <span>Event & Misi Komunitas</span>
                </h2>
                <p className="text-xs text-slate-400 mt-0.5">
                  Kontrak khusus, kupon bonus NC, dan program misi VTC Nismara Transport.
                </p>
              </div>

              <button
                onClick={fetchEvents}
                disabled={eventsLoading}
                className="px-3.5 py-1.5 rounded-xl bg-slate-900 hover:bg-slate-800 border border-slate-800 text-xs font-semibold text-slate-300 flex items-center gap-1.5 cursor-pointer disabled:opacity-50"
              >
                <RefreshCw
                  className={`w-3.5 h-3.5 ${eventsLoading ? "animate-spin text-amber-400" : ""}`}
                />
                <span>Segarkan</span>
              </button>
            </div>

            {/* Event Filter Pills */}
            <div className="flex items-center gap-2">
              {[
                { id: "ALL", label: "Semua Event" },
                { id: "CONTRACT", label: "Kontrak Khusus" },
                { id: "COUPON", label: "Kupon Hadiah" },
                { id: "GIVEAWAY", label: "Giveaway & Misi" },
                { id: "NC_EVENT", label: "Bonus NC" },
              ].map((pill) => (
                <button
                  key={pill.id}
                  onClick={() => setSelectedEventType(pill.id)}
                  className={`px-3 py-1 rounded-full text-xs font-semibold transition-all cursor-pointer ${
                    selectedEventType === pill.id
                      ? "bg-amber-500 text-slate-950 font-bold"
                      : "bg-slate-900 text-slate-400 hover:text-slate-200 border border-slate-800"
                  }`}
                >
                  {pill.label}
                </button>
              ))}
            </div>

            {/* Events List */}
            {eventsLoading && eventsFeed.length === 0 ? (
              <div className="p-12 text-center text-xs text-slate-400 animate-pulse">
                Memuat data event...
              </div>
            ) : filteredEvents.length === 0 ? (
              <div className="p-12 rounded-2xl bg-slate-900/40 border border-slate-800 text-center text-xs text-slate-400">
                Tidak ada event aktif pada kategori ini.
              </div>
            ) : (
              <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                {filteredEvents.map((event) => (
                  <div
                    key={event.id}
                    className="rounded-2xl bg-slate-900/70 border border-slate-800/80 p-5 flex flex-col justify-between space-y-4 hover:border-slate-700 transition-all"
                  >
                    <div className="space-y-2">
                      <div className="flex items-center justify-between">
                        <span className="px-2.5 py-0.5 rounded-full bg-amber-500/15 border border-amber-500/30 text-amber-300 text-[10px] font-bold">
                          {event.typeLabel}
                        </span>
                        {event.isActive && (
                          <span className="text-[10px] text-emerald-400 font-bold">
                            ● Aktif
                          </span>
                        )}
                      </div>
                      <h3 className="text-sm font-bold text-white">{event.title}</h3>
                      {event.description && (
                        <p className="text-xs text-slate-400 line-clamp-2">
                          {event.description}
                        </p>
                      )}
                    </div>

                    {/* Kupon Code if Coupon */}
                    {event.type === "COUPON" && event.couponCode && (
                      <div className="p-3 rounded-xl bg-slate-950 border border-amber-500/20 flex items-center justify-between">
                        <div>
                          <span className="text-[9px] text-slate-400 block font-mono">
                            KODE KUPON
                          </span>
                          <span className="text-sm font-mono font-bold text-amber-300">
                            {event.couponCode}
                          </span>
                        </div>
                        <button
                          onClick={() => handleCopyCouponCode(event.couponCode || "")}
                          className="px-3 py-1 rounded-lg bg-amber-500/20 hover:bg-amber-500/30 text-amber-300 text-xs font-semibold flex items-center gap-1 cursor-pointer"
                        >
                          {copiedCouponCode === event.couponCode ? (
                            <>
                              <Check className="w-3.5 h-3.5 text-emerald-400" />
                              <span>Tersalin</span>
                            </>
                          ) : (
                            <>
                              <Copy className="w-3.5 h-3.5" />
                              <span>Salin</span>
                            </>
                          )}
                        </button>
                      </div>
                    )}

                    <button
                      onClick={() => setSelectedEventDetail(event)}
                      className="w-full py-2 rounded-xl bg-slate-800 hover:bg-slate-700 text-slate-200 text-xs font-bold transition-all cursor-pointer"
                    >
                      Lihat Rincian Event
                    </button>
                  </div>
                ))}
              </div>
            )}
          </div>
        )}

        {/* VIEW: SETTINGS (STRICT ANTI AI-SLOP COPYWRITING) */}
        {activeTab === "settings" && (
          <div className="p-6 max-w-4xl mx-auto space-y-6">
            <div>
              <h2 className="text-xl font-black text-white flex items-center gap-2">
                <SettingsIcon className="w-5 h-5 text-amber-400" />
                <span>Pengaturan Sistem</span>
              </h2>
              <p className="text-xs text-slate-400 mt-0.5">
                Konfigurasi lokasi file executable, autentikasi driver, dan integrasi
                system tray.
              </p>
            </div>

            {/* SECTION 1: LOKASI EXECUTABLE (.EXE) SIMULATOR & MULTIPLAYER */}
            <div className="p-5 rounded-2xl bg-slate-900/70 border border-slate-800 space-y-4">
              <div className="flex items-center justify-between border-b border-slate-800 pb-3">
                <div className="flex items-center gap-2">
                  <FolderOpen className="w-4 h-4 text-amber-400" />
                  <h3 className="text-xs font-bold text-white uppercase tracking-wider">
                    Lokasi File Executable (.exe)
                  </h3>
                </div>
                <span className="text-[11px] text-slate-400">
                  Dapat ditentukan manual jika diinstal di folder non-standar
                </span>
              </div>

              {/* Row 1: ETS2 */}
              <div className="space-y-1.5">
                <div className="flex items-center justify-between text-xs">
                  <label className="font-semibold text-slate-200">
                    Euro Truck Simulator 2 (eurotrucks2.exe)
                  </label>
                  <span className="text-[11px] text-slate-400">
                    {customEts2Path ? "Lokasi Manual" : "Deteksi Otomatis Steam"}
                  </span>
                </div>
                <div className="flex items-center gap-2">
                  <input
                    type="text"
                    value={customEts2Path}
                    readOnly
                    placeholder="Deteksi otomatis via Steam libraryfolders.vdf"
                    className="flex-1 px-3 py-2 rounded-xl bg-slate-950/80 border border-slate-800 text-xs font-mono text-slate-300 focus:outline-none"
                  />
                  <button
                    onClick={() => handleBrowseExe("ets2")}
                    className="px-3 py-2 rounded-xl bg-slate-800 hover:bg-slate-700 text-xs font-semibold text-slate-200 flex items-center gap-1.5 transition-all cursor-pointer"
                  >
                    <FolderOpen className="w-3.5 h-3.5 text-amber-400" />
                    <span>Cari File</span>
                  </button>
                  {customEts2Path && (
                    <button
                      onClick={() => handleResetExe("ets2")}
                      className="p-2 rounded-xl bg-slate-800 hover:bg-slate-700 text-slate-400 hover:text-white transition-all cursor-pointer"
                      title="Reset ke deteksi otomatis"
                    >
                      <RotateCcw className="w-3.5 h-3.5" />
                    </button>
                  )}
                </div>
              </div>

              {/* Row 2: ATS */}
              <div className="space-y-1.5 pt-2 border-t border-slate-800/60">
                <div className="flex items-center justify-between text-xs">
                  <label className="font-semibold text-slate-200">
                    American Truck Simulator (amtrucks.exe)
                  </label>
                  <span className="text-[11px] text-slate-400">
                    {customAtsPath ? "Lokasi Manual" : "Deteksi Otomatis Steam"}
                  </span>
                </div>
                <div className="flex items-center gap-2">
                  <input
                    type="text"
                    value={customAtsPath}
                    readOnly
                    placeholder="Deteksi otomatis via Steam libraryfolders.vdf"
                    className="flex-1 px-3 py-2 rounded-xl bg-slate-950/80 border border-slate-800 text-xs font-mono text-slate-300 focus:outline-none"
                  />
                  <button
                    onClick={() => handleBrowseExe("ats")}
                    className="px-3 py-2 rounded-xl bg-slate-800 hover:bg-slate-700 text-xs font-semibold text-slate-200 flex items-center gap-1.5 transition-all cursor-pointer"
                  >
                    <FolderOpen className="w-3.5 h-3.5 text-amber-400" />
                    <span>Cari File</span>
                  </button>
                  {customAtsPath && (
                    <button
                      onClick={() => handleResetExe("ats")}
                      className="p-2 rounded-xl bg-slate-800 hover:bg-slate-700 text-slate-400 hover:text-white transition-all cursor-pointer"
                      title="Reset ke deteksi otomatis"
                    >
                      <RotateCcw className="w-3.5 h-3.5" />
                    </button>
                  )}
                </div>
              </div>

              {/* Row 3: TruckersMP */}
              <div className="space-y-1.5 pt-2 border-t border-slate-800/60">
                <div className="flex items-center justify-between text-xs">
                  <label className="font-semibold text-slate-200">
                    TruckersMP Launcher (TruckersMP-Launcher.exe)
                  </label>
                  <span className="text-[11px] text-slate-400">
                    {customTruckersmpPath ? "Lokasi Manual" : "Deteksi Otomatis Registri"}
                  </span>
                </div>
                <div className="flex items-center gap-2">
                  <input
                    type="text"
                    value={customTruckersmpPath}
                    readOnly
                    placeholder="Deteksi otomatis di AppData / Program Files / Registri Windows"
                    className="flex-1 px-3 py-2 rounded-xl bg-slate-950/80 border border-slate-800 text-xs font-mono text-slate-300 focus:outline-none"
                  />
                  <button
                    onClick={() => handleBrowseExe("truckersmp")}
                    className="px-3 py-2 rounded-xl bg-slate-800 hover:bg-slate-700 text-xs font-semibold text-slate-200 flex items-center gap-1.5 transition-all cursor-pointer"
                  >
                    <FolderOpen className="w-3.5 h-3.5 text-amber-400" />
                    <span>Cari File</span>
                  </button>
                  {customTruckersmpPath && (
                    <button
                      onClick={() => handleResetExe("truckersmp")}
                      className="p-2 rounded-xl bg-slate-800 hover:bg-slate-700 text-slate-400 hover:text-white transition-all cursor-pointer"
                      title="Reset ke deteksi otomatis"
                    >
                      <RotateCcw className="w-3.5 h-3.5" />
                    </button>
                  )}
                </div>
              </div>

              {/* Row 4: HaulMP */}
              <div className="space-y-1.5 pt-2 border-t border-slate-800/60">
                <div className="flex items-center justify-between text-xs">
                  <label className="font-semibold text-slate-200">
                    HaulMP Launcher (HaulMP Launcher.exe)
                  </label>
                  <span className="text-[11px] text-slate-400">
                    {customHaulmpPath ? "Lokasi Manual" : "Deteksi Otomatis LocalAppData"}
                  </span>
                </div>
                <div className="flex items-center gap-2">
                  <input
                    type="text"
                    value={customHaulmpPath}
                    readOnly
                    placeholder="Deteksi otomatis di AppData / Programs"
                    className="flex-1 px-3 py-2 rounded-xl bg-slate-950/80 border border-slate-800 text-xs font-mono text-slate-300 focus:outline-none"
                  />
                  <button
                    onClick={() => handleBrowseExe("haulmp")}
                    className="px-3 py-2 rounded-xl bg-slate-800 hover:bg-slate-700 text-xs font-semibold text-slate-200 flex items-center gap-1.5 transition-all cursor-pointer"
                  >
                    <FolderOpen className="w-3.5 h-3.5 text-amber-400" />
                    <span>Cari File</span>
                  </button>
                  {customHaulmpPath && (
                    <button
                      onClick={() => handleResetExe("haulmp")}
                      className="p-2 rounded-xl bg-slate-800 hover:bg-slate-700 text-slate-400 hover:text-white transition-all cursor-pointer"
                      title="Reset ke deteksi otomatis"
                    >
                      <RotateCcw className="w-3.5 h-3.5" />
                    </button>
                  )}
                </div>
              </div>
            </div>

            {/* SECTION 2: AUTENTIKASI AKUN DRIVER */}
            <div className="p-5 rounded-2xl bg-slate-900/70 border border-slate-800 space-y-4">
              <div className="flex items-center justify-between border-b border-slate-800 pb-3">
                <div className="flex items-center gap-2">
                  <User className="w-4 h-4 text-amber-400" />
                  <h3 className="text-xs font-bold text-white uppercase tracking-wider">
                    Autentikasi Akun Driver
                  </h3>
                </div>
                <a
                  href="https://transport.nismara.web.id/profile"
                  target="_blank"
                  rel="noreferrer"
                  className="text-xs text-amber-400 hover:underline flex items-center gap-1 cursor-pointer"
                >
                  <span>Buka Profil Web untuk Salin Token</span>
                  <ExternalLink className="w-3 h-3" />
                </a>
              </div>

              <div className="space-y-2">
                <label className="text-xs font-medium text-slate-300 block">
                  Token Driver (NSM-XXXXXX-...)
                </label>
                <div className="flex items-center gap-2">
                  <input
                    type="password"
                    value={driverToken}
                    onChange={(e) => setDriverToken(e.target.value)}
                    placeholder="Tempel token driver resmi Nismara Transport di sini"
                    className="flex-1 px-3 py-2.5 rounded-xl bg-slate-950/80 border border-slate-800 text-xs font-mono text-slate-100 focus:outline-none focus:border-amber-500"
                  />
                  <button
                    onClick={handleSaveDriverToken}
                    className="px-4 py-2.5 rounded-xl bg-amber-500 hover:bg-amber-400 text-slate-950 font-bold text-xs flex items-center gap-1.5 shadow-md shadow-amber-500/20 transition-all cursor-pointer"
                  >
                    <Save className="w-3.5 h-3.5" />
                    <span>Verifikasi & Simpan</span>
                  </button>
                </div>
                {resolvedConfigPath && (
                  <p className="text-[11px] text-slate-500 font-mono">
                    Disinkronkan ke: {resolvedConfigPath}
                  </p>
                )}
              </div>

              {driverData && (
                <div className="p-3.5 rounded-xl bg-slate-950/80 border border-emerald-500/30 flex items-center justify-between">
                  <div className="flex items-center gap-3">
                    <div className="w-8 h-8 rounded-full bg-emerald-500/20 border border-emerald-500/40 flex items-center justify-center text-xs font-bold text-emerald-300">
                      ✓
                    </div>
                    <div>
                      <span className="text-xs font-bold text-white block">
                        Terhubung sebagai {driverData.name} ({driverData.truckyRank})
                      </span>
                      <span className="text-[11px] text-slate-400">
                        Saldo Dompet: NC {(driverData.totalNC || 0).toLocaleString()} •
                        Poin Penalti: {driverData.points || 0}
                      </span>
                    </div>
                  </div>
                  <span className="px-2.5 py-0.5 rounded-full bg-emerald-500/20 text-emerald-300 text-[10px] font-bold">
                    TERVERIFIKASI
                  </span>
                </div>
              )}
            </div>

            {/* SECTION 3: PREFERENSI SYSTEM TRAY & OTOMATISASI */}
            <div className="p-5 rounded-2xl bg-slate-900/70 border border-slate-800 space-y-3">
              <h3 className="text-xs font-bold text-white uppercase tracking-wider border-b border-slate-800 pb-2">
                Preferensi Aplikasi & System Tray
              </h3>

              <div className="space-y-2.5 pt-1 text-xs">
                <label className="flex items-center gap-3 cursor-pointer">
                  <input
                    type="checkbox"
                    checked={autoMinimizeOnLaunch}
                    onChange={(e) => {
                      setAutoMinimizeOnLaunch(e.target.checked);
                      localStorage.setItem(
                        "nsm_auto_minimize",
                        e.target.checked ? "1" : "0",
                      );
                    }}
                    className="w-4 h-4 rounded bg-slate-950 border-slate-700 text-amber-500 focus:ring-0"
                  />
                  <span className="text-slate-300">
                    Sembunyikan Companion otomatis ke System Tray saat game diluncurkan
                  </span>
                </label>

                <label className="flex items-center gap-3 cursor-pointer">
                  <input
                    type="checkbox"
                    checked={autoSyncOnStartup}
                    onChange={(e) => {
                      setAutoSyncOnStartup(e.target.checked);
                      localStorage.setItem(
                        "nsm_auto_sync",
                        e.target.checked ? "1" : "0",
                      );
                    }}
                    className="w-4 h-4 rounded bg-slate-950 border-slate-700 text-amber-500 focus:ring-0"
                  />
                  <span className="text-slate-300">
                    Sinkronkan file NismaraTracker.dll ke folder game saat aplikasi dibuka
                  </span>
                </label>
              </div>
            </div>

            {/* SECTION 4: WIN32 MUTEX GATEKEEPER */}
            <div className="p-4 rounded-xl bg-slate-900/50 border border-slate-800 flex items-center justify-between text-xs">
              <div className="flex items-center gap-3">
                <ShieldCheck className="w-4 h-4 text-emerald-400" />
                <span className="text-slate-300 font-mono">
                  Global\NismaraCompanionRunning (Active Mutex Handle)
                </span>
              </div>
              <span className="text-slate-500 text-[11px]">
                Plugin in-game memverifikasi handle ini untuk mengizinkan telemetri
              </span>
            </div>
          </div>
        )}
      </main>

      {/* ARTICLE READER MODAL */}
      {readingArticle && (
        <div className="fixed inset-0 z-50 bg-black/80 backdrop-blur-md flex items-center justify-center p-4">
          <div className="bg-[#0b0f19] border border-slate-800 rounded-2xl max-w-2xl w-full max-h-[85vh] flex flex-col overflow-hidden shadow-2xl">
            <div className="p-4 border-b border-slate-800 flex items-center justify-between">
              <span className="text-xs text-slate-400">Warta Nismara Transport</span>
              <button
                onClick={() => setReadingArticle(null)}
                className="p-1 rounded-lg hover:bg-slate-800 text-slate-400 hover:text-white cursor-pointer"
              >
                <X className="w-4 h-4" />
              </button>
            </div>
            <div className="p-6 overflow-y-auto space-y-4">
              <h2 className="text-lg font-black text-white">{readingArticle.title}</h2>
              <div className="text-[11px] text-slate-400">
                Dipublikasikan pada {formatNewsDate(readingArticle.publishedAt)}
              </div>
              <div className="text-xs text-slate-300 leading-relaxed whitespace-pre-line border-t border-slate-800/80 pt-4">
                {extractBlockText(readingArticle.body) || readingArticle.seoDescription}
              </div>
            </div>
          </div>
        </div>
      )}

      {/* EVENT DETAIL MODAL */}
      {selectedEventDetail && (
        <div className="fixed inset-0 z-50 bg-black/80 backdrop-blur-md flex items-center justify-center p-4">
          <div className="bg-[#0b0f19] border border-slate-800 rounded-2xl max-w-xl w-full max-h-[85vh] flex flex-col overflow-hidden shadow-2xl">
            <div className="p-4 border-b border-slate-800 flex items-center justify-between">
              <span className="text-xs text-slate-400">
                Rincian {selectedEventDetail.typeLabel}
              </span>
              <button
                onClick={() => setSelectedEventDetail(null)}
                className="p-1 rounded-lg hover:bg-slate-800 text-slate-400 hover:text-white cursor-pointer"
              >
                <X className="w-4 h-4" />
              </button>
            </div>
            <div className="p-6 overflow-y-auto space-y-4 text-xs">
              <h2 className="text-base font-black text-white">
                {selectedEventDetail.title}
              </h2>
              <p className="text-slate-300 leading-relaxed">
                {selectedEventDetail.description}
              </p>
              {selectedEventDetail.couponCode && (
                <div className="p-3 rounded-xl bg-slate-950 border border-amber-500/30 flex items-center justify-between font-mono">
                  <span>KODE: {selectedEventDetail.couponCode}</span>
                  <button
                    onClick={() =>
                      handleCopyCouponCode(selectedEventDetail.couponCode || "")
                    }
                    className="px-3 py-1 rounded bg-amber-500/20 text-amber-300 font-sans font-bold cursor-pointer"
                  >
                    Salin
                  </button>
                </div>
              )}
            </div>
          </div>
        </div>
      )}

      {/* UPDATE MODAL */}
      {showUpdateModal && availableUpdate && (
        <div className="fixed inset-0 z-50 bg-black/80 backdrop-blur-md flex items-center justify-center p-4">
          <div className="bg-[#0b0f19] border border-amber-500/40 rounded-2xl max-w-md w-full p-6 shadow-2xl space-y-4">
            <div className="flex items-center gap-3">
              <div className="w-10 h-10 rounded-xl bg-amber-500/15 border border-amber-500/30 flex items-center justify-center text-amber-400">
                <ArrowUpCircle className="w-5 h-5" />
              </div>
              <div>
                <h3 className="text-sm font-bold text-white">
                  Pembaruan v{availableUpdate.version} Tersedia
                </h3>
                <p className="text-xs text-slate-400">
                  Versi baru Nismara Companion siap dipasang.
                </p>
              </div>
            </div>

            {updateDownloading ? (
              <div className="space-y-2 py-2">
                <div className="flex items-center justify-between text-xs text-slate-300">
                  <span>Mengunduh pembaruan...</span>
                  <span className="font-mono">
                    {downloadProgress && downloadProgress.total > 0
                      ? `${Math.round((downloadProgress.downloaded / downloadProgress.total) * 100)}%`
                      : "Memulai..."}
                  </span>
                </div>
                <div className="w-full h-2 rounded-full bg-slate-800 overflow-hidden">
                  <div
                    className="h-full bg-amber-500 transition-all duration-300"
                    style={{
                      width: `${
                        downloadProgress && downloadProgress.total > 0
                          ? Math.min(
                              100,
                              Math.round(
                                (downloadProgress.downloaded / downloadProgress.total) *
                                  100,
                              ),
                            )
                          : 15
                      }%`,
                    }}
                  />
                </div>
              </div>
            ) : (
              <div className="flex items-center justify-end gap-2 pt-2">
                <button
                  onClick={() => setShowUpdateModal(false)}
                  className="px-3.5 py-2 rounded-xl bg-slate-800 hover:bg-slate-700 text-xs font-semibold text-slate-300 cursor-pointer"
                >
                  Nanti Saja
                </button>
                <button
                  onClick={handleApplyUpdate}
                  className="px-4 py-2 rounded-xl bg-amber-500 hover:bg-amber-400 text-slate-950 text-xs font-bold shadow-md cursor-pointer"
                >
                  Pasang Sekarang
                </button>
              </div>
            )}
          </div>
        </div>
      )}
    </div>
  );
}
