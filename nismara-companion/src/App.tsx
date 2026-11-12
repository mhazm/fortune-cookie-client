import { useState, useEffect, useMemo } from "react";
import { invoke } from "@tauri-apps/api/core";
import { check, Update } from "@tauri-apps/plugin-updater";
import { relaunch } from "@tauri-apps/plugin-process";
import { openUrl } from "@tauri-apps/plugin-opener";
import {
  LayoutGrid,
  Newspaper,
  Trophy,
  Settings as SettingsIcon,
  Headphones,
  CheckCircle2,
  AlertCircle,
  Download,
  Info,
  ExternalLink,
  Radio,
  X,
  ShieldCheck,
  FolderSync,
  FolderOpen,
  Save,
  RefreshCw,
  Sparkles,
  ShieldAlert,
  ArrowUpCircle,
  Calendar,
  User,
  BookOpen,
  Gift,
  Copy,
  Check,
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
  // Ref format: image-<hash>-<width>x<height>-<ext>
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

const APP_VERSION = "1.0.2";

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
  const [showAboutModal, setShowAboutModal] = useState(false);

  // News from Sanity State
  const [articles, setArticles] = useState<SanityArticle[]>([]);
  const [newsLoading, setNewsLoading] = useState(false);
  const [newsError, setNewsError] = useState<string | null>(null);
  const [selectedCategory, setSelectedCategory] = useState<string>("ALL");
  const [readingArticle, setReadingArticle] = useState<SanityArticle | null>(null);

  // Events from Tracker Server (contracts, coupons, giveaways, ncevents)
  const [eventsFeed, setEventsFeed] = useState<EventFeedItem[]>([]);
  const [eventsLoading, setEventsLoading] = useState(false);
  const [eventsError, setEventsError] = useState<string | null>(null);
  const [selectedEventType, setSelectedEventType] = useState<string>("ALL");
  const [selectedEventDetail, setSelectedEventDetail] = useState<EventFeedItem | null>(null);
  const [copiedCouponCode, setCopiedCouponCode] = useState<string | null>(null);

  // Auto-Updater & Version Enforcement State
  const [updateChecking, setUpdateChecking] = useState(false);
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

  // Official Production Cloud Endpoint (Hardcoded & Secured for Beta Release)
  const PRODUCTION_API_URL = "http://103.150.101.154:3100";
  const serverUrl = PRODUCTION_API_URL;
  const [driverToken, setDriverToken] = useState(
    () => localStorage.getItem("nsm_driver_token") || "",
  );
  const [resolvedConfigPath, setResolvedConfigPath] = useState<string>("");

  const [customEts2Path, setCustomEts2Path] = useState(
    () => localStorage.getItem("nsm_ets2_path") || "",
  );
  const [customAtsPath, setCustomAtsPath] = useState(
    () => localStorage.getItem("nsm_ats_path") || "",
  );

  // Show toast notification
  const notify = (message: string, isError = false) => {
    setStatusToast({ message, isError });
    setTimeout(() => {
      setStatusToast(null);
    }, 4500);
  };

  // Safe fetch system status from Tauri backend with manual path overrides
  const refreshSystemStatus = async (
    ets2Override?: string,
    atsOverride?: string,
  ) => {
    const ePath = ets2Override !== undefined ? ets2Override : customEts2Path;
    const aPath = atsOverride !== undefined ? atsOverride : customAtsPath;

    try {
      const res = await invoke<SystemStatus>("get_system_status", {
        customEts2Path: ePath.trim() || null,
        customAtsPath: aPath.trim() || null,
      });
      setSystemStatus(res);
    } catch (err: any) {
      console.error("Failed to query system status:", err);
    }
  };

  // Check Tauri Updater against GitHub Releases
  const checkForUpdates = async (manual = false) => {
    setUpdateChecking(true);
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
          notify(`Nismara Companion sudah dalam versi terbaru (v${APP_VERSION}).`);
        }
      }
    } catch (err: any) {
      console.warn("Tauri updater check error:", err);
      const errMsg = String(err?.message || err);
      if (
        errMsg.includes("Could not fetch a valid release JSON") ||
        errMsg.includes("404") ||
        errMsg.includes("not found")
      ) {
        // When latest.json is 404 (initial release or no newer release published), app is already latest
        setAvailableUpdate(null);
        if (manual) {
          notify(`Nismara Companion sudah dalam versi terbaru (v${APP_VERSION}).`);
        }
      } else if (manual) {
        notify(`Pemeriksaan pembaruan: ${errMsg}`, true);
      }
    } finally {
      setUpdateChecking(false);
    }
  };

  // Check backend server version requirements (/api/tracker/version)
  const checkServerVersion = async (targetBaseUrl?: string) => {
    const url = (targetBaseUrl || serverUrl).trim().replace(/\/+$/, "");
    try {
      const resp = await fetch(`${url}/api/tracker/version`);
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
      // Backend might be offline during dev, ignore
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
        notify("Seluruh plugin game sudah dalam kondisi mutakhir.");
      }
    } catch (err: any) {
      if (!silent) {
        notify(`Auto-sync plugin: ${err}`, true);
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

  // Fetch Articles from Sanity CMS via Nismara Tracker Server (/api/tracker/news)
  const fetchSanityArticles = async (forceRefresh = false) => {
    setNewsLoading(true);
    setNewsError(null);
    try {
      const qs = forceRefresh ? "?refresh=1" : "";
      const endpoints = [
        `${PRODUCTION_API_URL}/api/tracker/news${qs}`,
        `http://localhost:3100/api/tracker/news${qs}`,
      ];

      let data: SanityArticle[] = [];
      let lastStatusError = "";

      for (const endpoint of endpoints) {
        try {
          const res = await fetch(endpoint, {
            headers: driverToken ? { Authorization: `Bearer ${driverToken}` } : {},
          });
          if (res.ok) {
            const json = await res.json();
            const list = Array.isArray(json.articles)
              ? json.articles
              : Array.isArray(json.data)
              ? json.data
              : Array.isArray(json.result)
              ? json.result
              : Array.isArray(json)
              ? json
              : [];
            if (list.length > 0) {
              data = list;
              break;
            }
          } else {
            lastStatusError = `HTTP ${res.status}`;
          }
        } catch (err: any) {
          lastStatusError = err?.message || String(err);
        }
      }

      if (data.length > 0) {
        setArticles(data);
      } else {
        setNewsError(
          lastStatusError
            ? `Gagal memuat berita dari server (${lastStatusError}).`
            : "Tidak ada artikel yang ditemukan saat ini.",
        );
      }
    } catch (err: any) {
      console.error("[News] Failed to load news from Tracker Server:", err);
      setNewsError(err?.message || "Gagal memuat berita dari server.");
    } finally {
      setNewsLoading(false);
    }
  };

  const availableCategories = useMemo(() => {
    const cats = new Set<string>();
    articles.forEach((a) => {
      a.categories?.forEach((c) => {
        if (c.title) cats.add(c.title);
      });
    });
    return ["ALL", ...Array.from(cats)];
  }, [articles]);

  const filteredArticles = useMemo(() => {
    if (selectedCategory === "ALL") return articles;
    return articles.filter((a) =>
      a.categories?.some((c) => c.title === selectedCategory)
    );
  }, [articles, selectedCategory]);

  // Fetch Events from Tracker Server (Contracts, Coupons, Giveaways, NC Events)
  const fetchTrackerEvents = async () => {
    setEventsLoading(true);
    setEventsError(null);
    try {
      const endpoints = [
        `${PRODUCTION_API_URL}/api/tracker/events`,
        `http://localhost:3100/api/tracker/events`,
      ];

      let loadedFeed: EventFeedItem[] | null = null;
      let lastErr = "";

      for (const endpoint of endpoints) {
        try {
          const res = await fetch(endpoint, {
            headers: driverToken ? { Authorization: `Bearer ${driverToken}` } : {},
          });
          if (res.ok) {
            const json: EventsApiResponse = await res.json();
            if (json.success && json.data?.feed) {
              loadedFeed = json.data.feed;
              break;
            }
          } else {
            lastErr = `Server status ${res.status}`;
          }
        } catch (e: any) {
          lastErr = e?.message || String(e);
        }
      }

      if (loadedFeed) {
        setEventsFeed(loadedFeed);
      } else {
        setEventsError(lastErr || "Data event tidak tersedia.");
      }
    } catch (err: any) {
      console.error("[Events] Failed to load events:", err);
      setEventsError(
        err?.message || "Gagal memuat event komunitas dari server.",
      );
    } finally {
      setEventsLoading(false);
    }
  };

  const filteredEvents = useMemo(() => {
    if (selectedEventType === "ALL") return eventsFeed;
    return eventsFeed.filter((e) => e.type === selectedEventType);
  }, [eventsFeed, selectedEventType]);

  const handleCopyCouponCode = (code: string) => {
    navigator.clipboard.writeText(code);
    setCopiedCouponCode(code);
    notify(`Kode kupon "${code}" berhasil disalin!`);
    setTimeout(() => setCopiedCouponCode(null), 3000);
  };

  // Load saved config & perform initial startup checks
  useEffect(() => {
    invoke<PluginConfigPayload>("get_plugin_config")
      .then((cfg) => {
        if (cfg.ets2_config_path) {
          setResolvedConfigPath(cfg.ets2_config_path);
        }
        if (cfg.driver_token) {
          setDriverToken(cfg.driver_token);
          localStorage.setItem("nsm_driver_token", cfg.driver_token);
        }
        checkServerVersion(PRODUCTION_API_URL);
      })
      .catch((err) => {
        console.warn("Could not load initial plugin config:", err);
        checkServerVersion(PRODUCTION_API_URL);
      });

    // Check for App Updates silently on launch
    checkForUpdates(false);
    // Auto-sync DLLs to games if games are closed
    handleAutoSyncPlugins(true);
    // Fetch News from Sanity CMS & Community Events from Server
    fetchSanityArticles();
    fetchTrackerEvents();
  }, []);

  useEffect(() => {
    refreshSystemStatus();
    // Periodic poll every 5 seconds
    const timer = setInterval(() => {
      refreshSystemStatus();
    }, 5000);
    return () => clearInterval(timer);
  }, [customEts2Path, customAtsPath]);

  const handleLaunchGame = async (gameId: string) => {
    setLoadingAction(`launch_${gameId}`);
    try {
      const msg = await invoke<string>("launch_game", { gameId });
      notify(msg || `Meluncurkan ${gameId.toUpperCase()} via Steam...`);
      setTimeout(() => refreshSystemStatus(), 2500);
    } catch (err: any) {
      notify(`Gagal meluncurkan game: ${err}`, true);
    } finally {
      setLoadingAction(null);
    }
  };

  const handleInstallPlugin = async (gameId: string) => {
    setLoadingAction(`install_${gameId}`);
    const customPath = gameId === "ets2" ? customEts2Path : customAtsPath;
    try {
      const msg = await invoke<string>("install_plugin", {
        gameId,
        customGamePath: customPath.trim() || null,
      });
      // Also sync current serverUrl & driverToken to Documents\<Game>\nismara_config.dat
      await invoke<PluginConfigPayload>("save_plugin_config", {
        driverToken: driverToken.trim(),
        apiBaseUrl: serverUrl.trim(),
      }).catch(() => {});
      notify(msg || `Plugin berhasil dipasang ke folder game!`);
      refreshSystemStatus();
    } catch (err: any) {
      notify(`Gagal memasang plugin: ${err}`, true);
    } finally {
      setLoadingAction(null);
    }
  };

  const handleSaveDirectories = () => {
    localStorage.setItem("nsm_ets2_path", customEts2Path.trim());
    localStorage.setItem("nsm_ats_path", customAtsPath.trim());
    notify("Pengaturan direktori game berhasil disimpan & diverifikasi!");
    refreshSystemStatus(customEts2Path.trim(), customAtsPath.trim());
  };

  const handleSavePluginConfig = async () => {
    const cleanToken = driverToken.trim();
    localStorage.setItem("nsm_driver_token", cleanToken);
    try {
      const saved = await invoke<PluginConfigPayload>("save_plugin_config", {
        driverToken: cleanToken,
        apiBaseUrl: PRODUCTION_API_URL,
      });
      setDriverToken(saved.driver_token);
      setResolvedConfigPath(saved.ets2_config_path);
      notify(
        "Token driver berhasil disinkronkan ke Documents\\<Game>\\nismara_config.dat!",
      );
    } catch (err: any) {
      notify(`Gagal menyimpan konfigurasi plugin: ${err}`, true);
    }
  };

  const ets2Game = systemStatus?.games.find((g) => g.id === "ets2");
  const atsGame = systemStatus?.games.find((g) => g.id === "ats");

  return (
    <div className="flex h-screen w-screen select-none overflow-hidden bg-[#0a0f18] text-slate-100 font-sans">
      {/* Toast Notification */}
      {statusToast && (
        <div
          className={`fixed top-4 right-6 z-50 flex items-center gap-2.5 px-4 py-2.5 rounded-lg shadow-2xl border text-sm backdrop-blur-md transition-all animate-in fade-in duration-200 ${
            statusToast.isError
              ? "bg-rose-950/90 border-rose-600/60 text-rose-200 shadow-rose-950/50"
              : "bg-slate-900/95 border-sky-500/60 text-sky-200 shadow-sky-950/50"
          }`}
        >
          {statusToast.isError ? (
            <AlertCircle className="w-4 h-4 text-rose-400 shrink-0" />
          ) : (
            <CheckCircle2 className="w-4 h-4 text-sky-400 shrink-0" />
          )}
          <span className="font-medium">{statusToast.message}</span>
        </div>
      )}

      {/* LEFT SIDEBAR NAVIGATION */}
      <aside className="w-20 shrink-0 bg-[#070b12] border-r border-slate-800/80 flex flex-col items-center py-5 justify-between z-20">
        <div className="flex flex-col items-center gap-7 w-full">
          {/* Official Nismara Logo */}
          <div
            className="flex flex-col items-center group cursor-pointer"
            onClick={() => setActiveTab("games")}
          >
            <div className="w-12 h-12 rounded-xl bg-slate-900/90 border border-slate-700/80 flex items-center justify-center p-1.5 shadow-lg shadow-sky-500/10 group-hover:scale-105 group-hover:border-sky-500/50 transition-all">
              <img
                src="/images/logo_nismara.png"
                alt="Nismara"
                className="w-full h-full object-contain filter drop-shadow"
                onError={(e) => {
                  (e.target as HTMLElement).style.display = "none";
                }}
              />
            </div>
            <span className="text-[10px] font-semibold text-sky-400/90 mt-1 uppercase tracking-widest">
              Beta
            </span>
          </div>

          {/* Navigation Items */}
          <nav className="flex flex-col items-center gap-3 w-full px-2">
            {/* Games */}
            <button
              onClick={() => setActiveTab("games")}
              className={`flex flex-col items-center justify-center w-full py-2.5 rounded-xl transition-all ${
                activeTab === "games"
                  ? "bg-sky-500/15 text-sky-400 border border-sky-500/30 shadow-inner shadow-sky-500/10"
                  : "text-slate-400 hover:text-slate-200 hover:bg-slate-800/50"
              }`}
            >
              <LayoutGrid className="w-5 h-5 mb-1" />
              <span className="text-[11px] font-medium">Games</span>
            </button>

            {/* News */}
            <button
              onClick={() => setActiveTab("news")}
              className={`flex flex-col items-center justify-center w-full py-2.5 rounded-xl transition-all ${
                activeTab === "news"
                  ? "bg-sky-500/15 text-sky-400 border border-sky-500/30 shadow-inner shadow-sky-500/10"
                  : "text-slate-400 hover:text-slate-200 hover:bg-slate-800/50"
              }`}
            >
              <Newspaper className="w-5 h-5 mb-1" />
              <span className="text-[11px] font-medium">News</span>
            </button>

            {/* Events */}
            <button
              onClick={() => setActiveTab("events")}
              className={`flex flex-col items-center justify-center w-full py-2.5 rounded-xl transition-all ${
                activeTab === "events"
                  ? "bg-sky-500/15 text-sky-400 border border-sky-500/30 shadow-inner shadow-sky-500/10"
                  : "text-slate-400 hover:text-slate-200 hover:bg-slate-800/50"
              }`}
            >
              <Trophy className="w-5 h-5 mb-1" />
              <span className="text-[11px] font-medium">Events</span>
            </button>

            {/* Settings */}
            <button
              onClick={() => setActiveTab("settings")}
              className={`flex flex-col items-center justify-center w-full py-2.5 rounded-xl transition-all ${
                activeTab === "settings"
                  ? "bg-sky-500/15 text-sky-400 border border-sky-500/30 shadow-inner shadow-sky-500/10"
                  : "text-slate-400 hover:text-slate-200 hover:bg-slate-800/50"
              }`}
            >
              <SettingsIcon className="w-5 h-5 mb-1" />
              <span className="text-[11px] font-medium">Settings</span>
            </button>
          </nav>
        </div>

        {/* Support at bottom */}
        <div className="w-full px-2 flex flex-col items-center gap-3">
          <a
            href="https://discord.gg/nismara"
            target="_blank"
            rel="noreferrer"
            className="flex flex-col items-center justify-center w-full py-2 rounded-xl text-slate-400 hover:text-sky-300 hover:bg-slate-800/50 transition-all"
            title="Nismara Discord & Support"
          >
            <Headphones className="w-5 h-5 mb-1" />
            <span className="text-[10px] font-medium">Support</span>
          </a>
        </div>
      </aside>

      {/* MAIN CONTENT AREA */}
      <div className="flex-1 flex flex-col h-full relative overflow-hidden bg-[#080d16]">
        {/* SERVER VERSION ALERT (GATEKEEPER) */}
        {serverVersionAlert && (
          <div className="bg-gradient-to-r from-rose-950 via-rose-900 to-rose-950 border-b border-rose-500/50 px-6 py-2.5 flex items-center justify-between text-xs text-rose-100 backdrop-blur-md z-40 shadow-lg shadow-rose-950/50">
            <div className="flex items-center gap-2.5">
              <ShieldAlert className="w-4 h-4 text-rose-400 shrink-0 animate-pulse" />
              <span>
                <strong>VERSI KLIEN TIDAK SESUAI:</strong> Server Nismara
                mewajibkan versi minimum{" "}
                <strong>v{serverVersionAlert.minVersion}</strong>. Akses
                telemetri & tracking in-game dibatasi hingga Anda memperbarui
                Nismara Companion.
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

        {/* VIEW: GAMES (LAUNCHER HERO SPLIT) */}
        {activeTab === "games" && (
          <div className="flex-1 flex w-full h-[calc(100vh-42px)] relative">
            {/* LEFT HALF: EURO TRUCK SIMULATOR 2 */}
            <div className="flex-1 relative flex flex-col justify-between p-8 border-r border-slate-800/60 overflow-hidden group">
              {/* Background Artwork */}
              <div
                className="absolute inset-0 bg-cover bg-no-repeat transition-transform duration-700 group-hover:scale-105"
                style={{
                  backgroundImage: `url('/images/ets2_truck_images.webp'), url('/launcher-bg.jpg')`,
                  backgroundPosition: "center center",
                }}
              />
              {/* Dark Gradient Overlays for contrast */}
              <div className="absolute inset-0 bg-gradient-to-t from-[#070b13] via-[#070b13]/55 to-[#070b13]/25 pointer-events-none" />
              <div className="absolute inset-0 bg-gradient-to-r from-[#070b13]/85 via-transparent to-black/35 pointer-events-none" />

              {/* Top Header Badge: Official ETS2 Logo */}
              <div className="relative z-10 flex items-center justify-between">
                <div className="flex items-center">
                  <img
                    src="/images/ets2-logo.webp"
                    alt="Euro Truck Simulator 2"
                    className="h-16 w-auto object-contain filter drop-shadow-2xl"
                  />
                </div>

                {ets2Game?.is_running && (
                  <div className="flex items-center gap-1.5 px-3 py-1 rounded-full bg-emerald-500/20 border border-emerald-500/40 text-emerald-300 text-xs font-semibold backdrop-blur-md">
                    <Radio className="w-3.5 h-3.5 animate-pulse text-emerald-400" />
                    <span>In-Game Active</span>
                  </div>
                )}
              </div>

              {/* Bottom Content Area */}
              <div className="relative z-10 flex flex-col gap-4 max-w-md">
                {/* Status Indicator */}
                <div className="flex items-center gap-2 text-sm">
                  {!systemStatus ? (
                    <div className="flex items-center gap-2 text-slate-400 font-medium animate-pulse">
                      <span className="w-2.5 h-2.5 rounded-full bg-slate-400" />
                      <span>Memeriksa direktori ETS2...</span>
                    </div>
                  ) : ets2Game?.is_running ? (
                    <div className="flex items-center gap-2 text-emerald-400 font-medium">
                      <span className="w-2.5 h-2.5 rounded-full bg-emerald-400 shadow-lg shadow-emerald-400/50" />
                      <span>Sedang Bermain Euro Truck Simulator 2</span>
                    </div>
                  ) : ets2Game?.is_installed ? (
                    <div className="flex items-center gap-2 text-slate-300 font-medium">
                      <span
                        className={`w-2.5 h-2.5 rounded-full ${
                          ets2Game.plugin_outdated
                            ? "bg-amber-400 animate-pulse shadow-lg shadow-amber-400/50"
                            : "bg-emerald-400"
                        }`}
                      />
                      <span>
                        {!ets2Game.plugin_installed
                          ? "Plugin NismaraTracker belum terpasang"
                          : ets2Game.plugin_outdated
                            ? "Update Plugin Tersedia"
                            : "Siap Bermain"}
                      </span>
                    </div>
                  ) : (
                    <div className="flex items-center gap-2 text-amber-400 font-medium">
                      <span className="w-2.5 h-2.5 rounded-full bg-amber-500" />
                      <span>
                        Folder ETS2 belum ditentukan atau disk terhambat
                      </span>
                    </div>
                  )}
                </div>

                {/* Action Buttons */}
                <div className="flex items-center gap-3 pt-1">
                  {!systemStatus ? (
                    <button
                      disabled
                      className="px-8 py-3.5 rounded-xl bg-slate-800/80 border border-slate-700 text-slate-400 font-semibold text-sm cursor-wait animate-pulse"
                    >
                      Memeriksa...
                    </button>
                  ) : ets2Game?.is_running ? (
                    <button
                      disabled
                      className="px-7 py-3.5 rounded-xl bg-emerald-600/30 border border-emerald-500/50 text-emerald-300 font-bold text-sm flex items-center gap-2 cursor-default"
                    >
                      <Radio className="w-4 h-4 animate-pulse" />
                      <span>ETS2 Sedang Berjalan</span>
                    </button>
                  ) : !ets2Game?.is_installed ? (
                    <button
                      onClick={() => setActiveTab("settings")}
                      className="px-7 py-3.5 rounded-xl bg-slate-800 hover:bg-slate-700 border border-slate-600 text-slate-200 font-semibold text-sm flex items-center gap-2 transition-all hover:scale-102"
                    >
                      <FolderOpen className="w-4 h-4 text-sky-400" />
                      <span>Atur Folder di Settings ⚙️</span>
                    </button>
                  ) : !ets2Game.plugin_installed ? (
                    <button
                      onClick={() => handleInstallPlugin("ets2")}
                      disabled={loadingAction === "install_ets2"}
                      className="px-7 py-3.5 rounded-xl bg-gradient-to-r from-amber-500 to-amber-600 hover:from-amber-400 hover:to-amber-500 text-slate-950 font-bold text-sm shadow-xl shadow-amber-500/25 flex items-center gap-2 transition-all hover:scale-102 active:scale-98"
                    >
                      <Download className="w-4 h-4" />
                      <span>
                        {loadingAction === "install_ets2"
                          ? "Memasang..."
                          : "Pasang Plugin In-Game"}
                      </span>
                    </button>
                  ) : (
                    <>
                      <button
                        onClick={() => handleLaunchGame("ets2")}
                        disabled={loadingAction === "launch_ets2"}
                        className="px-8 py-3.5 rounded-xl bg-gradient-to-r from-blue-600 to-sky-500 hover:from-blue-500 hover:to-sky-400 text-white font-bold text-sm shadow-xl shadow-sky-500/30 flex items-center gap-2 transition-all hover:scale-102 active:scale-98 cursor-pointer"
                      >
                        <span>
                          {loadingAction === "launch_ets2"
                            ? "Membuka..."
                            : "Open ETS2"}
                        </span>
                        <span className="text-lg">➜</span>
                      </button>
                      {ets2Game.plugin_outdated && (
                        <button
                          onClick={() => handleInstallPlugin("ets2")}
                          disabled={loadingAction === "install_ets2"}
                          className="px-5 py-3.5 rounded-xl bg-gradient-to-r from-amber-500 to-amber-600 hover:from-amber-400 hover:to-amber-500 text-slate-950 font-bold text-sm shadow-xl shadow-amber-500/25 flex items-center gap-2 transition-all hover:scale-102 active:scale-98 cursor-pointer"
                          title="Perbarui file NismaraTracker.dll ke versi terbaru yang dibundel"
                        >
                          <ArrowUpCircle className="w-4 h-4" />
                          <span>
                            {loadingAction === "install_ets2"
                              ? "Memperbarui..."
                              : "Perbarui Plugin"}
                          </span>
                        </button>
                      )}
                    </>
                  )}

                  {ets2Game?.is_installed && ets2Game.plugin_installed && (
                    <button
                      onClick={() => handleInstallPlugin("ets2")}
                      title="Sinkronkan / Verifikasi ulang file NismaraTracker.dll"
                      className="p-3.5 rounded-xl bg-slate-900/80 hover:bg-slate-800 border border-slate-700/80 text-slate-300 hover:text-white transition-all backdrop-blur-md cursor-pointer"
                    >
                      <FolderSync className="w-4 h-4" />
                    </button>
                  )}
                </div>
              </div>
            </div>

            {/* RIGHT HALF: AMERICAN TRUCK SIMULATOR */}
            <div className="flex-1 relative flex flex-col justify-between p-8 overflow-hidden group">
              {/* Background Artwork */}
              <div
                className="absolute inset-0 bg-cover bg-no-repeat transition-transform duration-700 group-hover:scale-105"
                style={{
                  backgroundImage: `url('/images/ats_truck_images.webp'), url('/launcher-bg.jpg')`,
                  backgroundPosition: "center center",
                }}
              />
              {/* Dark Gradient Overlays */}
              <div className="absolute inset-0 bg-gradient-to-t from-[#070b13] via-[#070b13]/55 to-[#070b13]/25 pointer-events-none" />
              <div className="absolute inset-0 bg-gradient-to-l from-[#070b13]/85 via-transparent to-black/35 pointer-events-none" />

              {/* Top Header Badge: Official ATS Logo */}
              <div className="relative z-10 flex items-center justify-between">
                <div className="flex items-center">
                  <img
                    src="/images/ats-logo.webp"
                    alt="American Truck Simulator"
                    className="h-16 w-auto object-contain filter drop-shadow-2xl"
                  />
                </div>

                {atsGame?.is_running && (
                  <div className="flex items-center gap-1.5 px-3 py-1 rounded-full bg-emerald-500/20 border border-emerald-500/40 text-emerald-300 text-xs font-semibold backdrop-blur-md">
                    <Radio className="w-3.5 h-3.5 animate-pulse text-emerald-400" />
                    <span>In-Game Active</span>
                  </div>
                )}
              </div>

              {/* Bottom Content Area */}
              <div className="relative z-10 flex flex-col gap-4 max-w-md">
                {/* Status Indicator */}
                <div className="flex items-center gap-2 text-sm">
                  {!systemStatus ? (
                    <div className="flex items-center gap-2 text-slate-400 font-medium animate-pulse">
                      <span className="w-2.5 h-2.5 rounded-full bg-slate-400" />
                      <span>Memeriksa direktori ATS...</span>
                    </div>
                  ) : atsGame?.is_running ? (
                    <div className="flex items-center gap-2 text-emerald-400 font-medium">
                      <span className="w-2.5 h-2.5 rounded-full bg-emerald-400 shadow-lg shadow-emerald-400/50" />
                      <span>Sedang Bermain American Truck Simulator</span>
                    </div>
                  ) : atsGame?.is_installed ? (
                    <div className="flex items-center gap-2 text-slate-300 font-medium">
                      <span
                        className={`w-2.5 h-2.5 rounded-full ${
                          atsGame.plugin_outdated
                            ? "bg-amber-400 animate-pulse shadow-lg shadow-amber-400/50"
                            : "bg-emerald-400"
                        }`}
                      />
                      <span>
                        {!atsGame.plugin_installed
                          ? "Plugin NismaraTracker belum terpasang"
                          : atsGame.plugin_outdated
                            ? "Update Plugin Tersedia"
                            : "Ready to Play"}
                      </span>
                    </div>
                  ) : (
                    <div className="flex items-center gap-2 text-slate-400 font-medium">
                      <span className="w-2.5 h-2.5 rounded-full bg-slate-500" />
                      <span>
                        Game belum terinstal atau tentukan folder di Settings
                      </span>
                    </div>
                  )}
                </div>

                {/* Action Buttons */}
                <div className="flex items-center gap-3 pt-1">
                  {!systemStatus ? (
                    <button
                      disabled
                      className="px-8 py-3.5 rounded-xl bg-slate-800/80 border border-slate-700 text-slate-400 font-semibold text-sm cursor-wait animate-pulse"
                    >
                      Memeriksa...
                    </button>
                  ) : atsGame?.is_running ? (
                    <button
                      disabled
                      className="px-7 py-3.5 rounded-xl bg-emerald-600/30 border border-emerald-500/50 text-emerald-300 font-bold text-sm flex items-center gap-2 cursor-default"
                    >
                      <Radio className="w-4 h-4 animate-pulse" />
                      <span>ATS Sedang Berjalan</span>
                    </button>
                  ) : atsGame?.is_installed ? (
                    !atsGame.plugin_installed ? (
                      <button
                        onClick={() => handleInstallPlugin("ats")}
                        disabled={loadingAction === "install_ats"}
                        className="px-7 py-3.5 rounded-xl bg-gradient-to-r from-amber-500 to-amber-600 hover:from-amber-400 text-slate-950 font-bold text-sm flex items-center gap-2 transition-all hover:scale-102"
                      >
                        <Download className="w-4 h-4" />
                        <span>Pasang Plugin</span>
                      </button>
                    ) : (
                      <>
                        <button
                          onClick={() => handleLaunchGame("ats")}
                          disabled={loadingAction === "launch_ats"}
                          className="px-8 py-3.5 rounded-xl bg-gradient-to-r from-red-600 to-rose-500 hover:from-red-500 hover:to-rose-400 text-white font-bold text-sm shadow-xl shadow-red-500/25 flex items-center gap-2 transition-all hover:scale-102 cursor-pointer"
                        >
                          <span>Open ATS</span>
                          <span className="text-lg">➜</span>
                        </button>
                        {atsGame.plugin_outdated && (
                          <button
                            onClick={() => handleInstallPlugin("ats")}
                            disabled={loadingAction === "install_ats"}
                            className="px-5 py-3.5 rounded-xl bg-gradient-to-r from-amber-500 to-amber-600 hover:from-amber-400 hover:to-amber-500 text-slate-950 font-bold text-sm shadow-xl shadow-amber-500/25 flex items-center gap-2 transition-all hover:scale-102 active:scale-98 cursor-pointer"
                            title="Perbarui file NismaraTracker.dll ke versi terbaru yang dibundel"
                          >
                            <ArrowUpCircle className="w-4 h-4" />
                            <span>
                              {loadingAction === "install_ats"
                                ? "Memperbarui..."
                                : "Perbarui Plugin"}
                            </span>
                          </button>
                        )}
                        <button
                          onClick={() => handleInstallPlugin("ats")}
                          title="Sinkronkan / Verifikasi ulang file NismaraTracker.dll"
                          className="p-3.5 rounded-xl bg-slate-900/80 hover:bg-slate-800 border border-slate-700/80 text-slate-300 hover:text-white transition-all backdrop-blur-md cursor-pointer"
                        >
                          <FolderSync className="w-4 h-4" />
                        </button>
                      </>
                    )
                  ) : (
                    <button
                      onClick={() => setActiveTab("settings")}
                      className="px-7 py-3.5 rounded-xl bg-slate-800 hover:bg-slate-700 border border-slate-700 text-slate-300 font-medium text-sm flex items-center gap-2 backdrop-blur-md transition-all hover:scale-102"
                    >
                      <FolderOpen className="w-4 h-4 text-sky-400" />
                      <span>Atur Folder di Settings ⚙️</span>
                    </button>
                  )}
                </div>
              </div>
            </div>
          </div>
        )}

        {/* VIEW: NEWS */}
        {activeTab === "news" && (
          <div className="flex-1 p-8 overflow-y-auto max-w-5xl mx-auto w-full">
            <div className="mb-6 flex flex-col md:flex-row md:items-center justify-between gap-4">
              <div>
                <h2 className="text-2xl font-bold text-white flex items-center gap-2">
                  <Newspaper className="w-6 h-6 text-sky-400" />
                  <span>Nismara News & Announcements</span>
                </h2>
                <p className="text-sm text-slate-400 mt-1">
                  Kabar resmi, log pembaruan, dan pengumuman komunitas Nismara Group langsung dari Sanity CMS.
                </p>
              </div>

              {/* Refresh Button */}
              <button
                onClick={() => fetchSanityArticles(true)}
                disabled={newsLoading}
                className="self-start md:self-auto px-4 py-2 rounded-xl bg-slate-800/80 hover:bg-slate-700 border border-slate-700/80 text-slate-200 text-xs font-semibold flex items-center gap-2 transition-all cursor-pointer disabled:opacity-50"
              >
                <RefreshCw
                  className={`w-3.5 h-3.5 ${
                    newsLoading ? "animate-spin text-sky-400" : ""
                  }`}
                />
                <span>{newsLoading ? "Memuat..." : "Segarkan Berita"}</span>
              </button>
            </div>

            {/* Category Filter Pills */}
            <div className="flex items-center gap-2 overflow-x-auto pb-3 mb-6 scrollbar-none">
              {availableCategories.map((cat) => (
                <button
                  key={cat}
                  onClick={() => setSelectedCategory(cat)}
                  className={`px-3.5 py-1.5 rounded-full text-xs font-semibold transition-all cursor-pointer whitespace-nowrap ${
                    selectedCategory === cat
                      ? "bg-sky-500 text-white shadow-md shadow-sky-500/20"
                      : "bg-slate-800/60 hover:bg-slate-800 text-slate-400 hover:text-slate-200 border border-slate-700/50"
                  }`}
                >
                  {cat === "ALL" ? "Semua Kategori" : cat}
                </button>
              ))}
            </div>

            {/* Error banner if any */}
            {newsError && (
              <div className="p-4 rounded-xl bg-rose-500/10 border border-rose-500/30 text-rose-300 text-xs flex items-center justify-between mb-6">
                <div className="flex items-center gap-2">
                  <AlertCircle className="w-4 h-4 shrink-0 text-rose-400" />
                  <span>{newsError}</span>
                </div>
                <button
                  onClick={() => fetchSanityArticles(true)}
                  className="px-3 py-1 bg-rose-500/20 hover:bg-rose-500/30 text-rose-200 font-bold rounded-lg cursor-pointer transition-all"
                >
                  Coba Lagi
                </button>
              </div>
            )}

            {/* Loading skeletons */}
            {newsLoading && articles.length === 0 ? (
              <div className="grid grid-cols-1 md:grid-cols-2 gap-5">
                {[1, 2, 3, 4].map((i) => (
                  <div
                    key={i}
                    className="p-5 rounded-2xl bg-slate-900/60 border border-slate-800 animate-pulse flex flex-col gap-3"
                  >
                    <div className="w-full h-44 rounded-xl bg-slate-800/70" />
                    <div className="h-4 w-28 rounded bg-slate-800" />
                    <div className="h-6 w-3/4 rounded bg-slate-800" />
                    <div className="h-12 w-full rounded bg-slate-800/50" />
                  </div>
                ))}
              </div>
            ) : filteredArticles.length === 0 ? (
              <div className="p-12 text-center rounded-2xl bg-slate-900/40 border border-slate-800 text-slate-400">
                <Newspaper className="w-10 h-10 mx-auto mb-3 opacity-40 text-slate-500" />
                <p className="text-sm">Tidak ada artikel untuk kategori ini.</p>
              </div>
            ) : (
              <div className="grid grid-cols-1 md:grid-cols-2 gap-5">
                {filteredArticles.map((article) => {
                  const imgUrl = getSanityImageUrl(
                    article.mainImage?.asset?._ref,
                  );
                  const excerpt =
                    article.seoDescription ||
                    extractBlockText(article.body).slice(0, 160) + "...";

                  return (
                    <article
                      key={article._id}
                      className="group rounded-2xl bg-slate-900/70 border border-slate-800/90 hover:border-slate-700/80 hover:bg-slate-900/90 transition-all flex flex-col overflow-hidden backdrop-blur-sm shadow-lg shadow-black/20"
                    >
                      {/* Cover Image if available */}
                      {imgUrl ? (
                        <div className="relative w-full h-48 bg-slate-950 overflow-hidden">
                          <img
                            src={imgUrl}
                            alt={article.title}
                            className="w-full h-full object-cover group-hover:scale-105 transition-transform duration-300"
                            loading="lazy"
                          />
                          <div className="absolute inset-0 bg-gradient-to-t from-slate-900 via-transparent to-black/20" />
                          <div className="absolute top-3 left-3 flex flex-wrap gap-1.5">
                            {article.categories?.map((cat, idx) => (
                              <span
                                key={idx}
                                className={`px-2.5 py-0.5 rounded-md text-[10px] font-bold uppercase tracking-wider border backdrop-blur-md ${getCategoryColor(
                                  cat.title,
                                )}`}
                              >
                                {cat.title}
                              </span>
                            ))}
                          </div>
                        </div>
                      ) : (
                        <div className="p-4 pb-0 flex items-center gap-1.5">
                          {article.categories?.map((cat, idx) => (
                            <span
                              key={idx}
                              className={`px-2.5 py-0.5 rounded-md text-[10px] font-bold uppercase tracking-wider border ${getCategoryColor(
                                cat.title,
                              )}`}
                            >
                              {cat.title}
                            </span>
                          ))}
                        </div>
                      )}

                      <div className="p-5 flex-1 flex flex-col justify-between gap-4">
                        <div className="space-y-2">
                          <div className="flex items-center justify-between text-[11px] text-slate-400">
                            <span className="flex items-center gap-1">
                              <Calendar className="w-3.5 h-3.5 text-slate-500" />
                              <span>{formatNewsDate(article.publishedAt)}</span>
                            </span>
                            {article.author?.name && (
                              <span className="flex items-center gap-1 text-slate-400">
                                <User className="w-3.5 h-3.5 text-slate-500" />
                                <span>{article.author.name}</span>
                              </span>
                            )}
                          </div>

                          <h3 className="text-base font-bold text-white group-hover:text-sky-300 transition-colors line-clamp-2">
                            {article.title}
                          </h3>

                          <p className="text-xs text-slate-300/90 leading-relaxed line-clamp-3">
                            {excerpt}
                          </p>
                        </div>

                        <div className="pt-3 border-t border-slate-800/80 flex items-center justify-between gap-2">
                          <button
                            onClick={() => setReadingArticle(article)}
                            className="px-3 py-1.5 rounded-lg bg-sky-500/15 hover:bg-sky-500/25 border border-sky-500/30 text-sky-300 text-xs font-semibold flex items-center gap-1.5 transition-all cursor-pointer"
                          >
                            <BookOpen className="w-3.5 h-3.5" />
                            <span>Baca Rincian</span>
                          </button>

                          {article.slug?.current && (
                            <button
                              onClick={() => {
                                const url = `https://nismara.web.id/articles/${article.slug?.current}`;
                                openUrl(url).catch(() =>
                                  window.open(url, "_blank"),
                                );
                              }}
                              className="px-3 py-1.5 rounded-lg hover:bg-slate-800 text-slate-400 hover:text-slate-200 text-xs font-medium flex items-center gap-1 transition-all cursor-pointer"
                              title="Buka di Website Resmi"
                            >
                              <span>Buka di Web</span>
                              <ExternalLink className="w-3.5 h-3.5" />
                            </button>
                          )}
                        </div>
                      </div>
                    </article>
                  );
                })}
              </div>
            )}
          </div>
        )}

        {/* VIEW: EVENTS */}
        {activeTab === "events" && (
          <div className="flex-1 p-8 overflow-y-auto max-w-5xl mx-auto w-full">
            <div className="mb-6 flex flex-col md:flex-row md:items-center justify-between gap-4">
              <div>
                <h2 className="text-2xl font-bold text-white flex items-center gap-2">
                  <Trophy className="w-6 h-6 text-amber-400" />
                  <span>VTC Events, Contracts & Quests</span>
                </h2>
                <p className="text-sm text-slate-400 mt-1">
                  Special Contracts, Giveaway berhadiah, pembagian kupon, dan
                  NC Multiplier boost resmi Nismara.
                </p>
              </div>

              {/* Refresh Button */}
              <button
                onClick={fetchTrackerEvents}
                disabled={eventsLoading}
                className="self-start md:self-auto px-4 py-2 rounded-xl bg-slate-800/80 hover:bg-slate-700 border border-slate-700/80 text-slate-200 text-xs font-semibold flex items-center gap-2 transition-all cursor-pointer disabled:opacity-50"
              >
                <RefreshCw
                  className={`w-3.5 h-3.5 ${
                    eventsLoading ? "animate-spin text-amber-400" : ""
                  }`}
                />
                <span>{eventsLoading ? "Memuat..." : "Segarkan Event"}</span>
              </button>
            </div>

            {/* Event Type Filter Pills */}
            <div className="flex items-center gap-2 overflow-x-auto pb-3 mb-6 scrollbar-none">
              {[
                { key: "ALL", label: "Semua Event" },
                { key: "CONTRACT", label: "Special Contracts" },
                { key: "GIVEAWAY", label: "Giveaway Quests" },
                { key: "COUPON", label: "Kupon Hadiah" },
                { key: "NC_EVENT", label: "NC Boost" },
              ].map((tab) => (
                <button
                  key={tab.key}
                  onClick={() => setSelectedEventType(tab.key)}
                  className={`px-3.5 py-1.5 rounded-full text-xs font-semibold transition-all cursor-pointer whitespace-nowrap ${
                    selectedEventType === tab.key
                      ? "bg-amber-500 text-slate-950 font-bold shadow-md shadow-amber-500/20"
                      : "bg-slate-800/60 hover:bg-slate-800 text-slate-400 hover:text-slate-200 border border-slate-700/50"
                  }`}
                >
                  {tab.label}
                </button>
              ))}
            </div>

            {/* Error banner if any */}
            {eventsError && (
              <div className="p-4 rounded-xl bg-rose-500/10 border border-rose-500/30 text-rose-300 text-xs flex items-center justify-between mb-6">
                <div className="flex items-center gap-2">
                  <AlertCircle className="w-4 h-4 shrink-0 text-rose-400" />
                  <span>{eventsError}</span>
                </div>
                <button
                  onClick={fetchTrackerEvents}
                  className="px-3 py-1 bg-rose-500/20 hover:bg-rose-500/30 text-rose-200 font-bold rounded-lg cursor-pointer transition-all"
                >
                  Coba Lagi
                </button>
              </div>
            )}

            {/* Loading skeletons */}
            {eventsLoading && eventsFeed.length === 0 ? (
              <div className="grid grid-cols-1 md:grid-cols-2 gap-5">
                {[1, 2, 3, 4].map((i) => (
                  <div
                    key={i}
                    className="p-5 rounded-2xl bg-slate-900/60 border border-slate-800 animate-pulse flex flex-col gap-3"
                  >
                    <div className="w-full h-40 rounded-xl bg-slate-800/70" />
                    <div className="h-4 w-28 rounded bg-slate-800" />
                    <div className="h-6 w-3/4 rounded bg-slate-800" />
                    <div className="h-10 w-full rounded bg-slate-800/50" />
                  </div>
                ))}
              </div>
            ) : filteredEvents.length === 0 ? (
              <div className="p-12 text-center rounded-2xl bg-slate-900/40 border border-slate-800 text-slate-400">
                <Trophy className="w-10 h-10 mx-auto mb-3 opacity-40 text-slate-500" />
                <p className="text-sm">Tidak ada event dalam kategori ini.</p>
              </div>
            ) : (
              <div className="grid grid-cols-1 md:grid-cols-2 gap-5">
                {filteredEvents.map((item) => {
                  return (
                    <div
                      key={item.id}
                      className="group rounded-2xl bg-slate-900/70 border border-slate-800/90 hover:border-slate-700/80 hover:bg-slate-900/90 transition-all flex flex-col overflow-hidden backdrop-blur-sm shadow-lg shadow-black/20"
                    >
                      {/* Banner Image if available */}
                      {item.imageUrl ? (
                        <div className="relative w-full h-44 bg-slate-950 overflow-hidden">
                          <img
                            src={item.imageUrl}
                            alt={item.title}
                            className="w-full h-full object-cover group-hover:scale-105 transition-transform duration-300"
                            loading="lazy"
                          />
                          <div className="absolute inset-0 bg-gradient-to-t from-slate-900 via-transparent to-black/20" />
                          <div className="absolute top-3 left-3 flex items-center gap-1.5">
                            <span
                              className={`px-2.5 py-0.5 rounded-md text-[10px] font-bold uppercase tracking-wider border backdrop-blur-md ${item.badgeColor}`}
                            >
                              {item.typeLabel}
                            </span>
                            {item.isActive ? (
                              <span className="px-2 py-0.5 rounded-md text-[10px] font-bold bg-emerald-500/80 text-white backdrop-blur-md flex items-center gap-1 shadow-sm">
                                <span className="w-1.5 h-1.5 rounded-full bg-white animate-pulse" />
                                AKTIF
                              </span>
                            ) : item.isScheduled ? (
                              <span className="px-2 py-0.5 rounded-md text-[10px] font-bold bg-sky-500/80 text-white backdrop-blur-md">
                                TERJADWAL
                              </span>
                            ) : (
                              <span className="px-2 py-0.5 rounded-md text-[10px] font-bold bg-slate-700/80 text-slate-300 backdrop-blur-md">
                                SELESAI
                              </span>
                            )}
                          </div>
                        </div>
                      ) : (
                        <div className="p-5 pb-0 flex items-center justify-between">
                          <span
                            className={`px-2.5 py-0.5 rounded-md text-[10px] font-bold uppercase tracking-wider border ${item.badgeColor}`}
                          >
                            {item.typeLabel}
                          </span>
                          {item.isActive ? (
                            <span className="px-2 py-0.5 rounded-md text-[10px] font-bold bg-emerald-500/20 text-emerald-400 border border-emerald-500/30 flex items-center gap-1">
                              <span className="w-1.5 h-1.5 rounded-full bg-emerald-400 animate-pulse" />
                              AKTIF
                            </span>
                          ) : (
                            <span className="px-2 py-0.5 rounded-md text-[10px] font-bold bg-slate-800 text-slate-400">
                              SELESAI
                            </span>
                          )}
                        </div>
                      )}

                      <div className="p-5 flex-1 flex flex-col justify-between gap-4">
                        <div className="space-y-2.5">
                          {/* Date Range */}
                          <div className="flex items-center gap-1 text-[11px] text-slate-400">
                            <Calendar className="w-3.5 h-3.5 text-slate-500" />
                            <span>
                              {formatNewsDate(item.startDate || "")}
                              {item.endDate
                                ? ` — ${formatNewsDate(item.endDate)}`
                                : ""}
                            </span>
                          </div>

                          {/* Title */}
                          <h3 className="text-base font-bold text-white group-hover:text-amber-300 transition-colors">
                            {item.title}
                          </h3>

                          {/* Description or Excerpt */}
                          {item.description && (
                            <p className="text-xs text-slate-300/90 leading-relaxed line-clamp-2">
                              {item.description}
                            </p>
                          )}

                          {/* Specific stats per event type */}
                          {item.type === "CONTRACT" && item.stats && (
                            <div className="p-2.5 rounded-xl bg-slate-950/60 border border-slate-800 text-xs text-slate-300 flex items-center justify-between">
                              <span>
                                Perusahaan:{" "}
                                <strong className="text-white">
                                  {item.company}
                                </strong>
                              </span>
                              <span className="text-sky-400 font-semibold">
                                {item.stats?.completedContracts ?? 0} Selesai
                              </span>
                            </div>
                          )}

                          {item.type === "COUPON" && item.couponCode && (
                            <div className="p-2.5 rounded-xl bg-amber-500/10 border border-amber-500/30 flex items-center justify-between">
                              <div className="text-xs">
                                <span className="text-slate-400 block text-[10px]">
                                  KODE KUPON:
                                </span>
                                <span className="font-mono font-bold text-amber-300 text-sm">
                                  {item.couponCode}
                                </span>
                              </div>
                              <button
                                onClick={() =>
                                  handleCopyCouponCode(item.couponCode || "")
                                }
                                className="px-2.5 py-1 rounded-lg bg-amber-500/20 hover:bg-amber-500/30 text-amber-200 text-xs font-semibold flex items-center gap-1 transition-all cursor-pointer"
                              >
                                {copiedCouponCode === item.couponCode ? (
                                  <>
                                    <Check className="w-3 h-3 text-emerald-400" />
                                    <span>Tersalin</span>
                                  </>
                                ) : (
                                  <>
                                    <Copy className="w-3 h-3" />
                                    <span>Salin</span>
                                  </>
                                )}
                              </button>
                            </div>
                          )}

                          {item.type === "GIVEAWAY" && (
                            <div className="p-2.5 rounded-xl bg-purple-500/10 border border-purple-500/30 text-xs flex items-center justify-between">
                              <span className="text-purple-300">
                                🎯{" "}
                                <strong>
                                  {item.quests?.length || 0} Quest Tersedia
                                </strong>
                              </span>
                              <span className="text-slate-400">
                                {item.ticketPriceNC
                                  ? `Tiket: ${item.ticketPriceNC.toLocaleString()} NC`
                                  : "Quest Gratis"}
                              </span>
                            </div>
                          )}

                          {item.type === "NC_EVENT" && (
                            <div className="p-2.5 rounded-xl bg-emerald-500/10 border border-emerald-500/30 text-xs flex items-center justify-between">
                              <span className="text-emerald-300 font-bold text-sm">
                                ⚡ {item.multiplierText}
                              </span>
                              <span className="text-slate-400">
                                {item.participantsCount} Driver Berpartisipasi
                              </span>
                            </div>
                          )}
                        </div>

                        {/* Bottom Actions */}
                        <div className="pt-3 border-t border-slate-800/80 flex items-center justify-between gap-2">
                          <button
                            onClick={() => setSelectedEventDetail(item)}
                            className="px-3 py-1.5 rounded-lg bg-slate-800 hover:bg-slate-700/80 text-slate-200 text-xs font-semibold flex items-center gap-1.5 transition-all cursor-pointer"
                          >
                            <Trophy className="w-3.5 h-3.5 text-amber-400" />
                            <span>Rincian Event</span>
                          </button>

                          {item.type === "CONTRACT" && (
                            <span className="text-[11px] text-slate-400">
                              Total:{" "}
                              <strong className="text-white">
                                {item.stats?.totalNCEarned?.toLocaleString()} NC
                              </strong>
                            </span>
                          )}

                          {item.type === "COUPON" &&
                            item.claimsCount !== undefined && (
                              <span className="text-[11px] text-slate-400">
                                {item.claimsCount} Driver Klaim
                              </span>
                            )}
                        </div>
                      </div>
                    </div>
                  );
                })}
              </div>
            )}
          </div>
        )}

        {/* VIEW: SETTINGS */}
        {activeTab === "settings" && (
          <div className="flex-1 p-8 overflow-y-auto max-w-4xl mx-auto w-full">
            <div className="mb-6">
              <h2 className="text-2xl font-bold text-white flex items-center gap-2">
                <SettingsIcon className="w-6 h-6 text-sky-400" />
                <span>Pengaturan Nismara Companion</span>
              </h2>
              <p className="text-sm text-slate-400 mt-1">
                Konfigurasi folder instalasi game manual, endpoint tracker, dan
                token driver.
              </p>
            </div>

            <div className="space-y-5">
              {/* Gatekeeper Card */}
              <div className="p-5 rounded-2xl bg-slate-900/80 border border-emerald-500/30 flex items-center justify-between">
                <div className="flex items-center gap-3.5">
                  <div className="w-10 h-10 rounded-xl bg-emerald-500/15 border border-emerald-500/30 flex items-center justify-center text-emerald-400">
                    <ShieldCheck className="w-5 h-5" />
                  </div>
                  <div>
                    <h4 className="text-sm font-bold text-slate-100">
                      Win32 Mutex Gatekeeper Protocol
                    </h4>
                    <p className="text-xs text-emerald-400/90 font-mono mt-0.5">
                      Global\NismaraCompanionRunning (Active & Holding Handle)
                    </p>
                  </div>
                </div>
                <span className="px-3 py-1 rounded-full bg-emerald-500/20 border border-emerald-500/40 text-emerald-300 text-xs font-bold">
                  PROTECTED
                </span>
              </div>

              {/* MANUAL GAME DIRECTORIES (SOLVES DISK HANG ISSUES) */}
              <div className="p-6 rounded-2xl bg-slate-900/80 border border-sky-500/40 space-y-4">
                <div className="flex items-center justify-between">
                  <div className="flex items-center gap-2.5">
                    <FolderOpen className="w-5 h-5 text-sky-400" />
                    <h3 className="text-sm font-bold text-slate-100 uppercase tracking-wider">
                      Direktori Game Manual (Anti Disk Hang)
                    </h3>
                  </div>
                  <button
                    onClick={handleSaveDirectories}
                    className="px-4 py-1.5 rounded-xl bg-sky-500 hover:bg-sky-400 text-slate-950 font-bold text-xs flex items-center gap-1.5 shadow-lg shadow-sky-500/20 transition-all cursor-pointer"
                  >
                    <Save className="w-3.5 h-3.5" />
                    <span>Simpan & Terapkan</span>
                  </button>
                </div>
                <p className="text-xs text-slate-400 leading-relaxed">
                  Tentukan lokasi folder game secara langsung di bawah ini.
                  Pengaturan manual ini mengabaikan scanning disk lain yang
                  mungkin sedang error/lambat sehingga game langsung terdeteksi
                  instan.
                </p>

                {/* ETS2 Manual Path */}
                <div>
                  <div className="flex items-center justify-between mb-1.5">
                    <label className="text-xs font-medium text-slate-300">
                      Folder Euro Truck Simulator 2
                    </label>
                    <span
                      className={`text-[11px] font-semibold ${ets2Game?.is_installed ? "text-emerald-400" : "text-amber-400"}`}
                    >
                      {ets2Game?.is_installed
                        ? `✓ Terdeteksi (${ets2Game.game_path})`
                        : "Belum Ditemukan"}
                    </span>
                  </div>
                  <input
                    type="text"
                    value={customEts2Path}
                    onChange={(e) => setCustomEts2Path(e.target.value)}
                    placeholder={
                      ets2Game?.game_path ||
                      "Kosongkan untuk deteksi otomatis via Steam libraryfolders.vdf"
                    }
                    className="w-full px-4 py-2.5 rounded-xl bg-slate-950/80 border border-slate-700 text-slate-100 text-xs focus:outline-none focus:border-sky-500 font-mono"
                  />
                  <div className="flex items-center justify-between mt-1">
                    <span className="text-[11px] text-slate-500">
                      Kosongkan agar otomatis mengikuti library Steam Anda, atau isi manual jika kustom.
                    </span>
                    <button
                      type="button"
                      onClick={() => {
                        setCustomEts2Path("");
                        localStorage.removeItem("nsm_ets2_path");
                        refreshSystemStatus("", customAtsPath);
                      }}
                      className="text-[11px] text-sky-400 hover:underline cursor-pointer"
                    >
                      Gunakan Deteksi Otomatis Steam
                    </button>
                  </div>
                </div>

                {/* ATS Manual Path */}
                <div className="pt-2 border-t border-slate-800">
                  <div className="flex items-center justify-between mb-1.5">
                    <label className="text-xs font-medium text-slate-300">
                      Folder American Truck Simulator
                    </label>
                    <span
                      className={`text-[11px] font-semibold ${atsGame?.is_installed ? "text-emerald-400" : "text-amber-400"}`}
                    >
                      {atsGame?.is_installed
                        ? `✓ Terdeteksi (${atsGame.game_path})`
                        : "Belum Ditemukan"}
                    </span>
                  </div>
                  <input
                    type="text"
                    value={customAtsPath}
                    onChange={(e) => setCustomAtsPath(e.target.value)}
                    placeholder={
                      atsGame?.game_path ||
                      "Kosongkan untuk deteksi otomatis via Steam libraryfolders.vdf"
                    }
                    className="w-full px-4 py-2.5 rounded-xl bg-slate-950/80 border border-slate-700 text-slate-100 text-xs focus:outline-none focus:border-sky-500 font-mono"
                  />
                  <div className="flex items-center justify-between mt-1">
                    <span className="text-[11px] text-slate-500">
                      Kosongkan agar otomatis mengikuti library Steam Anda, atau isi manual jika kustom.
                    </span>
                    <button
                      type="button"
                      onClick={() => {
                        setCustomAtsPath("");
                        localStorage.removeItem("nsm_ats_path");
                        refreshSystemStatus(customEts2Path, "");
                      }}
                      className="text-[11px] text-sky-400 hover:underline cursor-pointer"
                    >
                      Gunakan Deteksi Otomatis Steam
                    </button>
                  </div>
                </div>
              </div>

              {/* Endpoint & Token Card */}
              <div className="p-6 rounded-2xl bg-slate-900/80 border border-slate-800 space-y-4">
                <div className="flex items-center justify-between">
                  <div>
                    <h3 className="text-sm font-bold text-slate-200 uppercase tracking-wider">
                      Koneksi Server 24/7 & Token Driver
                    </h3>
                    {resolvedConfigPath && (
                      <p className="text-[11px] text-slate-400 font-mono mt-0.5">
                        Lokasi Config: {resolvedConfigPath}
                      </p>
                    )}
                  </div>
                  <button
                    onClick={handleSavePluginConfig}
                    className="px-4 py-1.5 rounded-xl bg-emerald-500 hover:bg-emerald-400 text-slate-950 font-bold text-xs flex items-center gap-1.5 shadow-lg shadow-emerald-500/20 transition-all cursor-pointer"
                  >
                    <Save className="w-3.5 h-3.5" />
                    <span>Simpan Token ke Game Config</span>
                  </button>
                </div>

                {/* Secured Production Cloud Indicator (IP Hidden from User) */}
                <div className="flex items-center justify-between p-3.5 rounded-xl bg-slate-950/70 border border-slate-800">
                  <div className="flex items-center gap-3">
                    <div className="w-2.5 h-2.5 rounded-full bg-emerald-400 shadow-[0_0_8px_rgba(52,211,153,0.6)] animate-pulse" />
                    <div>
                      <span className="text-xs font-semibold text-slate-200 block">
                        Nismara Official Cloud Network
                      </span>
                      <span className="text-[11px] text-slate-400">
                        Dedicated 24/7 high-speed telemetry server
                      </span>
                    </div>
                  </div>
                  <span className="text-[11px] font-mono text-emerald-400 bg-emerald-500/10 border border-emerald-500/20 px-2.5 py-1 rounded-lg">
                    Online (100% Secure)
                  </span>
                </div>

                <div>
                  <label className="block text-xs font-medium text-slate-300 mb-1.5">
                    Driver Tracker Token
                  </label>
                  <input
                    type="text"
                    value={driverToken}
                    onChange={(e) => {
                      setDriverToken(e.target.value);
                      localStorage.setItem("nsm_driver_token", e.target.value);
                    }}
                    placeholder="Masukkan token Anda dari Dashboard Website (NSM-XXXXXX-...)"
                    className="w-full px-4 py-2.5 rounded-xl bg-slate-950/80 border border-slate-700 text-slate-100 text-sm focus:outline-none focus:border-sky-500 font-mono"
                  />
                </div>
              </div>

              {/* Plugin Sync & Version Verification Card */}
              <div className="p-6 rounded-2xl bg-slate-900/80 border border-slate-800 space-y-4">
                <div className="flex items-center justify-between">
                  <div className="flex items-center gap-2.5">
                    <Sparkles className="w-5 h-5 text-sky-400" />
                    <div>
                      <h3 className="text-sm font-bold text-slate-100 uppercase tracking-wider">
                        Pembaruan & Sinkronisasi Plugin In-Game
                      </h3>
                      <p className="text-[11px] text-slate-400 mt-0.5">
                        Memastikan binary NismaraTracker.dll di folder Euro Truck
                        Simulator 2 dan ATS selalu versi terbaru.
                      </p>
                    </div>
                  </div>
                  <button
                    onClick={() => handleAutoSyncPlugins(false)}
                    className="px-4 py-2 rounded-xl bg-sky-500 hover:bg-sky-400 text-slate-950 font-bold text-xs flex items-center gap-1.5 shadow-lg shadow-sky-500/20 transition-all cursor-pointer"
                  >
                    <FolderSync className="w-4 h-4" />
                    <span>Sinkronkan Plugin ke Game</span>
                  </button>
                </div>

                <div className="grid grid-cols-2 gap-3 pt-1">
                  <div className="p-3 rounded-xl bg-slate-950/60 border border-slate-800 flex items-center justify-between">
                    <div>
                      <span className="text-xs font-semibold text-slate-300">
                        Euro Truck Simulator 2
                      </span>
                      <p className="text-[10px] text-slate-500 font-mono truncate max-w-[200px]">
                        {ets2Game?.plugin_path || "Belum dipasang"}
                      </p>
                    </div>
                    <span
                      className={`px-2 py-0.5 rounded text-[10px] font-bold ${
                        !ets2Game?.plugin_installed
                          ? "bg-slate-800 text-slate-400"
                          : ets2Game.plugin_outdated
                            ? "bg-amber-500/20 text-amber-300 border border-amber-500/40"
                            : "bg-emerald-500/20 text-emerald-300 border border-emerald-500/40"
                      }`}
                    >
                      {!ets2Game?.plugin_installed
                        ? "Belum Pasang"
                        : ets2Game.plugin_outdated
                          ? "Update Tersedia"
                          : "Up to Date ✓"}
                    </span>
                  </div>

                  <div className="p-3 rounded-xl bg-slate-950/60 border border-slate-800 flex items-center justify-between">
                    <div>
                      <span className="text-xs font-semibold text-slate-300">
                        American Truck Simulator
                      </span>
                      <p className="text-[10px] text-slate-500 font-mono truncate max-w-[200px]">
                        {atsGame?.plugin_path || "Belum dipasang"}
                      </p>
                    </div>
                    <span
                      className={`px-2 py-0.5 rounded text-[10px] font-bold ${
                        !atsGame?.plugin_installed
                          ? "bg-slate-800 text-slate-400"
                          : atsGame.plugin_outdated
                            ? "bg-amber-500/20 text-amber-300 border border-amber-500/40"
                            : "bg-emerald-500/20 text-emerald-300 border border-emerald-500/40"
                      }`}
                    >
                      {!atsGame?.plugin_installed
                        ? "Belum Pasang"
                        : atsGame.plugin_outdated
                          ? "Update Tersedia"
                          : "Up to Date ✓"}
                    </span>
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* BOTTOM TICKER / FOOTER BAR */}
        <footer className="h-10 shrink-0 bg-[#070b12] border-t border-slate-800/80 flex items-center justify-between px-5 text-xs text-slate-400 z-20">
          {/* Left Ticker */}
          <div className="flex items-center gap-3 overflow-hidden">
            <div className="flex items-center gap-1.5 shrink-0">
              <span className="font-semibold text-slate-300">What's new</span>
              <span
                className={`px-1.5 py-0.5 rounded text-[10px] font-black uppercase ${
                  articles[0]?.categories?.[0]?.title
                    ?.toUpperCase()
                    .includes("UPDATE")
                    ? "bg-blue-600 text-white"
                    : articles[0]?.categories?.[0]?.title
                        ?.toUpperCase()
                        .includes("CONVOY")
                    ? "bg-emerald-600 text-white"
                    : "bg-sky-600 text-white"
                }`}
              >
                {articles[0]?.categories?.[0]?.title
                  ? articles[0].categories[0].title.toUpperCase()
                  : "NEW"}
              </span>
            </div>
            <button
              onClick={() => {
                if (articles[0]) {
                  setReadingArticle(articles[0]);
                } else {
                  setActiveTab("news");
                }
              }}
              className="truncate text-slate-300/90 hover:text-sky-300 text-[11px] text-left transition-colors cursor-pointer group flex items-center gap-1.5"
              title={
                articles[0]
                  ? `Klik untuk membaca: ${articles[0].title}`
                  : "Buka Tab Berita"
              }
            >
              <span className="truncate">
                {articles[0]
                  ? `${articles[0].title}${
                      articles[0].seoDescription
                        ? ` — ${articles[0].seoDescription}`
                        : ""
                    }`
                  : "Nismara Companion v1.0.0 resmi diluncurkan dengan native Gatekeeper Mutex guard dan peluncur game instan."}
              </span>
              <span className="text-[10px] text-sky-400 opacity-0 group-hover:opacity-100 transition-opacity shrink-0">
                (Baca ↗)
              </span>
            </button>
          </div>

          {/* Right Update Checker & Info */}
          <div className="flex items-center gap-3 shrink-0 ml-4">
            <button
              onClick={() => checkForUpdates(true)}
              disabled={updateChecking}
              className="flex items-center gap-1.5 px-2.5 py-1 rounded-lg bg-slate-800/80 hover:bg-slate-700/80 text-sky-400 hover:text-sky-300 text-[11px] font-semibold border border-slate-700/60 transition-all cursor-pointer"
              title="Periksa Pembaruan Nismara Companion & Plugin In-game"
            >
              <RefreshCw
                className={`w-3 h-3 ${updateChecking ? "animate-spin text-sky-300" : ""}`}
              />
              <span>{updateChecking ? "Memeriksa..." : "Periksa Pembaruan"}</span>
            </button>

            {availableUpdate && (
              <button
                onClick={() => setShowUpdateModal(true)}
                className="flex items-center gap-1 px-2.5 py-0.5 rounded-full bg-emerald-500/20 border border-emerald-500/40 text-emerald-300 text-[10px] font-bold animate-pulse cursor-pointer shadow-lg shadow-emerald-500/20"
              >
                <Sparkles className="w-3 h-3 text-emerald-400" />
                <span>v{availableUpdate.version} Tersedia!</span>
              </button>
            )}

            <button
              onClick={() => setShowAboutModal(true)}
              className="flex items-center gap-1 hover:text-slate-200 transition-colors cursor-pointer ml-1"
            >
              <span>About</span>
              <Info className="w-3.5 h-3.5" />
            </button>
          </div>
        </footer>
      </div>

      {/* AUTO-UPDATER MODAL */}
      {showUpdateModal && availableUpdate && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-md animate-in fade-in duration-150">
          <div className="w-[480px] p-6 rounded-2xl bg-slate-900 border border-sky-500/50 shadow-2xl shadow-sky-950/60 text-slate-200 relative flex flex-col gap-4">
            {!updateDownloading && (
              <button
                onClick={() => setShowUpdateModal(false)}
                className="absolute top-4 right-4 p-1 rounded-lg text-slate-400 hover:text-white hover:bg-slate-800 transition-colors"
              >
                <X className="w-4 h-4" />
              </button>
            )}

            <div className="flex items-center gap-3">
              <div className="w-12 h-12 rounded-xl bg-gradient-to-br from-sky-500 to-blue-600 flex items-center justify-center text-white shadow-lg shadow-sky-500/30">
                <Sparkles className="w-6 h-6" />
              </div>
              <div>
                <h3 className="text-base font-bold text-white flex items-center gap-2">
                  <span>Pembaruan Versi Tersedia</span>
                  <span className="px-2 py-0.5 rounded text-[10px] font-black bg-sky-500 text-slate-950">
                    NEW
                  </span>
                </h3>
                <div className="flex items-center gap-2 text-xs mt-0.5">
                  <span className="text-slate-400">Versi Saat Ini: v{APP_VERSION}</span>
                  <span className="text-slate-500">➜</span>
                  <span className="text-sky-300 font-bold">
                    Versi Baru: v{availableUpdate.version}
                  </span>
                </div>
              </div>
            </div>

            <div className="space-y-1.5">
              <span className="text-xs font-semibold text-slate-300 uppercase tracking-wider">
                Catatan Rilis (Changelog):
              </span>
              <div className="p-3.5 rounded-xl bg-slate-950 border border-slate-800 text-xs text-slate-300 leading-relaxed max-h-36 overflow-y-auto whitespace-pre-wrap font-sans">
                {availableUpdate.body ||
                  "Pembaruan kestabilan sistem, auto-sync telemetri, dan kompatibilitas server terbaru."}
              </div>
            </div>

            {/* Download Progress Bar */}
            {updateDownloading && downloadProgress && (
              <div className="space-y-1.5">
                <div className="flex items-center justify-between text-xs text-slate-300">
                  <span>Mengunduh paket instalasi...</span>
                  <span className="font-mono text-sky-400">
                    {downloadProgress.total > 0
                      ? `${Math.round((downloadProgress.downloaded / downloadProgress.total) * 100)}%`
                      : "Memulai..."}
                  </span>
                </div>
                <div className="w-full h-2 rounded-full bg-slate-800 overflow-hidden">
                  <div
                    className="h-full bg-gradient-to-r from-sky-500 to-emerald-400 transition-all duration-150"
                    style={{
                      width: `${
                        downloadProgress.total > 0
                          ? Math.min(
                              100,
                              (downloadProgress.downloaded /
                                downloadProgress.total) *
                                100,
                            )
                          : 10
                      }%`,
                    }}
                  />
                </div>
              </div>
            )}

            {/* Actions */}
            <div className="flex items-center justify-end gap-3 pt-2">
              {!updateDownloading && (
                <button
                  onClick={() => setShowUpdateModal(false)}
                  className="px-4 py-2 rounded-xl text-xs font-medium text-slate-400 hover:text-slate-200 transition-colors"
                >
                  Nanti Saja
                </button>
              )}
              <button
                onClick={handleApplyUpdate}
                disabled={updateDownloading}
                className="px-5 py-2.5 rounded-xl bg-gradient-to-r from-sky-500 to-blue-600 hover:from-sky-400 hover:to-blue-500 text-white font-bold text-xs shadow-lg shadow-sky-500/25 flex items-center gap-2 transition-all hover:scale-102 active:scale-98 disabled:opacity-50 cursor-pointer"
              >
                {updateDownloading ? (
                  <>
                    <RefreshCw className="w-3.5 h-3.5 animate-spin" />
                    <span>Sedang Memasang & Memulai Ulang...</span>
                  </>
                ) : (
                  <>
                    <Download className="w-3.5 h-3.5" />
                    <span>Unduh & Perbarui Sekarang</span>
                  </>
                )}
              </button>
            </div>
          </div>
        </div>
      )}

      {/* ABOUT MODAL */}
      {showAboutModal && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/75 backdrop-blur-sm animate-in fade-in duration-150">
          <div className="w-[440px] p-6 rounded-2xl bg-slate-900 border border-slate-700/80 shadow-2xl text-slate-200 relative flex flex-col gap-4">
            <button
              onClick={() => setShowAboutModal(false)}
              className="absolute top-4 right-4 p-1 rounded-lg text-slate-400 hover:text-white hover:bg-slate-800 transition-colors"
            >
              <X className="w-4 h-4" />
            </button>

            <div className="flex items-center gap-3">
              <div className="w-12 h-12 rounded-xl bg-slate-950 border border-slate-800 flex items-center justify-center p-1.5 shadow-lg shadow-sky-500/25">
                <img
                  src="/images/logo_nismara.png"
                  alt="Nismara"
                  className="w-full h-full object-contain"
                />
              </div>
              <div>
                <h3 className="text-base font-bold text-white">
                  Nismara Companion
                </h3>
                <span className="text-xs text-sky-400 font-medium">
                  Version {APP_VERSION} (Official Release)
                </span>
              </div>
            </div>

            <p className="text-xs text-slate-300 leading-relaxed">
              Nismara Companion adalah aplikasi pendamping desktop resmi untuk
              komunitas pengemudi VTC <strong>Nismara Transport</strong>.
              Aplikasi ini bertindak sebagai penjaga gerbang (Gatekeeper Mutex)
              untuk in-game telemetry plugin (`NismaraTracker.dll`) serta media
              komunikasi telemetri real-time.
            </p>

            <div className="p-3 rounded-xl bg-slate-950/70 border border-slate-800 text-[11px] space-y-1 font-mono text-slate-400">
              <div>Framework: Tauri v2 + React 19 + Tailwind v4</div>
              <div>Runtime: Rust Native x64 / MSVC static</div>
              <div>Gatekeeper: Global\NismaraCompanionRunning</div>
            </div>

            <div className="flex items-center justify-between text-xs text-slate-500 pt-1">
              <span>© 2026 Nismara Transport</span>
              <a
                href="https://beta.nismara.web.id"
                target="_blank"
                rel="noreferrer"
                className="text-sky-400 hover:underline flex items-center gap-1"
              >
                <span>Website Resmi</span>
                <ExternalLink className="w-3 h-3" />
              </a>
            </div>
          </div>
        </div>
      )}

      {/* ARTICLE READER MODAL */}
      {readingArticle && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-md p-4 sm:p-6 animate-in fade-in duration-150">
          <div className="w-full max-w-2xl max-h-[85vh] rounded-2xl bg-slate-900 border border-slate-700/80 shadow-2xl text-slate-200 relative flex flex-col overflow-hidden">
            {/* Modal Header */}
            <div className="p-5 border-b border-slate-800 flex items-center justify-between bg-slate-900/95 sticky top-0 z-10">
              <div className="flex items-center gap-2">
                <BookOpen className="w-5 h-5 text-sky-400" />
                <span className="text-sm font-bold text-white uppercase tracking-wider">
                  Nismara Gazette
                </span>
              </div>
              <button
                onClick={() => setReadingArticle(null)}
                className="p-1.5 rounded-lg text-slate-400 hover:text-white hover:bg-slate-800 transition-colors cursor-pointer"
              >
                <X className="w-5 h-5" />
              </button>
            </div>

            {/* Modal Scrollable Content */}
            <div className="flex-1 overflow-y-auto p-6 space-y-5">
              {/* Cover Image */}
              {readingArticle.mainImage?.asset?._ref && (
                <div className="w-full h-64 rounded-xl overflow-hidden bg-slate-950">
                  <img
                    src={
                      getSanityImageUrl(
                        readingArticle.mainImage.asset._ref,
                      ) || ""
                    }
                    alt={readingArticle.title}
                    className="w-full h-full object-cover"
                  />
                </div>
              )}

              {/* Badges & Meta */}
              <div className="flex flex-wrap items-center justify-between gap-2 text-xs text-slate-400">
                <div className="flex flex-wrap gap-1.5">
                  {readingArticle.categories?.map((cat, idx) => (
                    <span
                      key={idx}
                      className={`px-2.5 py-0.5 rounded-md text-[10px] font-bold uppercase tracking-wider border ${getCategoryColor(
                        cat.title,
                      )}`}
                    >
                      {cat.title}
                    </span>
                  ))}
                </div>
                <div className="flex items-center gap-3">
                  <span className="flex items-center gap-1">
                    <Calendar className="w-3.5 h-3.5 text-slate-500" />
                    <span>
                      {formatNewsDate(readingArticle.publishedAt)}
                    </span>
                  </span>
                  {readingArticle.author?.name && (
                    <span className="flex items-center gap-1">
                      <User className="w-3.5 h-3.5 text-slate-500" />
                      <span>{readingArticle.author.name}</span>
                    </span>
                  )}
                </div>
              </div>

              {/* Title */}
              <h2 className="text-2xl font-bold text-white leading-snug">
                {readingArticle.title}
              </h2>

              {/* Text content paragraphs */}
              <div className="text-sm text-slate-300 leading-relaxed whitespace-pre-line space-y-4 font-sans">
                {extractBlockText(readingArticle.body) ||
                  readingArticle.seoDescription ||
                  "Tidak ada konten rincian."}
              </div>
            </div>

            {/* Modal Footer */}
            <div className="p-4 bg-slate-950/80 border-t border-slate-800 flex items-center justify-between text-xs text-slate-400">
              <span>Publikasi resmi Nismara Group (Sanity CMS)</span>
              {readingArticle.slug?.current && (
                <button
                  onClick={() => {
                    const url = `https://nismara.web.id/articles/${readingArticle.slug?.current}`;
                    openUrl(url).catch(() => window.open(url, "_blank"));
                  }}
                  className="px-4 py-2 rounded-lg bg-sky-600 hover:bg-sky-500 text-white font-bold flex items-center gap-1.5 transition-all cursor-pointer shadow-md shadow-sky-600/20"
                >
                  <span>Buka di Browser</span>
                  <ExternalLink className="w-3.5 h-3.5" />
                </button>
              )}
            </div>
          </div>
        </div>
      )}

      {/* EVENT DETAIL MODAL */}
      {selectedEventDetail && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-md p-4 sm:p-6 animate-in fade-in duration-150">
          <div className="w-full max-w-2xl max-h-[85vh] rounded-2xl bg-slate-900 border border-slate-700/80 shadow-2xl text-slate-200 relative flex flex-col overflow-hidden">
            {/* Modal Header */}
            <div className="p-5 border-b border-slate-800 flex items-center justify-between bg-slate-900/95 sticky top-0 z-10">
              <div className="flex items-center gap-2">
                <Trophy className="w-5 h-5 text-amber-400" />
                <span className="text-sm font-bold text-white uppercase tracking-wider">
                  Rincian {selectedEventDetail.typeLabel}
                </span>
              </div>
              <button
                onClick={() => setSelectedEventDetail(null)}
                className="p-1.5 rounded-lg text-slate-400 hover:text-white hover:bg-slate-800 transition-colors cursor-pointer"
              >
                <X className="w-5 h-5" />
              </button>
            </div>

            {/* Modal Scrollable Content */}
            <div className="flex-1 overflow-y-auto p-6 space-y-5">
              {/* Cover Banner */}
              {selectedEventDetail.imageUrl && (
                <div className="w-full h-52 rounded-xl overflow-hidden bg-slate-950">
                  <img
                    src={selectedEventDetail.imageUrl}
                    alt={selectedEventDetail.title}
                    className="w-full h-full object-cover"
                  />
                </div>
              )}

              {/* Badges & Meta */}
              <div className="flex flex-wrap items-center justify-between gap-2 text-xs text-slate-400">
                <div className="flex items-center gap-2">
                  <span
                    className={`px-2.5 py-0.5 rounded-md text-[10px] font-bold uppercase tracking-wider border ${selectedEventDetail.badgeColor}`}
                  >
                    {selectedEventDetail.typeLabel}
                  </span>
                  {selectedEventDetail.isActive ? (
                    <span className="px-2 py-0.5 rounded-md text-[10px] font-bold bg-emerald-500/20 text-emerald-400 border border-emerald-500/30 flex items-center gap-1">
                      <span className="w-1.5 h-1.5 rounded-full bg-emerald-400 animate-pulse" />
                      SEDANG BERLANGSUNG
                    </span>
                  ) : (
                    <span className="px-2 py-0.5 rounded-md text-[10px] font-bold bg-slate-800 text-slate-400">
                      EVENT SELESAI
                    </span>
                  )}
                </div>

                <div className="flex items-center gap-1">
                  <Calendar className="w-3.5 h-3.5 text-slate-500" />
                  <span>
                    {formatNewsDate(selectedEventDetail.startDate || "")}
                    {selectedEventDetail.endDate
                      ? ` — ${formatNewsDate(selectedEventDetail.endDate)}`
                      : ""}
                  </span>
                </div>
              </div>

              {/* Title */}
              <h2 className="text-2xl font-bold text-white leading-snug">
                {selectedEventDetail.title}
              </h2>

              {/* Description if any */}
              {selectedEventDetail.description && (
                <p className="text-sm text-slate-300 leading-relaxed whitespace-pre-line">
                  {selectedEventDetail.description}
                </p>
              )}

              {/* CONTRACT DETAIL STATS */}
              {selectedEventDetail.type === "CONTRACT" &&
                selectedEventDetail.stats && (
                  <div className="p-4 rounded-xl bg-slate-950/70 border border-slate-800 space-y-3">
                    <h4 className="text-xs font-bold text-sky-400 uppercase tracking-wider">
                      Statistik Kontrak Perusahaan
                    </h4>
                    <div className="grid grid-cols-2 sm:grid-cols-4 gap-3 text-center">
                      <div className="p-2.5 rounded-lg bg-slate-900 border border-slate-800/80">
                        <span className="block text-slate-400 text-[10px]">
                          Total Kontrak
                        </span>
                        <span className="text-sm font-bold text-white">
                          {selectedEventDetail.stats.completedContracts ?? 0} Selesai
                        </span>
                      </div>
                      <div className="p-2.5 rounded-lg bg-slate-900 border border-slate-800/80">
                        <span className="block text-slate-400 text-[10px]">
                          Total Jarak
                        </span>
                        <span className="text-sm font-bold text-white">
                          {(selectedEventDetail.stats.totalDistanceKm ?? 0).toLocaleString()}{" "}
                          km
                        </span>
                      </div>
                      <div className="p-2.5 rounded-lg bg-slate-900 border border-slate-800/80">
                        <span className="block text-slate-400 text-[10px]">
                          Total Muatan
                        </span>
                        <span className="text-sm font-bold text-white">
                          {(selectedEventDetail.stats.totalMassTons ?? 0).toLocaleString()}{" "}
                          Ton
                        </span>
                      </div>
                      <div className="p-2.5 rounded-lg bg-slate-900 border border-slate-800/80">
                        <span className="block text-slate-400 text-[10px]">
                          Total NC Diperoleh
                        </span>
                        <span className="text-sm font-bold text-emerald-400">
                          {(selectedEventDetail.stats.totalNCEarned ?? 0).toLocaleString()}{" "}
                          NC
                        </span>
                      </div>
                    </div>
                  </div>
                )}

              {/* COUPON DETAIL */}
              {selectedEventDetail.type === "COUPON" &&
                selectedEventDetail.couponCode && (
                  <div className="p-4 rounded-xl bg-amber-500/10 border border-amber-500/30 space-y-3">
                    <span className="text-xs font-bold text-amber-300 uppercase tracking-wider block">
                      Kupon Klaim Driver
                    </span>
                    <div className="flex items-center justify-between p-3 rounded-lg bg-slate-950/80 border border-amber-500/20">
                      <div>
                        <span className="text-slate-400 text-[10px] block">
                          KODE KUPON:
                        </span>
                        <span className="text-lg font-mono font-bold text-amber-300">
                          {selectedEventDetail.couponCode}
                        </span>
                      </div>
                      <button
                        onClick={() =>
                          handleCopyCouponCode(
                            selectedEventDetail.couponCode || "",
                          )
                        }
                        className="px-3.5 py-1.5 rounded-lg bg-amber-500/20 hover:bg-amber-500/30 text-amber-200 text-xs font-semibold flex items-center gap-1.5 transition-all cursor-pointer"
                      >
                        {copiedCouponCode ===
                        selectedEventDetail.couponCode ? (
                          <>
                            <Check className="w-3.5 h-3.5 text-emerald-400" />
                            <span>Tersalin ke Clipboard</span>
                          </>
                        ) : (
                          <>
                            <Copy className="w-3.5 h-3.5" />
                            <span>Salin Kode Kupon</span>
                          </>
                        )}
                      </button>
                    </div>
                    <div className="text-xs text-slate-300 flex items-center justify-between">
                      <span>
                        Hadiah:{" "}
                        <strong>
                          {selectedEventDetail.rewardType ===
                          "PENALTY_TICKET"
                            ? "Tiket Hapus Penalti"
                            : "Nismara Coin (NC)"}
                        </strong>
                      </span>
                      <span>
                        Total Klaim:{" "}
                        <strong>
                          {selectedEventDetail.claimsCount || 0} Driver
                        </strong>
                      </span>
                    </div>
                  </div>
                )}

              {/* GIVEAWAY QUESTS & PRIZES */}
              {selectedEventDetail.type === "GIVEAWAY" && (
                <div className="space-y-4">
                  {/* Quests List */}
                  {selectedEventDetail.quests &&
                    selectedEventDetail.quests.length > 0 && (
                      <div className="p-4 rounded-xl bg-slate-950/70 border border-slate-800 space-y-3">
                        <h4 className="text-xs font-bold text-purple-300 uppercase tracking-wider flex items-center gap-1.5">
                          <Trophy className="w-3.5 h-3.5 text-purple-400" />
                          <span>Misi / Quests untuk Mendapatkan Tiket</span>
                        </h4>
                        <div className="space-y-2">
                          {selectedEventDetail.quests.map((q, idx) => (
                            <div
                              key={idx}
                              className="p-3 rounded-lg bg-slate-900 border border-slate-800 flex items-center justify-between gap-3 text-xs"
                            >
                              <div className="space-y-0.5">
                                <span className="font-bold text-white block">
                                  {q.title}
                                </span>
                                <span className="text-slate-400 block">
                                  {q.description}
                                </span>
                              </div>
                              <span className="px-2.5 py-1 rounded-md bg-purple-500/20 text-purple-300 font-bold border border-purple-500/30 shrink-0">
                                +{q.rewardTickets} Tiket
                              </span>
                            </div>
                          ))}
                        </div>
                      </div>
                    )}

                  {/* Prizes List */}
                  {selectedEventDetail.prizes &&
                    selectedEventDetail.prizes.length > 0 && (
                      <div className="p-4 rounded-xl bg-slate-950/70 border border-slate-800 space-y-3">
                        <h4 className="text-xs font-bold text-amber-300 uppercase tracking-wider flex items-center gap-1.5">
                          <Gift className="w-3.5 h-3.5 text-amber-400" />
                          <span>Kolam Hadiah & Juara (Prize Pool)</span>
                        </h4>
                        <div className="space-y-2">
                          {selectedEventDetail.prizes.map((p, idx) => (
                            <div
                              key={idx}
                              className="p-3 rounded-lg bg-slate-900 border border-slate-800 space-y-1.5 text-xs"
                            >
                              <div className="flex items-center justify-between">
                                <span className="font-bold text-amber-300">
                                  {p.tierTitle}
                                </span>
                                <span className="text-[11px] text-slate-400">
                                  {p.winnerCount || 1} Pemenang
                                </span>
                              </div>
                              <div className="flex flex-wrap gap-1.5 pt-1">
                                {p.rewards.map((r, rIdx) => (
                                  <span
                                    key={rIdx}
                                    className="px-2 py-0.5 rounded bg-slate-800 border border-slate-700/80 text-[11px] text-slate-200"
                                  >
                                    🎁 {r.title}
                                  </span>
                                ))}
                              </div>
                            </div>
                          ))}
                        </div>
                      </div>
                    )}
                </div>
              )}

              {/* NC EVENT DETAIL */}
              {selectedEventDetail.type === "NC_EVENT" && (
                <div className="p-4 rounded-xl bg-emerald-500/10 border border-emerald-500/30 space-y-2 text-xs">
                  <span className="text-emerald-300 font-bold block text-sm">
                    Pengali Bonus NC: {selectedEventDetail.multiplierText}
                  </span>
                  <p className="text-slate-300">
                    Selama event berlangsung, setiap pekerjaan yang diselesaikan
                    otomatis mendapatkan bonus pendapatan Nismara Coin berlipat
                    ganda!
                  </p>
                  <div className="pt-2 text-slate-400 flex items-center justify-between">
                    <span>
                      Partisipasi:{" "}
                      <strong>
                        {selectedEventDetail.participantsCount || 0} Driver
                      </strong>
                    </span>
                    <span>
                      Status:{" "}
                      <strong>
                        {selectedEventDetail.isActive ? "Aktif" : "Selesai"}
                      </strong>
                    </span>
                  </div>
                </div>
              )}
            </div>

            {/* Modal Footer */}
            <div className="p-4 bg-slate-950/80 border-t border-slate-800 flex items-center justify-between text-xs text-slate-400">
              <span>Nismara Transport Logistics Events</span>
              <button
                onClick={() => setSelectedEventDetail(null)}
                className="px-4 py-2 rounded-lg bg-slate-800 hover:bg-slate-700 text-white font-bold transition-all cursor-pointer"
              >
                Tutup
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
