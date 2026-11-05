# AGENTS.md — NismaraTracker & Nismara Tracker Server

Dokumen ini adalah panduan arsitektur, referensi teknis, histori implementasi, serta aturan **DO & DON'T** untuk seluruh ekosistem **NismaraTracker**. Dokumen ini dirancang agar AI Agent / Model apa pun yang melanjutkan pengembangan dapat langsung memahami keseluruhan konteks proyek tanpa kehilangan detail sedikit pun.

---

## 1. Gambaran Umum Proyek (Project Overview)

Ekosistem **NismaraTracker** terdiri dari dua repositori/komponen utama yang saling terhubung:

1. **Client Plugin & Companion App (`nismara-tracker` & `nismara-companion`)**
   - **Lokasi**: `d:\Project\Nismara Transport\Project\nismara-tracker`
   - **Repository Git**: `https://github.com/mhazm/fortune-cookie-client.git`
   - **Peran**: Native C++20 (`x64`) in-game telemetry & overlay plugin untuk **Euro Truck Simulator 2 (ETS2)** dan **American Truck Simulator (ATS)** serta desktop companion hub berbasis Tauri v2 + React.
   - **Bentuk Distribusi**: **Single Standalone Binary (`bin/NismaraTracker.dll`)** tanpa ketergantungan DLL eksternal (menggunakan static MSVC runtime `/MT`) dan aplikasi desktop installer (`.msi` / `.exe`).
   - **Antarmuka**: In-game GUI berbentuk **iPhone 16 OS** (dimodelkan presisi dari `IPhone_16_Vector.svg` & `Nismara Group Logo.svg`) yang dirender langsung di dalam proses DirectX 11 game (toggle dengan tombol **`F11`**).

2. **Dedicated Tracker Backend (`nismara-tracker-server`)**
   - **Lokasi**: `D:\Project\Nismara Transport\Project\nismara-tracker-server`
   - **Repository Git**: `https://github.com/mhazm/fortune-cookie-server.git`
   - **Peran**: Server backend mandiri berbasis **Node.js + Express + Mongoose** yang menjembatani `NismaraTracker.dll` dengan database utama MongoDB (`nismara` database).
   - **Alasan Arsitektur**: Website utama Nismara Transport di-hosting di **Vercel** (serverless). Polling dan pengiriman telemetri/job dari plugin secara terus-menerus ke Vercel akan menguras kuota *Serverless Function Invocations* dan menyebabkan *overcost*. Oleh karena itu, seluruh komunikasi dari `NismaraTracker.dll` diarahkan ke `nismara-tracker-server` (berjalan di `http://localhost:3100` saat development atau VPS khusus saat production) dengan koneksi MongoDB pool yang persisten.

---

## 2. Tech Stack Lengkap

### A. Client Plugin (`nismara-tracker`)
- **Bahasa & Standar**: C++20 (`MSVC` x64, `/W4 /MP /EHsc /utf-8`, Static Runtime `/MT` di Release dan `/MTd` di Debug).
- **Build System**: CMake (`>= 3.20`) + MSBuild (Visual Studio 2022/2026 x64).
- **Game Telemetry API**: **SCS Telemetry SDK v1.01** (`scs-sdk/include/`).
- **Graphics & Rendering Hook**:
  - **DirectX 11 (`d3d11.lib`, `dxgi.lib`)**: Hooking `IDXGISwapChain::Present` (VTable index `8`) dan `IDXGISwapChain::ResizeBuffers` (VTable index `13`).
  - **MinHook (x64)**: Digunakan untuk hooking fungsi DirectX 11, Win32 Cursor APIs, dan DirectInput8.
  - **Dear ImGui (Docking Branch)**: Dirender di atas *render target* DX11 game dengan font TrueType asli Windows (`Segoe UI Semibold` / `Bold` / `Regular` dari `C:\Windows\Fonts`).
- **Input & Mouse Capture Bypass**:
  - Hooking `WndProc` (`SetWindowLongPtrW`) pada window game untuk menangkap hotkey (`F11` / `VK_F11`) dan memblokir input mouse/keyboard ke game saat layar iPhone 16 sedang aktif.
  - Hooking Win32 `SetCursorPos`, `ClipCursor`, `GetCursorPos` serta **DirectInput8 (`IDirectInputDevice8W::GetDeviceState` & `GetDeviceData`)** agar kamera kabin truk tidak ikut berputar saat pemain menggerakkan kursor di dalam UI iPhone 16.
- **Image & Texture Pipeline**:
  - **Windows Imaging Component (`windowscodecs.lib` / WIC)**: Memuat custom wallpaper (`.png`, `.jpg`, `.jpeg`, `.webp`, `.bmp`, `.gif`) menjadi `ID3D11ShaderResourceView*`.
  - **Embedded Vector Logo Mask (`include/ui/nismara_logo_data.hpp`)**: Hasil rasterisasi *64x-supersampled (8x8 MSAA)* dari `Nismara Group Logo.svg` (`360x100` alpha mask) yang di-composite langsung ke tengah default/preset wallpaper (`512x1108`).
- **Networking & JSON**:
  - **WinHTTP (`winhttp.lib`)**: Asynchronous HTTP/HTTPS client pada background worker thread (`std::thread`) sehingga tidak pernah memblokir render thread DX11 maupun callback SCS SDK.
  - **nlohmann/json (`vendor/json/nlohmann/json.hpp`)**: Single-header JSON parser/serializer.
- **Server Detection (`iphlpapi.lib`, `ws2_32.lib`, `tlhelp32.h`)**:
  - Kombinasi pemeriksaan modul DLL yang ter-inject di memori proses game, tabel koneksi jaringan aktif, dan parsing *reverse-chronological* pada `Documents\Euro Truck Simulator 2\game.log.txt` (dan ATS).

### B. Backend Server (`nismara-tracker-server`)
- **Runtime & Framework**: Node.js + Express (`^4.21.2`) + Mongoose (`^8.10.1`) + CORS + Dotenv.
- **Database**: MongoDB (`nismara` database) menggunakan skema koleksi yang 100% kompatibel dengan ekosistem Web (`beta.nismara.web.id`) dan Bot Discord Nismara Transport.

---

## 3. Struktur Direktori & Peta File

### A. `d:\Project\Nismara Transport\Project\nismara-tracker`
```text
nismara-tracker/
├── CMakeLists.txt                       # Konfigurasi build CMake (x64, C++20, /MT, single DLL)
├── Nismara Group Logo.svg               # Master vector logo Nismara Group (di-bake ke nismara_logo_data.hpp)
├── IPhone_16_Vector.svg                 # Master referensi geometri dan palet warna casing iPhone 16
├── include/
│   ├── core/
│   │   ├── telemetry_state.hpp          # Thread-safe singleton penyimpanan data telemetri, akun, fleet, garage, job
│   │   ├── scs_callbacks.hpp            # Deklarasi callback SCS SDK (frame, channel, config, gameplay events)
│   │   ├── config_manager.hpp           # Load/save konfigurasi lokal (Documents/<Game>/nismara_config.dat)
│   │   ├── profile_validator.hpp        # Validasi 9 parameter cvars game & deteksi mod terlarang
│   │   ├── api_client.hpp               # Async WinHTTP client (/me, /jobs/start, /jobs/complete, /jobs/cancel)
│   │   └── server_detector.hpp          # Detektor server bermain (Singleplayer, SCS Convoy, TruckersMP, HaulMP)
│   ├── hooks/
│   │   ├── dx11_hook.hpp                # Hook IDXGISwapChain::Present & ResizeBuffers
│   │   └── wndproc_hook.hpp             # Hook WndProc, Win32 Cursor APIs, & DirectInput8 mouse lock bypass
│   └── ui/
│       ├── tablet_gui.hpp               # State & renderer utama antarmuka iPhone 16 OS
│       ├── texture_manager.hpp          # Manajemen tekstur DX11 untuk preset & custom wallpaper
│       └── nismara_logo_data.hpp        # Data alpha mask 64x-supersampled (360x100) dari Nismara Group Logo.svg
├── src/
│   ├── dllmain.cpp                      # Entry point DLL & ekspor scs_telemetry_init / scs_telemetry_shutdown
│   ├── NismaraTracker.def               # Definisi simbol ekspor DLL untuk SCS SDK
│   ├── core/
│   │   ├── telemetry_state.cpp          # Implementasi thread-safe getter/setter TelemetryState
│   │   ├── scs_callbacks.cpp            # Parser atribut SCS SDK, fleet matching, akumulasi damage, & job lifecycle
│   │   ├── config_manager.cpp           # Parser & writer Documents/<Game>/nismara_config.dat + auto-create folder wallpapers/
│   │   ├── profile_validator.cpp        # Parser file config.cfg & game.log.txt (cvars & forbidden mods scan)
│   │   ├── api_client.cpp               # Worker thread WinHTTP & JSON parsing respons backend
│   │   └── server_detector.cpp          # Logika deteksi modul DLL (core_ets2mp.dll, haulmp_stub.dll) & game.log.txt
│   ├── hooks/
│   │   ├── dx11_hook.cpp                # Dummy swapchain VTable discovery, RTV lifecycle, ImGui frame loop
│   │   └── wndproc_hook.cpp             # F11 toggle, DirectInput8 VTable hook, & pembebasan kursor di dalam game
│   └── ui/
│       ├── texture_manager.cpp          # Generator wallpaper procedural iOS 18 + Nismara Logo & WIC loader
│       └── tablet_gui.cpp               # Seluruh UI iPhone 16 (Lock Screen, Home Screen, Dynamic Island, 6 Apps)
└── bin/
    └── NismaraTracker.dll               # Output akhir binary plugin siap pakai
```

### B. `D:\Project\Nismara Transport\Project\nismara-tracker-server`
```text
nismara-tracker-server/
├── server.js                            # Entry point Express server (Port 3100) & koneksi MongoDB
├── .env                                 # Konfigurasi PORT, MONGODB_URI, GUILD_ID, dan ekonomi reward
└── src/
    ├── middleware/
    │   └── authDriver.js                # Validasi token driver (Header x-tracker-token / Authorization Bearer / Body)
    ├── models/
    │   ├── User.js                      # Koleksi `users` (name, discordId, trackerToken, truckyRank, steamId, dll.)
    │   ├── Currency.js                  # Koleksi `currencies` (userId: String(discordId), guildId, totalNC, dll.)
    │   ├── Point.js                     # Koleksi `points` (userId: String(discordId), guildId, points, level, exp)
    │   ├── Fleet.js                     # Koleksi `fleets` (owner: ObjectId(user._id), fleet_number, in_game_id, dll.)
    │   ├── Garage.js                    # Koleksi `garages` (owner: ObjectId(user._id), city, country, level, slots)
    │   ├── JobHistory.js                # Koleksi `jobhistories` (jobId sekuensial "1","2",..., provider: "nismara")
    │   ├── Cargo.js                     # Koleksi `cargos` (pencocokan kargo & multiplier harga)
    │   ├── Fuel.js                      # Koleksi `fuels` (harga bahan bakar per liter)
    │   ├── CurrencyBoost.js             # Koleksi `currencyboosts` (multiplier event NC/EXP aktif)
    │   ├──CommunityGoal.js             # Koleksi `communitygoals` (kontribusi event komunitas otomatis)
    │   └── Transaction.js               # Koleksi `transactions` (pencatatan mutasi pendapatan job NC)
    ├── routes/
    │   ├── me.routes.js                 # GET & POST /api/tracker/me (sinkronisasi profil, wallet, fleet, garage, ongoingJob)
    │   └── jobs.routes.js               # POST /api/tracker/jobs/start, /complete, /cancel
    └── services/
        └── jobProcessor.js              # Kalkulasi finansial job, cap +25% plannedDistanceKm, update wallet/exp/fleet
```

---

## 4. Aturan Skema Database MongoDB (KRITIS!)

Saat memodifikasi query atau skema di `nismara-tracker-server`, **WAJIB** mematuhi relasi antar-koleksi MongoDB Nismara berikut:

1. **Perbedaan `user._id` vs `user.discordId`**:
   - Koleksi **`currencies`** dan **`points`** **TIDAK** menggunakan `ObjectId(user._id)`, melainkan menggunakan:
     - `userId: String(user.discordId)`
     - `guildId: "863959415702028318"` (atau `process.env.NISMARA_GUILD_ID`)
   - Koleksi **`fleets`** dan **`garages`** menggunakan:
     - `owner: user._id` (`ObjectId`)
2. **Koleksi `points` (POIN PENALTI DRIVER, BUKAN PRESTASI)**:
   - Field `totalPoints` di koleksi `points` mencatat **poin penalti pelanggaran driver** (skala 0–50).
   - Nilai ideal adalah **0** (Aman). Status: `<10` Aman (Hijau), `10-24` Penalty 1 (Kuning), `25-49` Penalty 2 (Oranye), `≥50` Penalty 3 (Merah).
   - Jangan pernah menganggap `totalPoints` sebagai skor prestasi atau men-default nilainya ke 100!
3. **Koleksi `garages` (`safeboxStock` = TIKET HAPUS PENALTI)**:
   - Field `safeboxStock` di koleksi `garages` adalah **jumlah tiket hapus penalti**, BUKAN saldo uang/NC.
   - Kapasitas maksimal tiket: $\text{Max Tickets} = 10 + (\text{safeboxLevel} - 1) \times 5$.
   - Rasio penggunaan: **1 Tiket Safebox = 1 Poin Penalti Dihapus**.
4. **Koleksi `currencies` & Aturan Diskon Kumulatif**:
   - Field saldo utama di database bernama **`totalNC`** (perhatikan huruf besar `NC`).
   - Endpoint `/api/tracker/me` mengembalikan keduanya (`totalNC` dan `totalNc`) di JSON.
   - Biaya tebus penalti via NC: Base `pointPrice = 3000 NC` (dari `guildsettings`).
   - **Diskon Kumulatif**: Server Booster (-500 NC) + Nismara+ Aktif (-500 NC). Jika driver memiliki keduanya, diskon menjadi **-1,000 NC** (biaya tebus menjadi 2,000 NC/poin).
5. **Koleksi `jobhistories` (Pencatatan Job dari Plugin)**:
   - **`provider`**: Selalu bernilai `"nismara"` (untuk membedakan dari job legacy `"trucky"`).
   - **`jobId`**: String angka sekuensial auto-increment mulai dari `"1"`, `"2"`, `"3"`, dst. Dihasilkan oleh fungsi `getNextSequentialJobId()` di `src/routes/jobs.routes.js` yang mencari nilai numerik tertinggi pada dokumen `jobhistories` dengan `provider: "nismara"`.
   - **`status`**: `"ONGOING"` saat job dimulai (`job_started`), lalu berubah menjadi `"COMPLETED"` (`job_delivered`) atau `"CANCELED"` (`job_cancelled`).

---

## 5. Fitur & Milestone yang Sudah Selesai Dikerjakan (Phases 1–5)

### Phase 1: Core SCS Telemetry SDK & DirectX 11 Overlay
- Integrasi penuh dengan **SCS Telemetry SDK v1.01** (`scs_telemetry_init` / `scs_telemetry_shutdown`).
- Hooking `IDXGISwapChain::Present` dan `ResizeBuffers` menggunakan dummy window/swapchain VTable discovery.
- Penanganan penuh *fullscreen / alt-tab / resolution resize* dengan melepas `ID3D11RenderTargetView` sebelum `ResizeBuffers` dieksekusi.
- Hooking `WndProc` (`F11` untuk membuka/menutup HP) + bypass penguncian kursor Win32 & DirectInput8 agar mouse dapat mengklik UI dengan bebas tanpa menggerakkan kamera truk.

### Phase 2: Sinkronisasi Akun Driver (`NSM-...`)
- Konfigurasi lokal disimpan secara otomatis di direktori dokumen user game (`Documents\Euro Truck Simulator 2\nismara_config.dat` atau `Documents\American Truck Simulator\nismara_config.dat`, dengan dukungan parameter `-homedir` dan path XDG/macOS), sehingga folder instalasi `bin/win_x64/plugins/` tetap bersih (hanya berisi binary `NismaraTracker.dll`) dan satu lokasi dengan `game.log.txt` serta `profiles/.../save/.../game.sii`.
- Verifikasi token driver (`NSM-XXXXXX-...`) ke `/api/tracker/me` secara asynchronous.
- Menampilkan nama driver, rank, level, EXP, Nismara+ status, dan saldo **Nismara Cash (NC)** secara real-time.

### Phase 3: Antarmuka iPhone 16 OS & Custom Wallpapers
- Casing dan layar OLED **iPhone 16** (`392x794` px, layar dalam `350x758` px dengan `46px` rounded corners) dengan 5 pilihan warna frame (*Ultramarine*, *Black Titanium*, *Natural Titanium*, *Teal*, *Pink*).
- **Lock Screen**: Jam besar iOS 18 (`62px`), tanggal, greeting pill, widget **TRUCK STATUS**, widget **NISMARA WALLET**, widget **SERVER STATUS** (`Connected` + `Playing Singleplayer / TruckersMP / HaulMP / SCS Convoy`), dan kartu notifikasi rute aktif.
- **Home Screen**: Smart Driver Widget di bagian atas, grid 8 ikon aplikasi (*Telemetry*, *Job*, *Nismara ID*, *Garage*, *Events [SOON]*, *Wallpapers*, *Settings*, *Lock*), serta iOS Frosted Dock di bagian bawah.
- **Dynamic Island**: Pill interaktif di bagian atas layar yang menampilkan nomor lambung fleet aktif (`NL-074 | 80 km/h`) dan dapat diklik untuk melakukan *expand* informasi detail (kecepatan, gigi, sisa jarak navigasi, atau bahan bakar & RPM).
- **Default & Custom Wallpaper**:
  - 5 Preset tema iOS 18 (*Ultramarine*, *Midnight*, *Executive Gold*, *Cyberpunk*, *Sunset*) yang menampilkan logo resmi **`Nismara Group Logo.svg`** tepat di tengah wallpaper.
  - Mendukung pemilihan gambar kustom dari folder `Documents\<Game>\wallpapers\` secara terpisah untuk Lock Screen dan Home Screen.

### Phase 4: Sinkronisasi Kendaraan In-Game $\leftrightarrow$ Database `fleets` & `garages`
- Setiap kali truk di dalam game berganti atau akun selesai disinkronisasi, `TelemetryState::MatchActiveVehicleWithFleet` mencocokkan truk in-game dengan daftar `fleets` milik driver di database:
  - **Prioritas 1**: Pencocokan eksak `in_game_id` (`brand_id + "." + model_id`, contoh: `renault.magnum.2009` == `renault.magnum.2009`).
  - **Prioritas 2**: Pencocokan prefix model dasar (`scania.s_2016` cocok dengan `scania.s_2016.8x4`) dengan syarat `fleet.game` sesuai (`ETS2` atau `ATS`).
  - **Prioritas 3**: Fallback pencocokan nama brand + model secara *case-insensitive*.
- Aplikasi **My Garage** di dalam HP menampilkan daftar seluruh garasi (`garages`) dan armada truk (`fleets`) milik driver beserta kondisi kesehatan, odometer, bahan bakar, plat nomor, dan badge **`IN USE`** pada armada yang sedang dikendarai.

### Phase 5: Deteksi Server, Pencatatan Job (`jobhistories`), Akumulasi Damage & Anti-Farming Cap
1. **Deteksi Server (`ServerDetector`)**:
   - Mendeteksi secara akurat apakah driver bermain di **`Singleplayer`**, **`SCS Convoy`**, **`TruckersMP`**, atau **`HaulMP`**.
   - Menggunakan deteksi modul DLL di memori (`core_ets2mp.dll` untuk TruckersMP; `haulmp_stub.dll` / `haulmp-telemetry.dll` untuk HaulMP) serta pembacaan terbalik (*reverse scan*) `game.log.txt` sehingga tidak salah mendeteksi jika driver sebelumnya pernah membuka mod lain.
2. **Siklus Hidup Job Otomatis (`ONGOING` $\rightarrow$ `COMPLETED` / `CANCELED`)**:
   - Saat mengambil job di game (`SCS_TELEMETRY_GAMEPLAY_EVENT_job_started`): plugin mengirim `POST /api/tracker/jobs/start` dan membuat dokumen `jobhistories` berstatus `"ONGOING"` dengan `jobId` sekuensial (`"1"`, `"2"`, ...).
   - Saat menyelesaikan job (`SCS_TELEMETRY_GAMEPLAY_EVENT_job_delivered`): plugin mengirim `POST /api/tracker/jobs/complete`, backend menghitung pendapatan **NC**, **EXP**, pajak, biaya bahan bakar, bonus on-time, meng-update kondisi & odometer armada di koleksi `fleets`, mencatat mutasi di `transactions`, dan memperbarui saldo wallet driver.
   - Saat membatalkan job (`SCS_TELEMETRY_GAMEPLAY_EVENT_job_cancelled`): plugin mengirim `POST /api/tracker/jobs/cancel` dan mengubah status dokumen menjadi `"CANCELED"`.
3. **Perhitungan Damage Akumulatif (Anti-Repair Reset)**:
   - Selama job berlangsung (`job_active == true`), setiap kenaikan wear pada 5 komponen truk (`engine`, `transmission`, `cabin`, `chassis`, `wheels`) serta `trailer` dan `cargo` diakumulasikan ke variabel `job_accum_*` (`src/core/scs_callbacks.cpp`).
   - Jika driver melakukan **repair di bengkel di tengah perjalanan**, nilai wear di game memang turun ke `0`, namun `job_accum_*` **TIDAK di-reset**. Jika setelah repair driver menabrak lagi, kenaikan damage baru langsung ditambahkan ke akumulasi sebelumnya.
4. **Pembatasan Jarak Maksimum `+25%` (`plannedDistanceKm` Cap)**:
   - Untuk mencegah eksploitasi *distance farming* (mengambil job jarak pendek lalu berputar-putar agar mendapat NC besar), baik di plugin (`src/core/scs_callbacks.cpp`) maupun di backend (`src/services/jobProcessor.js`), parameter `distanceKm` dibatasi maksimum **`125%` (`+25%`) dari `plannedDistanceKm`**:
     $$\text{effectiveDistanceKm} = \min\left(\text{drivenKm},\ \text{round}(\text{plannedDistanceKm} \times 1.25)\right)$$

### Phase 6: Aplikasi Penalty Points, Safebox Tickets & Diskon Kumulatif
1. **Aplikasi Penalty Points (`PhoneAppId::PenaltyPoints`)**:
   - Ikon Shield Alert Crimson di Home Screen iPhone 16.
   - Status bar penalti `0 – 50` poin dengan 4 tingkat bahaya (Aman, Penalty 1, Penalty 2, Penalty 3).
   - Modul tebus via NC dengan stepper poin dan total kalkulasi biaya.
   - Modul hapus penalti via tiket brankas garasi (`Safebox`).
   - Notifikasi toast hasil transaksi secara real-time.
2. **Diskon Kumulatif Penebusan NC**:
   - Server Booster: Diskon `500 NC`.
   - Nismara+ Aktif: Diskon `500 NC`.
   - Keduanya aktif: Diskon kumulatif **`1,000 NC`** (biaya tebus turun dari 3,000 NC menjadi 2,000 NC/poin).
3. **Endpoint Backend Khusus**:
   - `POST /api/tracker/points/pay`: Pemotongan atomik saldo NC, pengurangan `points.totalPoints`, pencatatan `pointhistories` & `currencyhistories`.
   - `POST /api/tracker/points/use-ticket`: Pemotongan stok tiket `garages.safeboxStock`, pengurangan `points.totalPoints`, pencatatan `pointhistories`.
4. **Perbaikan Tampilan My Garage & Nismara ID**:
   - `RenderGarageView`: Safebox menampilkan `X / Y Tiket Hapus Penalti` (bukan saldo NC) dengan tombol langsung menuju aplikasi Penalty Points.
   - `RenderNismaraIdView`: Mengoreksi label dari *Driver Points* menjadi *Poin Penalti* dengan indikator warna status.

### Phase 7: Perombakan Total Antarmuka 7 Aplikasi (Native iOS UI & Interactive Sub-Navigation)
1. **Penerapan Native Segmented Control (`RenderSegmentedControl`)**:
   - Seluruh 7 aplikasi kini memiliki arsitektur layout khas iOS mandiri yang unik, tidak lagi monoton.
2. **My Garage (`RenderAppGarage` & `RenderFleetDetailInspector`)**:
   - Sub-tab: `Overview`, `Armada (X)`, `Fasilitas`.
   - Setiap kartu kendaraan di tab `Armada` dapat diklik untuk masuk ke tampilan drill-down **Vehicle Detail Inspector**.
   - Inspector menampilkan spesifikasi teknis lengkap, status asuransi, live in-game vs database odometer, keausan komponen, tombol bind manual, serta **Jadwal & Target Servis Berkala (Countdown km)** untuk mesin (`maint_engine`), transmisi (`maint_transmission`), rem (`maint_brakes`), dan ban (`maint_tires`). Jika jarak tempuh melewati target, badge merah `LEWAT X km!` akan menyala.
   - Tombol navigasi atas berganti dinamis menjadi `< Garage` dan judul menjadi `Vehicle Detail`.
3. **Job (`RenderAppDispatch`)**:
   - Sub-tab: `Waybill Rute` & `Slip Paycheck`.
   - `Waybill Rute`: Menampilkan visual rute timeline (Origin $\rightarrow$ Destination) dengan indikator panah, jarak rencana, sisa navigasi real-time, manifest kargo, kuota cap jarak (+25%), serta akumulasi kerusakan dan statistik kecepatan.
   - `Slip Paycheck`: Menampilkan invoice/paystub digital resmi pekerjaan terakhir lengkap dengan pendapatan bersih NC, bonus multiplayer, hardcore parking, potongan pajak, service, dan denda poin penalti.
4. **Nismara ID (`RenderAppNismaraId`)**:
   - Desain **Apple Wallet Holographic Pass** dengan barcode digital prosedural, Discord ID, SteamID64, Trucky Role/Rank emblem dengan warna badge resmi.
   - Kartu Apple Card saldo Nismara Cash, progress EXP/Level driver, dan pill status pelanggaran.
5. **Telemetry (`RenderAppTelemetry`)**:
   - Sub-tab: `Cockpit`, `Powertrain`, `Diagnostik`.
   - Cockpit menghadirkan digital cluster speedometer besar, kotak transmisi `[D12]`/`[N]`/`[R1]`, cruise control, RPM bar dengan redline warning, serta kompas navigasi dan koordinat GPS dunia.
6. **Penalty Points (`RenderAppPenaltyPoints`)**:
   - Sub-tab: `Tebus NC`, `Tiket Garasi`, `Aturan`.
   - Gauge bahaya terintegrasi dengan stepper interaktif tebus NC (kalkulasi diskon otomatis) dan penggunaan tiket Safebox garasi.
7. **Wallpapers (`RenderAppWallpapers`)**:
   - Sub-tab: `Preset iOS 18`, `Foto Disk`, `Warna Casing`.
8. **Settings (`RenderAppSettings`)**:
   - Format iOS Inset Grouped Table dengan kelompok pengaturan perilaku, cloud endpoint switcher, token password masking, dan info sistem.

### Phase 8: Nismara Companion Desktop App, Win32 Mutex Gatekeeper & Auto-Updater
1. **Nismara Companion App (`nismara-companion/`)**:
   - **Framework**: Tauri v2 (`x86_64-pc-windows-msvc`) + React 19 + TypeScript + Vite + Tailwind CSS v4.
   - **UI Desktop Modern**:
     - *Header Quick Launch*: Menampilkan status deteksi Steam, status instalasi plugin `NismaraTracker.dll`, tombol `Install Plugin`, serta tombol `Launch ETS2` / `Launch ATS` (`steam://rungameid/227300` & `270880`).
     - *5 Tab Navigasi Utama*: **Dashboard**, **Job Waybill**, **My Garage** (lengkap dengan hitungan mundur target servis berkala), **Penalty Points** (tebus via saldo NC & gunakan tiket Safebox), dan **Settings** (pengaturan base URL, switch profile token, info hardware).
2. **Win32 Named Mutex Gatekeeper Protocol (`Global\NismaraCompanionRunning`)**:
   - **Tujuan**: Memastikan in-game overlay iPhone 16 **hanya dapat diakses jika dan hanya jika Nismara Companion Desktop App sedang berjalan di background**.
   - **Implementasi Rust (`src-tauri/src/lib.rs`)**:
     - Membuka Named Mutex `Global\NismaraCompanionRunning` (dengan fallback `Local\NismaraCompanionRunning`) menggunakan Win32 API (`CreateMutexW`).
     - Handle mutex disimpan di memori selama Companion App hidup (`CompanionMutexGuard`), dan dilepas secara otomatis saat aplikasi ditutup.
   - **Implementasi In-Game C++ (Non-Intrusive & Crash-Proof)**:
     - Plugin `NismaraTracker.dll` tetap ter-load aman bersama game tanpa memicu `FreeLibrary` atau `SCS_RESULT_unsupported` yang berisiko menyebabkan game crash seusai video intro.
     - Di dalam hook keyboard Windows ([`src/hooks/wndproc_hook.cpp`](file:///d:/Project/Nismara%20Transport/Project/nismara-tracker/src/hooks/wndproc_hook.cpp)): Setiap kali tombol `F11` ditekan, fungsi `IsCompanionAppRunning()` memeriksa keberadaan Mutex.
     - **Jika Companion TIDAK berjalan**: Tombol `F11` sepenuhnya dinonaktifkan (*disabled/dormant*), kursor game tidak dibebaskan, dan overlay iPhone 16 tidak pernah terbuka.
     - **Jika Companion SEDANG berjalan**: Tombol `F11` langsung aktif normal membuka iPhone 16 OS.
     - **Jika Companion ditutup saat HP sedang terbuka**: Di dalam rendering loop ([`src/ui/tablet_gui.cpp`](file:///d:/Project/Nismara%20Transport/Project/nismara-tracker/src/ui/tablet_gui.cpp)), sistem mendeteksi hilangnya Mutex dan langsung menutup tampilan HP secara otomatis.
3. **Ekosistem CI/CD & Auto-Updater via GitHub Releases (`.github/workflows/release.yml`)**:
   - Kompilasi otomatis multi-platform (Windows `.exe`, Linux `.AppImage`/`.deb`, macOS `.dmg`) dijalankan via **GitHub Actions** setiap kali git tag `v*` di-push atau dijalankan via `workflow_dispatch`.
   - Pada runner Windows, workflow secara otomatis mengompilasi `NismaraTracker.dll` (Release x64) terlebih dahulu dengan CMake & MSBuild, menyalinnya ke `resources/` paket installer Companion, dan mengunggahnya sebagai standalone asset di GitHub Release.
   - Endpoint auto-updater di `tauri.conf.json` diarahkan langsung ke GitHub Releases:
     `https://github.com/mhazm/fortune-cookie-client/releases/latest/download/latest.json`
   - Hal ini menghemat beban bandwidth server VPS secara drastis (di-hosting gratis dan berkecepatan tinggi oleh GitHub Global CDN) serta memudahkan 300+ driver menerima pembaruan instan.
   - Integritas biner pembaruan divalidasi dengan enkripsi Minisign (`pubkey` di `tauri.conf.json` dan `TAURI_SIGNING_PRIVATE_KEY` di GitHub Secrets).

### Phase 9: Auto-Updater Otomatis, In-Game Version Gatekeeper & Sinkronisasi Biner DLL
1. **Pemeriksaan Pembaruan Klien Desktop (`Nismara Companion`)**:
   - Terintegrasi langsung dengan `@tauri-apps/plugin-updater` dan `@tauri-apps/plugin-process`.
   - Setiap kali Nismara Companion dibuka, aplikasi secara otomatis memeriksa pembaruan via GitHub Releases (`latest.json`).
   - Tersedia tombol manual `Periksa Pembaruan` di footer bar dan modal dialog interaktif dengan catatan rilis (changelog), progress bar download real-time, dan tombol `Unduh & Perbarui Sekarang` (auto-relaunch).
2. **Deteksi & Auto-Sync Biner DLL In-Game (`NismaraTracker.dll`)**:
   - Companion membandingkan ukuran dan timestamp biner `NismaraTracker.dll` di folder game (`plugins/`) dengan biner yang dibundel di dalam aplikasi.
   - Jika biner di folder game usang (`plugin_outdated`), Companion menampilkan badge status dan tombol interaktif `Perbarui Plugin DLL` (Amber), serta secara otomatis menyinkronkannya saat startup selama game sedang tidak berjalan.
3. **Penegakan Versi Minimum Server (Server-Side Version Gatekeeper)**:
   - Server `nismara-tracker-server` memberlakukan variabel `MIN_TRACKER_VERSION` (default `"1.0.0"`).
   - Middleware `authTracker.js` memeriksa header `X-Tracker-Version`. Jika versi klien lebih rendah dari versi minimum:
     - Mengembalikan `HTTP 426 Upgrade Required` dengan kode error `"OUTDATED_VERSION"` dan panduan pembaruan.
     - Tracking telemetri dan job dari klien usang ditolak dengan aman tanpa menyebabkan data korup.
   - Endpoint publik `GET /api/tracker/version` disediakan untuk memverifikasi batas versi minimum dan URL rilis resmi.
4. **Peringatan In-Game Overlay iPhone 16 (`tablet_gui.cpp`)**:
   - Jika plugin mendeteksi penolakan `HTTP 426 / OUTDATED_VERSION` dari server:
     - Layar iPhone 16 in-game menampilkan banner peringatan merah mencolok di bagian atas layar:
       `! UPDATE DIPERLUKAN: Wajib v{min_ver} - Buka Nismara Companion`.
     - Pengemudi langsung mengetahui bahwa mereka perlu membuka Companion untuk memperbarui aplikasi.

### Phase 10: Spesifikasi Hardcore Mode, Validasi Parameter Game, & Kalkulasi Hardcore Points (HP)
1. **Definisi Hardcore Mode**:
   - Mode simulasi penuh di mana seluruh parameter realisme game wajib aktif, tanpa bantuan instan (*no auto-assist*), tanpa mod terlarang, dan dengan pengujian berkendara ketat.
2. **9 Parameter Konfigurasi Profil Game Wajib**:
   1. `g_traffic >= 1` (Traffic wajib ada, tidak boleh 0).
   2. `g_fatigue = 1` (Fatigue / Kelelahan pengemudi = Yes).
   3. `g_police = 1` (Traffic Offence / Tilang polisi = Yes).
   4. `g_simple_parking_doubles = 0` / Parking Difficulty = Skilled Parking (Parkir manual presisi tinggi).
   5. `g_road_events >= 0.1` (Random Road Events minimal 10%).
   6. `g_rain_prob >= 0.1` (Probabilitas hujan minimal 10%).
   7. `g_trailer_advanced_coupling = 1` (Advanced Trailer Coupling = Yes).
   8. `g_fuel_economy = 1` (Realistic Fuel Consumption = Yes).
   9. `g_air_brake = 1` (Air Brakes Simulation = Yes).
3. **Kategori Kecepatan Mengemudi (Speed Classification)**:
   - **Real Miles** ($\le 100\text{ km/h}$): Kecepatan simulasi realistis standar VTC Nismara Transport. Memenuhi syarat Hardcore Points dan tanpa penalti kecepatan.
   - **Race Miles** ($> 100\text{ km/h}$ dan $\le 180\text{ km/h}$): Melebihi batas kecepatan simulasi. Dikenakan penalti `+2` Poin Pelanggaran (`pointhistories`), pembatalan bonus Distance & Mass (`-100%`), dan diskualifikasi rating Hardcore.
   - **Invalid Stats** ($> 180\text{ km/h}$): Kecepatan tidak wajar / manipulasi physics / warping. Dikenakan penalti berat `+5` Poin Pelanggaran, pembatalan reward Hardcore total, dan rating otomatis 0 bintang.

4. **Kriteria Validasi Kelayakan Hardcore (Semua Wajib Terpenuhi)**:
   - Kecepatan berada dalam kategori **Real Miles** ($\le 100\text{ km/h}$).
   - Seluruh 9 setting konfigurasi profil di atas terkonfigurasi dengan benar.
   - Tidak menggunakan Auto Load (*No Auto Load* — dikecualikan untuk Freight Market, Public Transport System, dan WOTR Freight Market).
   - Parkir Manual (*Manual Parking* — event `auto_parking_used == false`).
   - Jarak tempuh aktual di atas 250 km (`distanceKm >= 250`).
   - Tidak ada manipulasi waktu / warp (*No Warping* — rasio waktu in-game vs wall-clock valid, warp value tidak berubah).
   - Tidak menggunakan Damage Mod (kerusakan fisik aktif & logis).
   - Massa kargo lebih dari 0 ton (`cargoMassTons > 0`).
   - Bahan bakar terpakai lebih dari 0 liter (`fuelUsedLiters > 0`).
   - Tidak ada Forbidden Mods yang terdeteksi di `game.log.txt`.
   - Tidak melakukan rute berulang secara eksploitatif (*No Repetitive Route*).
   - Massa kargo masuk akal (*Reasonable Cargo Mass*).
5. **Rumus Perhitungan Hardcore Points (HP)**:
   $$\text{HP} = 0.4 \times \text{Distance} + 0.35 \times \text{Mass} - 0.4 \times \text{Fuel Economy} - 1.0 \times \text{Damage Percentage} - 5.0 \times \text{Number of Fines}$$
   - *Non SCS Stock Cargo*: Nilai bonus massa kargo tetap bernilai `5.25`.
6. **Penalti Pengurangan HP**:
   - *Late Delivery*: Penalti `-50%` dari total bonus Distance dan Mass.
   - *Rest Time Stop Overdue*: Melewati batas istirahat > 30 menit waktu game $\rightarrow$ Penalti `-50%` dari total bonus Distance dan Mass.
   - *Race / Invalid Stats*: Pengurangan total bonus Distance dan Mass (`-100%`).
   - *Job Canceled*: Pengurangan total bonus Distance dan Mass (`-100%`).
   - *Speeding (Batas > 40 km/h, toleransi 5 km/h)*: Maksimal waktu mengebut yang diizinkan adalah `10%` dari Real-Time driving. Setiap 1 menit mengebut di atas batas tersebut dikenakan penalti `-5%` dari bonus Distance dan Mass.
7. **Delivery Rating Bintang (0 hingga 5 Bintang)**:
   - **Distance Rating**: `<250km`: 0★, `250-499km`: 1★, `500-749km`: 2★, `750-999km`: 3★, `1000-1249km`: 4★, `≥1250km`: 5★.
   - **Mass Rating**: `<10t`: 0★, `10-14.99t`: 1★, `15-19.99t`: 2★, `20-24.99t`: 3★, `25-29.99t`: 4★, `≥30t`: 5★ (Non SCS Stock Cargo = 1★).
   - **Damage Rating**: `0%`: 5★, `1-10%`: 4★, `11-20%`: 3★, `21-30%`: 2★, `31-40%`: 1★, `>40%`: 0★.
   - **Fuel Economy Rating**: Ideal ETS2 = 38 L/100km, ATS = 42 L/100km.
     $$\text{Stars} = \text{clamp}\left(5 - \frac{\text{FuelEconomy} - \text{Ideal}}{\text{Ideal}},\ 0,\ 5\right)$$
   - **Fines Rating**: `0 fines`: 5★, `1 fine`: 4★, `2 fines`: 3★, `3 fines`: 2★, `4 fines`: 1★, `≥5 fines`: 0★.
   - **Penalti Bintang Akhir**:
     - Late Delivery: `-2 Bintang`.
     - Rest Time Overdue: `-2 Bintang`.
     - Job Canceled / Race Stats / Invalid Stats: `0 Bintang (Diskualifikasi Total)`.
8. **Deteksi Forbidden Mods**:
   - Memeriksa daftar mod aktif di `game.log.txt` yang mengubah: damage calculation, fuel consumption, tollgates, tank size, speed limits, delivery time, sleeping time, bypass fines, atau economy mod.
9. **Integrasi Antarmuka In-Game iPhone 16 (`tablet_gui.cpp`)**:
   - *Settings App*: Dilengkapi kartu **DIAGNOSTIK SIMULASI HARDCORE** dengan checklist 9 parameter status `[OK]` / `[X]`, status mod game, dan tombol interaktif `Pindai Ulang Profil Game (Rescan)` yang dapat dipicu kapan saja tanpa restart game.
   - *Job Dispatch - Waybill*: Menampilkan klasifikasi kecepatan real-time (`Real Miles` hijau, `Race Miles` crimson, `Invalid Stats` merah), serta badge status kelayakan `[★ SIMULASI HARDCORE: ELIGIBLE]` atau `[SIMULASI STANDAR]`.
   - *Job Dispatch - Slip Paycheck*: Menampilkan total **Hardcore Points (`+X.X HP`)**, kalkulasi **5-Star Delivery Rating (`★ ★ ★ ★ ☆`)**, serta rincian alasan diskualifikasi jika kriteria tidak terpenuhi.

### Phase 11: Integrasi Berita Sanity CMS di Nismara Companion Desktop App
1. **Live Fetching dari Sanity CMS**:
   - Mengambil artikel resmi Nismara Group langsung dari **Sanity Edge CDN** (`projectId: yi1d1k89`, `dataset: production`, `apiVersion: 2024-03-15`) dengan GROQ query berurutan `publishedAt desc`.
   - Dilengkapi mekanisme fallback ke REST API publik `https://nismara.web.id/api/articles`.
2. **Koleksi & Penyajian Media Resolusi Tinggi**:
   - Transformasi referensi asset `image-<hash>-<width>x<height>-<ext>` ke URL Sanity Image CDN global (`https://cdn.sanity.io/images/yi1d1k89/production/...`).
   - Rasio 16:9 dengan overlay badge kategori dinamis (`Update`, `News`, `Transport`, dll.).
3. **Fitur Antarmuka Tab News**:
   - Filter interaktif berbasis kategori dinamis (pills) yang diekstrak otomatis dari metadata artikel.
   - Indikator loading skeleton dan tombol manual `Segarkan Berita` (`RefreshCw`).
   - Modal Reader pop-up (`Nismara Gazette`) di dalam aplikasi untuk membaca artikel lengkap tanpa harus keluar dari aplikasi.
   - Integrasi tombol `Buka di Web` dengan `@tauri-apps/plugin-opener` (`openUrl`) untuk membuka tautan di browser default sistem.

### Phase 12: Penggantian Tab Map Menjadi Events & Agregasi Multi-Koleksi MongoDB
1. **Agregasi Multi-Koleksi MongoDB di Backend (`/api/tracker/events`)**:
   - Penggantian placeholder tab Map di desktop companion dengan pusat informasi kegiatan VTC (**Events**).
   - Backend `nismara-tracker-server` mengagregasi 4 koleksi MongoDB:
     - `contracts`: Kontrak pengiriman khusus perusahaan (*Special Contract*), kalkulasi akumulasi muatan, jarak, dan reward NC.
     - `coupons`: Kupon hadiah & tiket penalty/NC reward bagi driver (`codeCoupon`), batas klaim, dan tanggal kedaluwarsa.
     - `giveaways`: Quest berhadiah & undian tiket berhadiah dengan rincian tier hadiah serta daftar tugas/syarat.
     - `ncevents`: Event pengali koin NC resmi komunitas (*Multiplier Boost Event*).
   - Format respons terstandarisasi dengan endpoint `GET /api/tracker/events` yang menyajikan ringkasan metrik statistik serta timeline feed kronologis.
2. **Antarmuka Interaktif Tab Events di Nismara Companion Desktop App**:
   - Ikon navigasi `Trophy` menggantikan `MapIcon` pada sidebar desktop app.
   - Filter pills kategori (`Semua`, `Kontrak`, `Kupon Hadiah`, `Giveaway Quest`, `NC Boost`).
   - Kartu event responsif dengan banner visual, badge status (`Aktif`, `Selesai`, `Terjadwal`), countdown/tanggal berakhir, dan preview spesifik (tombol salin kode kupon sekali-klik dengan feedback `Tersalin!`, ringkasan quest giveaway, dan statistik muatan kontrak).
   - Modal inspektor mendalam (*Event Detail Modal*) untuk melihat seluruh detail kontrak, rincian quest giveaway & tier hadiah, serta syarat partisipasi.

---

## 6. Konteks Produksi Skala Besar (~300+ Pengguna), Cross-Platform, & Server 24/7 (PRIORITAS UTAMA!)

Proyek ini digunakan oleh sekitar **300+ pengemudi aktif VTC Nismara Transport** dengan spesifikasi perangkat, sistem operasi, dan struktur penyimpanan yang sangat beragam. Seluruh desain dan kode di `nismara-tracker`, `nismara-companion`, dan `nismara-tracker-server` **WAJIB** mematuhi tiga pilar produksi berikut:

### A. Skala Multi-User (~300+ Orang) & Direktori Dinamis (Zero Hardcoded Paths/Tokens)
1. **Tidak Boleh Ada Path atau Token Hardcoded Milik Developer**:
   - Setiap pengguna memiliki lokasi instalasi Steam, library game (`C:\`, `D:\`, `E:\`, `/home/user/.steam`, `/Users/user/Library/Application Support/Steam`, dll.), serta lokasi folder profil (`Documents\Euro Truck Simulator 2` atau custom `-homedir`) yang berbeda-beda.
   - Jangan pernah menjadikan token personal developer (`NSM-...`) atau path lokal developer (seperti `F:\SteamLibrary\...`) sebagai nilai default di UI Companion maupun C++ plugin. Nilai default token dan custom path override harus kosong (`""`) untuk pengguna baru.
2. **Resolusi Direktori Otomatis, Penyimpanan di `Documents\<Game>`, & Kemudahan Akses `game.sii`**:
   - `Nismara Companion` wajib mendeteksi lokasi instalasi game secara otomatis melalui parsing `libraryfolders.vdf` lintas drive/partisi, serta menyediakan opsi pemilihan folder manual bagi pengguna dengan instalasi kustom.
   - Seluruh file konfigurasi dan data lokal (`nismara_config.dat`, folder `wallpapers/`, dan file heartbeat gatekeeper) **TIDAK** disimpan di dalam folder `bin/win_x64/plugins/` game agar folder `plugins/` tetap bersih (hanya berisi `NismaraTracker.dll`) dan bebas dari masalah izin tulis Administrator (`Program Files`).
   - Sebaliknya, seluruh file tersebut disimpan terpusat di direktori data dokumen game pengguna:
     - **Windows & Steam Proton**: `Documents\Euro Truck Simulator 2\` dan `Documents\American Truck Simulator\` (atau direktori `-homedir` jika pengguna memakai parameter launch `-homedir`).
     - **Linux Native**: `~/.local/share/Euro Truck Simulator 2/` dan `~/.local/share/American Truck Simulator/`.
     - **macOS**: `~/Library/Application Support/Euro Truck Simulator 2/` dan `~/Library/Application Support/American Truck Simulator/`.
   - Penempatan terpusat di folder dokumen game ini juga memberikan akses langsung satu pintu ke `game.log.txt`, `config.cfg`, serta `profiles/` & `steam_profiles/` (`save/.../game.sii`) untuk pengembangan fitur lanjutan ke depannya.
   - Saat pengguna mengisi `Driver Tracker Token` dan `Server Backend URL` di `Nismara Companion`, konfigurasi tersebut langsung disinkronkan secara otomatis ke `Documents\Euro Truck Simulator 2\nismara_config.dat` dan `Documents\American Truck Simulator\nismara_config.dat`.

### B. Dukungan Lintas Sistem Operasi (Windows, Linux, dan macOS)
Tidak semua anggota komunitas menggunakan Windows. Ekosistem Nismara harus mengutamakan kompatibilitas untuk **Windows**, **Linux**, dan **macOS**:
1. **Nismara Companion (`nismara-companion` - Tauri v2)**:
   - Kode Rust di `src-tauri/src/lib.rs` dan dependensi di `Cargo.toml` wajib memisahkan API spesifik OS menggunakan conditional compilation (`#[cfg(target_os = "windows")]`, `#[cfg(target_os = "linux")]`, `#[cfg(target_os = "macos")]`).
   - **Deteksi Steam per OS**:
     - **Windows**: Registry `HKCU\Software\Valve\Steam` / `HKLM\SOFTWARE\WOW6432Node\Valve\Steam` + `libraryfolders.vdf`.
     - **Linux**: `~/.steam/steam`, `~/.local/share/Steam`, dan Flatpak (`~/.var/app/com.valvesoftware.Steam/.local/share/Steam`) + `libraryfolders.vdf`.
     - **macOS**: `~/Library/Application Support/Steam` + `libraryfolders.vdf`.
   - **Peluncuran Game (`steam://rungameid/<id>`)**: Menggunakan `cmd /c start` di Windows, `xdg-open` di Linux, dan `open` di macOS (atau `tauri-plugin-opener`).
2. **In-Game Telemetry Plugin (`nismara-tracker`)**:
   - **Linux via Steam Proton / Wine**: Banyak pengguna Linux menjalankan ETS2/ATS versi Windows melalui Proton (DXVK $\rightarrow$ DirectX 11). Agar `NismaraTracker.dll` berjalan mulus di dalam Proton/Wine:
     - Pembuatan font di `tablet_gui.cpp` (`LoadTrueTypeFonts`) wajib memiliki fallback font multi-path (termasuk font bawaan Wine/Linux atau font default Dear ImGui) jika `Segoe UI` tidak tersedia di prefix Wine.
     - Mekanisme **Gatekeeper** tidak boleh hanya mengandalkan Win32 Named Mutex (`Global\NismaraCompanionRunning`), karena proses native Linux `Nismara Companion` terpisah dari namespace Win32 Mutex di dalam container Wine/Proton. Wajib dilengkapi dengan **Cross-Platform Gatekeeper Heartbeat File** (misalnya file lock/heartbeat bertimestamp yang diperbarui oleh Companion) sebagai pendamping Win32 Mutex.
   - **Linux Native (`.so`) & macOS Native (`.dylib`)**: Sistem build `CMakeLists.txt` dan abstraksi platform harus disiapkan agar dapat dikompilasi menjadi `NismaraTracker.dll` (`bin/win_x64/plugins/`), `libNismaraTracker.so` (`bin/linux_x64/plugins/`), dan `libNismaraTracker.dylib` (`Euro Truck Simulator 2.app/Contents/MacOS/plugins/`).

### C. Server Produksi 24/7 (`nismara-tracker-server`)
1. **Berjalan 24/7 dengan IP / Domain Publik Sendiri**:
   - Saat development, `nismara-tracker-server` berjalan di `http://localhost:3100`.
   - Saat production, server ini berjalan **24/7 di VPS/Dedicated Server khusus** yang memiliki IP publik atau domain sendiri untuk melayani ~300+ pengguna secara bersamaan.
2. **Keamanan Autentikasi Multi-User Ketat (Tanpa Localhost Bypass)**:
   - Di server 24/7 (terutama jika berada di balik reverse proxy seperti Nginx, Caddy, atau Cloudflare di mana `req.ip` terlihat sebagai `127.0.0.1`), middleware autentikasi (`src/middleware/authTracker.js`) **DILARANG KERAS** menyimpan variabel global `lastAuthenticatedUser` atau melakukan fallback ke user pertama di database ketika token tidak cocok!
   - Setiap request HTTP dari plugin/companion wajib diautentikasi secara independen dan terisolasi menggunakan `Authorization: Bearer <trackerToken>` atau `x-tracker-token` milik masing-masing pengguna.
3. **Pengecualian IP/Port Tracker Server dari Deteksi Socket Multiplayer**:
   - Di `src/core/server_detector.cpp`, saat memindai koneksi TCP aktif milik proses game untuk mendeteksi SCS Convoy / Multiplayer, host dan port dari `api_base_url` (`nismara-tracker-server`) wajib dikecualikan agar koneksi HTTP keep-alive ke server tracker 24/7 tidak salah terdeteksi sebagai sesi multiplayer.

---

## 7. Cara Build, Deploy, dan Menjalankan Proyek

### A. Build & Deploy `NismaraTracker.dll`
Jalankan perintah PowerShell berikut dari direktori `d:\Project\Nismara Transport\Project\nismara-tracker`:
```powershell
cmake --build build --config Release
Copy-Item -Path "bin\NismaraTracker.dll" -Destination "C:\Program Files (x86)\Steam\steamapps\common\Euro Truck Simulator 2\bin\win_x64\plugins\NismaraTracker.dll" -Force
```
*(Catatan: Jika game ETS2 sedang berjalan dan me-lock file `NismaraTracker.dll`, tutup game terlebih dahulu sebelum menjalankan `Copy-Item`).*

### B. Menjalankan `nismara-tracker-server`
Jalankan perintah PowerShell berikut dari direktori `D:\Project\Nismara Transport\Project\nismara-tracker-server`:
```powershell
node server.js
```
Saat development server berjalan di `http://localhost:3100` (endpoint health check: `GET http://localhost:3100/health`), dan saat production berjalan 24/7 pada IP/domain server publik yang dikonfigurasi.

### C. Menjalankan & Membangun `Nismara Companion`
Jalankan perintah berikut dari direktori `d:\Project\Nismara Transport\Project\nismara-tracker\nismara-companion`:
- **Mode Development (Hot-Reload)**:
  ```powershell
  npm run tauri dev
  ```
- **Kompilasi Standalone Release Installer (`.exe` / `.msi` di Windows, `.AppImage` / `.deb` di Linux, `.dmg` / `.app` di macOS)**:
  ```powershell
  npm run tauri build
  ```
  Binary hasil kompilasi akan berada di folder `src-tauri/target/release/` dan file setup installer di `src-tauri/target/release/bundle/`.

### C. Menjalankan & Memelihara Dedicated Tracker Server (`nismara-tracker-server`)
Jalankan perintah berikut dari direktori `D:\Project\Nismara Transport\Project\nismara-tracker-server`:
- **Mode Development (Hot-Reload)**:
  ```bash
  npm run dev
  ```
- **Mode Production (Node.js Direct)**:
  ```bash
  npm start
  ```
- **Mode Production 24/7 (PM2 Process Manager)**:
  Menggunakan `ecosystem.config.cjs` (format standar CommonJS untuk Node.js ESM) atau `ecosystem.js`:
  ```bash
  # Menjalankan server di background dengan PM2
  npm run pm2:start

  # Memantau log real-time
  npm run pm2:logs

  # Restart / Zero-downtime reload
  npm run pm2:restart
  npm run pm2:reload

  # Menghentikan server
  npm run pm2:stop

  # Membuka terminal dashboard CPU / Memory
  npm run pm2:monit
  ```
  Fitur otomatis di PM2 ecosystem: Auto-restart jika terjadi *unhandled crash*, batasan memori maksimum `500MB` (`max_memory_restart`), serta pencatatan log dengan timestamp terformat di folder `logs/pm2-error.log` dan `logs/pm2-out.log`.

---

## 8. Aturan Penting: DO & DON'T (Guardrails untuk AI Agent)

### DO (Yang Harus Dilakukan)
- **DO**: Pertahankan arsitektur UI in-game **100% Native C++ Dear ImGui (`src/ui/tablet_gui.cpp`)** yang ringan dan cepat tanpa ketergantungan CEF Browser atau WebSocket streaming.
- **DO**: Pertahankan kompilasi **single-binary (`NismaraTracker.dll` / `.so` / `.dylib`)** dengan static runtime (`/MT` pada MSVC). Semua aset visual bawaan (seperti logo atau ikon) harus digambar secara prosedural dengan `ImDrawList` atau di-embed sebagai array byte C++ (seperti `include/ui/nismara_logo_data.hpp`).
- **DO**: Utamakan kompatibilitas **Cross-Platform (Windows, Linux Proton/Native, dan macOS)** serta skala **~300+ pengguna** dengan direktori yang berbeda-beda di setiap perubahan kode.
- **DO**: Pertahankan protokol **Gatekeeper (`Global\NismaraCompanionRunning` + Cross-Platform Heartbeat)** sehingga plugin in-game hanya dapat membuka overlay jika Nismara Companion Desktop App sedang berjalan.
- **DO**: Selalu gunakan komunikasi HTTP asynchronous di thread terpisah (`ApiClient`) untuk seluruh request jaringan dari C++. Jangan pernah melakukan jaringan sinkron di dalam thread DirectX `Present` atau callback SCS SDK.
- **DO**: Pastikan setiap modifikasi UI di `src/ui/tablet_gui.cpp` tetap berada di dalam batas layar OLED iPhone 16 (`screen_min` hingga `screen_max`) dan sebelum pemanggilan `RenderPhoneBezelOverlay` agar tidak ada elemen visual yang bocor keluar dari bezel HP.
- **DO**: Gunakan `String(user.discordId)` dan `guildId: "863959415702028318"` saat melakukan query ke koleksi `currencies` dan `points`, serta `ObjectId(user._id)` saat melakukan query ke koleksi `fleets` dan `garages`.
- **DO**: Pertahankan mekanisme akumulasi damage (`job_accum_*`) dan batas maksimum jarak tempuh `+25%` dari `plannedDistanceKm` baik di sisi client C++ maupun di sisi server Node.js.
- **DO**: Jika memperbarui aset logo dari file SVG, gunakan skrip rasterisasi supersampling (seperti yang menghasilkan `include/ui/nismara_logo_data.hpp`) agar kurva bezier dan `fill-rule="evenodd"` tetap tajam tanpa artefak.

### DON'T (Yang Dilarang Dilakukan)
- **DON'T**: **Jangan pernah** menggunakan CEF Browser (`CefSharp`) atau WebSocket streaming untuk merender UI `tablet_gui`, karena akan membengkakkan ukuran instalasi ratusan megabyte dan menguras resource CPU/RAM pengguna.
- **DON'T**: **Jangan pernah** meng-hardcode path direktori lokal developer (seperti `F:\SteamLibrary\...`) atau token personal developer (`NSM-...`) sebagai default di kode client maupun companion app.
- **DON'T**: **Jangan pernah** menggunakan fallback `lastAuthenticatedUser` atau bypass autentikasi `isLocalRequest` di `nismara-tracker-server`, karena pada server produksi 24/7 yang melayani ~300 pengguna (terutama di balik reverse proxy), hal tersebut akan menyebabkan kebocoran/pencampuran sesi antar-driver.
- **DON'T**: **Jangan pernah** mengarahkan polling telemetri atau pengiriman job langsung ke domain Vercel (`beta.nismara.web.id` / website utama) secara terus-menerus. Selalu gunakan `nismara-tracker-server` mandiri (24/7) agar kuota serverless Vercel aman dari pembengkakan biaya (*overcost*).
- **DON'T**: **Jangan pernah** mengubah `CMAKE_MSVC_RUNTIME_LIBRARY` menjadi `/MD` (Dynamic DLL) pada build Windows, karena akan menyebabkan plugin gagal dimuat pada komputer driver yang belum menginstal VC++ Redistributable terbaru.
- **DON'T**: **Jangan pernah** menghapus atau melewati pemeriksaan Gatekeeper di `wndproc_hook.cpp` atau `tablet_gui.cpp`.
- **DON'T**: **Jangan pernah** melupakan pelepasan (`Release()`) `ID3D11RenderTargetView` di dalam hook `ResizeBuffers` (`src/hooks/dx11_hook.cpp`). Jika RTV tidak dilepas sebelum `ResizeBuffers` asli dipanggil, game ETS2/ATS akan langsung *crash* saat pengguna melakukan Alt-Tab atau mengubah resolusi layar.
- **DON'T**: **Jangan pernah** menggunakan karakter non-ASCII / simbol Unicode eksotis yang tidak didukung oleh subset glyph default Dear ImGui (`Segoe UI` ASCII/Latin-1 range) di dalam string UI C++, karena akan tampil sebagai tanda tanya (`?`). Gunakan teks bersih atau ikon vektor `ImDrawList`.
- **DON'T**: **Jangan pernah** mengandalkan pembacaan baris pertama hingga akhir pada `game.log.txt` untuk mendeteksi server multiplayer tanpa memprioritaskan modul DLL di memori (`haulmp_stub.dll` vs `core_ets2mp.dll`) dan urutan baris log paling bawah (*reverse scan*).

