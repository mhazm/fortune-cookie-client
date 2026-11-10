use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc,
};
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use tauri::Manager;

#[cfg(target_os = "windows")]
use std::ffi::OsStr;
#[cfg(target_os = "windows")]
use std::os::windows::ffi::OsStrExt;
#[cfg(target_os = "windows")]
use winreg::enums::*;
#[cfg(target_os = "windows")]
use winreg::RegKey;

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct GameInfo {
    pub id: String,
    pub name: String,
    pub steam_app_id: u32,
    pub is_installed: bool,
    pub game_path: String,
    pub exe_path: String,
    pub plugin_dir: String,
    pub plugin_installed: bool,
    pub plugin_path: String,
    pub user_data_dir: String,
    pub config_path: String,
    pub is_running: bool,
    pub plugin_outdated: bool,
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct SystemStatus {
    pub companion_version: String,
    pub os_platform: String,
    pub steam_installed: bool,
    pub steam_path: String,
    pub games: Vec<GameInfo>,
    pub gatekeeper_active: bool,
}

pub const DEFAULT_API_BASE_URL: &str = "http://103.150.101.154:3100";

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct PluginConfigPayload {
    pub driver_token: String,
    pub api_base_url: String,
    pub ets2_config_path: String,
    pub ats_config_path: String,
}

pub struct CompanionGatekeeperGuard {
    #[cfg(target_os = "windows")]
    handle: windows_sys::Win32::Foundation::HANDLE,
    stop_flag: Arc<AtomicBool>,
}

unsafe impl Send for CompanionGatekeeperGuard {}
unsafe impl Sync for CompanionGatekeeperGuard {}

impl Drop for CompanionGatekeeperGuard {
    fn drop(&mut self) {
        self.stop_flag.store(true, Ordering::SeqCst);
        cleanup_heartbeat_files();
        #[cfg(target_os = "windows")]
        {
            if !self.handle.is_null() {
                unsafe {
                    windows_sys::Win32::Foundation::CloseHandle(self.handle);
                }
            }
        }
    }
}

fn get_home_dir() -> Option<PathBuf> {
    #[cfg(target_os = "windows")]
    {
        if let Ok(up) = std::env::var("USERPROFILE") {
            let p = PathBuf::from(up);
            if p.exists() {
                return Some(p);
            }
        }
    }
    if let Ok(home) = std::env::var("HOME") {
        let p = PathBuf::from(home);
        if p.exists() {
            return Some(p);
        }
    }
    None
}

fn get_primary_game_user_dir(game_id: &str) -> PathBuf {
    let folder_name = if game_id == "ats" {
        "American Truck Simulator"
    } else {
        "Euro Truck Simulator 2"
    };

    if let Some(home) = get_home_dir() {
        #[cfg(target_os = "windows")]
        {
            let docs = home.join("Documents");
            if docs.exists() {
                return docs.join(folder_name);
            }
            let onedrive_docs = home.join("OneDrive").join("Documents");
            if onedrive_docs.exists() {
                return onedrive_docs.join(folder_name);
            }
            return docs.join(folder_name);
        }

        #[cfg(target_os = "macos")]
        {
            return home
                .join("Library")
                .join("Application Support")
                .join(folder_name);
        }

        #[cfg(not(any(target_os = "windows", target_os = "macos")))]
        {
            if let Ok(xdg) = std::env::var("XDG_DATA_HOME") {
                let xdg_path = PathBuf::from(xdg);
                if xdg_path.exists() {
                    return xdg_path.join(folder_name);
                }
            }
            return home.join(".local").join("share").join(folder_name);
        }
    }

    PathBuf::from(folder_name)
}

/// Returns all candidate user data directories for a game (including Steam Proton prefixes on Linux)
fn get_all_game_user_dirs(game_id: &str, libraries: &[PathBuf]) -> Vec<PathBuf> {
    let mut dirs = vec![get_primary_game_user_dir(game_id)];
    let folder_name = if game_id == "ats" {
        "American Truck Simulator"
    } else {
        "Euro Truck Simulator 2"
    };
    let app_id = if game_id == "ats" { "270880" } else { "227300" };

    // On Linux, if the user runs the Windows build of ETS2/ATS under Steam Proton,
    // the game's Documents folder lives inside compatdata/<app_id>/pfx/drive_c/users/steamuser/Documents/<GameName>
    for lib in libraries {
        let proton_docs = lib
            .join("steamapps")
            .join("compatdata")
            .join(app_id)
            .join("pfx")
            .join("drive_c")
            .join("users")
            .join("steamuser")
            .join("Documents")
            .join(folder_name);
        if proton_docs.exists() && !dirs.contains(&proton_docs) {
            dirs.push(proton_docs);
        }
    }

    dirs
}

fn write_heartbeat_files() {
    let steam_opt = get_steam_path();
    let libraries = steam_opt
        .as_ref()
        .map(|sp| get_all_steam_library_folders(sp))
        .unwrap_or_default();

    let ts = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0);
    let payload = format!("alive={}\npid={}\n", ts, std::process::id());

    for game_id in ["ets2", "ats"] {
        for dir in get_all_game_user_dirs(game_id, &libraries) {
            let _ = fs::create_dir_all(&dir);
            let hb = dir.join("nismara_companion_alive.lock");
            let _ = fs::write(&hb, &payload);
        }
    }
}

fn cleanup_heartbeat_files() {
    let steam_opt = get_steam_path();
    let libraries = steam_opt
        .as_ref()
        .map(|sp| get_all_steam_library_folders(sp))
        .unwrap_or_default();

    for game_id in ["ets2", "ats"] {
        for dir in get_all_game_user_dirs(game_id, &libraries) {
            let hb = dir.join("nismara_companion_alive.lock");
            let _ = fs::remove_file(&hb);
        }
    }
}

fn create_companion_gatekeeper() -> CompanionGatekeeperGuard {
    let stop_flag = Arc::new(AtomicBool::new(false));
    let thread_stop = Arc::clone(&stop_flag);

    // Ensure runtime & web-ui are synced to %LOCALAPPDATA%\Nismara Companion
    sync_bundled_runtime_to_local_appdata();

    // Immediately write initial heartbeat files and refresh every 4 seconds
    write_heartbeat_files();
    std::thread::spawn(move || {
        while !thread_stop.load(Ordering::Relaxed) {
            write_heartbeat_files();
            std::thread::sleep(Duration::from_secs(4));
        }
    });

    #[cfg(target_os = "windows")]
    {
        let global_name: Vec<u16> = OsStr::new("Global\\NismaraCompanionRunning")
            .encode_wide()
            .chain(std::iter::once(0))
            .collect();

        let mut handle = unsafe {
            windows_sys::Win32::System::Threading::CreateMutexW(
                std::ptr::null_mut(),
                0,
                global_name.as_ptr(),
            )
        };

        if handle.is_null() {
            let local_name: Vec<u16> = OsStr::new("Local\\NismaraCompanionRunning")
                .encode_wide()
                .chain(std::iter::once(0))
                .collect();

            handle = unsafe {
                windows_sys::Win32::System::Threading::CreateMutexW(
                    std::ptr::null_mut(),
                    0,
                    local_name.as_ptr(),
                )
            };
        }

        CompanionGatekeeperGuard { handle, stop_flag }
    }

    #[cfg(not(target_os = "windows"))]
    {
        CompanionGatekeeperGuard { stop_flag }
    }
}

fn get_steam_path() -> Option<PathBuf> {
    #[cfg(target_os = "windows")]
    {
        let hkcu = RegKey::predef(HKEY_CURRENT_USER);
        if let Ok(steam_key) = hkcu.open_subkey("Software\\Valve\\Steam") {
            if let Ok(path_str) = steam_key.get_value::<String, _>("SteamPath") {
                let p = PathBuf::from(path_str);
                if p.exists() {
                    return Some(p);
                }
            }
        }

        let hklm = RegKey::predef(HKEY_LOCAL_MACHINE);
        if let Ok(steam_key) = hklm.open_subkey("SOFTWARE\\WOW6432Node\\Valve\\Steam") {
            if let Ok(path_str) = steam_key.get_value::<String, _>("InstallPath") {
                let p = PathBuf::from(path_str);
                if p.exists() {
                    return Some(p);
                }
            }
        }
    }

    #[cfg(target_os = "macos")]
    {
        if let Some(home) = get_home_dir() {
            let p = home.join("Library").join("Application Support").join("Steam");
            if p.exists() {
                return Some(p);
            }
        }
    }

    #[cfg(not(any(target_os = "windows", target_os = "macos")))]
    {
        if let Some(home) = get_home_dir() {
            let candidates = [
                home.join(".steam").join("steam"),
                home.join(".local").join("share").join("Steam"),
                home.join(".var")
                    .join("app")
                    .join("com.valvesoftware.Steam")
                    .join(".local")
                    .join("share")
                    .join("Steam"),
            ];
            for cand in candidates {
                if cand.exists() {
                    return Some(cand);
                }
            }
        }
    }

    None
}

fn get_all_steam_library_folders(steam_path: &Path) -> Vec<PathBuf> {
    let mut libraries = vec![steam_path.to_path_buf()];
    let vdf_path = steam_path.join("steamapps").join("libraryfolders.vdf");

    if let Ok(content) = fs::read_to_string(&vdf_path) {
        for line in content.lines() {
            let trimmed = line.trim();
            if trimmed.starts_with("\"path\"") {
                let parts: Vec<&str> = trimmed.split('"').collect();
                if parts.len() >= 4 {
                    let clean = parts[3].replace("\\\\", "\\");
                    let lib_dir = PathBuf::from(clean);
                    if lib_dir.exists() && !libraries.contains(&lib_dir) {
                        libraries.push(lib_dir);
                    }
                }
            }
        }
    }

    libraries
}

fn is_process_running(exe_stem: &str) -> bool {
    #[cfg(target_os = "windows")]
    {
        use windows_sys::Win32::Foundation::CloseHandle;
        use windows_sys::Win32::System::Diagnostics::ToolHelp::{
            CreateToolhelp32Snapshot, Process32FirstW, Process32NextW, PROCESSENTRY32W,
            TH32CS_SNAPPROCESS,
        };

        unsafe {
            let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if snapshot.is_null() || snapshot == -1isize as _ {
                return false;
            }

            let mut entry: PROCESSENTRY32W = std::mem::zeroed();
            entry.dwSize = std::mem::size_of::<PROCESSENTRY32W>() as u32;

            let target_win = format!("{}.exe", exe_stem).to_lowercase();
            let target_raw = exe_stem.to_lowercase();
            let mut found = false;

            if Process32FirstW(snapshot, &mut entry) != 0 {
                loop {
                    let name_len = entry
                        .szExeFile
                        .iter()
                        .position(|&c| c == 0)
                        .unwrap_or(entry.szExeFile.len());
                    let process_name =
                        String::from_utf16_lossy(&entry.szExeFile[..name_len]).to_lowercase();

                    if process_name == target_win || process_name == target_raw {
                        found = true;
                        break;
                    }

                    if Process32NextW(snapshot, &mut entry) == 0 {
                        break;
                    }
                }
            }

            CloseHandle(snapshot);
            found
        }
    }

    #[cfg(not(target_os = "windows"))]
    {
        if let Ok(output) = std::process::Command::new("pgrep")
            .args(["-i", exe_stem])
            .output()
        {
            return output.status.success();
        }
        false
    }
}

/// Migrates any legacy `nismara_config.dat` and `wallpapers/` out of `plugins/` into `Documents/<Game>/`
fn migrate_legacy_plugin_files(plugin_dir: &Path, user_data_dir: &Path) {
    if !plugin_dir.exists() || plugin_dir == user_data_dir {
        return;
    }

    let _ = fs::create_dir_all(user_data_dir);
    let legacy_cfg = plugin_dir.join("nismara_config.dat");
    let target_cfg = user_data_dir.join("nismara_config.dat");

    if legacy_cfg.exists() {
        if !target_cfg.exists() {
            let _ = fs::copy(&legacy_cfg, &target_cfg);
        }
        let _ = fs::remove_file(&legacy_cfg);
    }

    let legacy_wallpapers = plugin_dir.join("wallpapers");
    let target_wallpapers = user_data_dir.join("wallpapers");
    if legacy_wallpapers.exists() && legacy_wallpapers.is_dir() {
        let _ = fs::create_dir_all(&target_wallpapers);
        if let Ok(entries) = fs::read_dir(&legacy_wallpapers) {
            for entry in entries.flatten() {
                let dest = target_wallpapers.join(entry.file_name());
                if !dest.exists() {
                    let _ = fs::copy(entry.path(), dest);
                }
            }
        }
        let _ = fs::remove_dir_all(&legacy_wallpapers);
    }
}

/// Checks a candidate game installation root for Windows/Proton (.exe), Linux Native, or macOS (.app)
fn probe_game_directory(candidate: &Path, app_name: &str, exe_stem: &str) -> Option<(PathBuf, PathBuf, PathBuf, PathBuf)> {
    let win_exe_name = format!("{}.exe", exe_stem);

    // 1. Windows / Steam Proton layout: <root>/bin/win_x64/<exe>.exe
    let win_exe = candidate.join("bin").join("win_x64").join(&win_exe_name);
    if win_exe.exists() {
        let p_dir = candidate.join("bin").join("win_x64").join("plugins");
        let p_file = p_dir.join("NismaraTracker.dll");
        return Some((candidate.to_path_buf(), win_exe, p_dir, p_file));
    }

    // 2. Direct bin/win_x64 folder selected by user
    let direct_win_exe = candidate.join(&win_exe_name);
    if direct_win_exe.exists() {
        let root = candidate
            .parent()
            .and_then(|p| p.parent())
            .map(Path::to_path_buf)
            .unwrap_or_else(|| candidate.to_path_buf());
        let p_dir = candidate.join("plugins");
        let p_file = p_dir.join("NismaraTracker.dll");
        return Some((root, direct_win_exe, p_dir, p_file));
    }

    // 3. Linux Native layout: <root>/bin/linux_x64/<exe>
    let linux_exe = candidate.join("bin").join("linux_x64").join(exe_stem);
    if linux_exe.exists() {
        let p_dir = candidate.join("bin").join("linux_x64").join("plugins");
        let p_file = p_dir.join("libNismaraTracker.so");
        return Some((candidate.to_path_buf(), linux_exe, p_dir, p_file));
    }

    // 4. macOS Native layout: <root>/<AppName>.app/Contents/MacOS/<exe>
    let mac_app_dir = candidate
        .join(format!("{}.app", app_name))
        .join("Contents")
        .join("MacOS");
    let mac_exe = mac_app_dir.join(exe_stem);
    if mac_exe.exists() {
        let p_dir = mac_app_dir.join("plugins");
        let p_file = p_dir.join("libNismaraTracker.dylib");
        return Some((candidate.to_path_buf(), mac_exe, p_dir, p_file));
    }

    None
}

fn copy_dir_recursive(src: &Path, dst: &Path) {
    if !src.exists() {
        return;
    }
    let _ = fs::create_dir_all(dst);
    if let Ok(entries) = fs::read_dir(src) {
        for entry in entries.flatten() {
            let src_path = entry.path();
            let dst_path = dst.join(entry.file_name());
            if src_path.is_dir() {
                copy_dir_recursive(&src_path, &dst_path);
            } else {
                let should_copy = match (fs::metadata(&src_path), fs::metadata(&dst_path)) {
                    (Ok(s_meta), Ok(d_meta)) => {
                        let newer = match (s_meta.modified(), d_meta.modified()) {
                            (Ok(s_mod), Ok(d_mod)) => s_mod
                                .duration_since(d_mod)
                                .map(|d| d.as_secs() > 2)
                                .unwrap_or(false),
                            _ => false,
                        };
                        s_meta.len() != d_meta.len() || newer
                    }
                    (Ok(_), Err(_)) => true,
                    _ => false,
                };
                if should_copy {
                    let _ = fs::copy(&src_path, &dst_path);
                }
            }
        }
    }
}

/// Ensures %LOCALAPPDATA%\Nismara Companion\runtime & web-ui are always populated
/// from the bundled resources directory, even if the user installed Nismara Companion
/// to a custom drive or directory, or after deleting AppData for a fresh reinstall.
fn sync_bundled_runtime_to_local_appdata() {
    #[cfg(target_os = "windows")]
    {
        if let Ok(local_appdata) = std::env::var("LOCALAPPDATA") {
            let companion_appdata = PathBuf::from(local_appdata).join("Nismara Companion");
            let _ = fs::create_dir_all(&companion_appdata);

            if let Ok(exe_path) = std::env::current_exe() {
                if let Some(exe_dir) = exe_path.parent() {
                    // 1. Sync runtime (Ultralight WebKit)
                    let runtime_candidates = [
                        exe_dir.join("resources").join("runtime"),
                        exe_dir.join("resources").join("resources").join("runtime"),
                        exe_dir.join("runtime"),
                        exe_dir.join("..").join("resources").join("runtime"),
                        exe_dir.join("..").join("src-tauri").join("resources").join("runtime"),
                    ];
                    let target_runtime = companion_appdata.join("runtime");
                    for cand in &runtime_candidates {
                        if cand.exists() && cand != &target_runtime {
                            copy_dir_recursive(cand, &target_runtime);
                            break;
                        }
                    }

                    // 2. Sync web-ui (React 19 Phone UI)
                    let webui_candidates = [
                        exe_dir.join("resources").join("web-ui"),
                        exe_dir.join("resources").join("resources").join("web-ui"),
                        exe_dir.join("web-ui"),
                        exe_dir.join("..").join("resources").join("web-ui"),
                        exe_dir.join("..").join("src-tauri").join("resources").join("web-ui"),
                    ];
                    let target_webui = companion_appdata.join("web-ui");
                    for cand in &webui_candidates {
                        if cand.exists() && cand != &target_webui {
                            copy_dir_recursive(cand, &target_webui);
                            break;
                        }
                    }

                    // 3. Sync NismaraTracker.dll to %LOCALAPPDATA%\Nismara Companion\resources\
                    let dll_candidates = [
                        exe_dir.join("resources").join("NismaraTracker.dll"),
                        exe_dir.join("resources").join("resources").join("NismaraTracker.dll"),
                        exe_dir.join("NismaraTracker.dll"),
                        exe_dir.join("..").join("resources").join("NismaraTracker.dll"),
                        exe_dir.join("..").join("src-tauri").join("resources").join("NismaraTracker.dll"),
                        exe_dir.join("..").join("bin").join("NismaraTracker.dll"),
                        exe_dir.join("..").join("..").join("bin").join("NismaraTracker.dll"),
                    ];
                    let target_res = companion_appdata.join("resources");
                    let target_dll = target_res.join("NismaraTracker.dll");
                    for cand in &dll_candidates {
                        if cand.exists() && cand != &target_dll {
                            let _ = fs::create_dir_all(&target_res);
                            let should_copy = match (fs::metadata(cand), fs::metadata(&target_dll)) {
                                (Ok(s_meta), Ok(d_meta)) => s_meta.len() != d_meta.len(),
                                (Ok(_), Err(_)) => true,
                                _ => false,
                            };
                            if should_copy {
                                let _ = fs::copy(cand, &target_dll);
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
}

fn find_bundled_plugin_source(binary_name: &str) -> Option<PathBuf> {
    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(parent) = exe_path.parent() {
            let exe_candidates = [
                parent.join("resources").join(binary_name),
                parent.join("_up_").join("resources").join(binary_name),
                parent.join(binary_name),
                parent.join("..").join("resources").join(binary_name),
                parent.join("..").join("bin").join(binary_name),
            ];

            for cand in &exe_candidates {
                if cand.exists() {
                    return Some(cand.clone());
                }
            }
        }
    }

    let candidate_sources = [
        PathBuf::from(format!("resources/{}", binary_name)),
        PathBuf::from(format!("_up_/resources/{}", binary_name)),
        PathBuf::from(format!("../resources/{}", binary_name)),
        PathBuf::from(format!("../../resources/{}", binary_name)),
        PathBuf::from(format!("../bin/{}", binary_name)),
        PathBuf::from(format!("bin/{}", binary_name)),
        PathBuf::from(format!("../../bin/{}", binary_name)),
        PathBuf::from(binary_name),
    ];

    for cand in &candidate_sources {
        if cand.exists() {
            return Some(cand.clone());
        }
    }

    None
}

fn detect_games(
    libraries: &[PathBuf],
    custom_ets2_path: Option<&str>,
    custom_ats_path: Option<&str>,
) -> Vec<GameInfo> {
    let game_targets = [
        ("ets2", "Euro Truck Simulator 2", 227300u32, "eurotrucks2", custom_ets2_path),
        ("ats", "American Truck Simulator", 270880u32, "amtrucks", custom_ats_path),
    ];

    let mut result = Vec::new();

    for (id, name, app_id, exe_stem, custom_path_opt) in game_targets {
        let mut installed = false;
        let mut game_root = PathBuf::new();
        let mut exe_file = PathBuf::new();
        let mut plugin_dir = PathBuf::new();
        let mut plugin_file = PathBuf::new();
        let mut plugin_found = false;

        let user_data_dir = get_primary_game_user_dir(id);
        let config_path = user_data_dir.join("nismara_config.dat");

        // 1. Check user-specified custom directory if provided
        if let Some(custom) = custom_path_opt {
            let trimmed = custom.trim();
            if !trimmed.is_empty() {
                let candidate_dir = PathBuf::from(trimmed);
                if let Some((root, exe, p_dir, p_file)) =
                    probe_game_directory(&candidate_dir, name, exe_stem)
                {
                    installed = true;
                    game_root = root;
                    exe_file = exe;
                    plugin_dir = p_dir;
                    plugin_file = p_file;
                    plugin_found = plugin_file.exists();
                }
            }
        }

        // 2. Auto-detect across all Steam library folders (libraryfolders.vdf)
        if !installed {
            for lib in libraries {
                let candidate_root = lib.join("steamapps").join("common").join(name);
                if let Some((root, exe, p_dir, p_file)) =
                    probe_game_directory(&candidate_root, name, exe_stem)
                {
                    installed = true;
                    game_root = root;
                    exe_file = exe;
                    plugin_dir = p_dir;
                    plugin_file = p_file;
                    plugin_found = plugin_file.exists();
                    break;
                }
            }
        }

        if installed {
            migrate_legacy_plugin_files(&plugin_dir, &user_data_dir);
        }

        let is_running = if installed {
            is_process_running(exe_stem)
        } else {
            false
        };

        let binary_name = plugin_file
            .file_name()
            .and_then(|n| n.to_str())
            .unwrap_or("NismaraTracker.dll");

        let mut plugin_outdated = false;
        if plugin_found {
            if let Some(src) = find_bundled_plugin_source(binary_name) {
                if let (Ok(src_meta), Ok(dst_meta)) =
                    (fs::metadata(&src), fs::metadata(&plugin_file))
                {
                    let is_newer = match (src_meta.modified(), dst_meta.modified()) {
                        (Ok(s_mod), Ok(d_mod)) => s_mod
                            .duration_since(d_mod)
                            .map(|d| d.as_secs() > 2)
                            .unwrap_or(false),
                        _ => false,
                    };
                    if src_meta.len() != dst_meta.len() || is_newer {
                        plugin_outdated = true;
                    }
                }
            }
        }

        result.push(GameInfo {
            id: id.to_string(),
            name: name.to_string(),
            steam_app_id: app_id,
            is_installed: installed,
            game_path: game_root.to_string_lossy().to_string(),
            exe_path: exe_file.to_string_lossy().to_string(),
            plugin_dir: plugin_dir.to_string_lossy().to_string(),
            plugin_installed: plugin_found,
            plugin_path: plugin_file.to_string_lossy().to_string(),
            user_data_dir: user_data_dir.to_string_lossy().to_string(),
            config_path: config_path.to_string_lossy().to_string(),
            is_running,
            plugin_outdated,
        });
    }

    result
}

fn read_kv_config(path: &Path) -> BTreeMap<String, String> {
    let mut map = BTreeMap::new();
    if let Ok(content) = fs::read_to_string(path) {
        for line in content.lines() {
            let trimmed = line.trim();
            if trimmed.is_empty() || trimmed.starts_with('#') || trimmed.starts_with(';') {
                continue;
            }
            if let Some(sep) = trimmed.find('=') {
                let k = trimmed[..sep].trim().to_string();
                let v = trimmed[sep + 1..].trim().to_string();
                map.insert(k, v);
            }
        }
    }
    map
}

fn write_kv_config(path: &Path, driver_token: &str, api_base_url: &str) -> Result<(), String> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)
            .map_err(|e| format!("Gagal membuat folder konfigurasi {:?}: {}", parent, e))?;
        let wallpapers = parent.join("wallpapers");
        let _ = fs::create_dir_all(wallpapers);
    }

    let mut existing = read_kv_config(path);
    let clean_url = api_base_url.trim().trim_end_matches('/');
    let final_url = if clean_url.is_empty()
        || clean_url == "http://localhost:3100"
        || clean_url == "http://localhost:3000"
        || clean_url == "http://127.0.0.1:3100"
    {
        DEFAULT_API_BASE_URL
    } else {
        clean_url
    };

    existing.insert("driver_token".to_string(), driver_token.trim().to_string());
    existing.insert("api_base_url".to_string(), final_url.to_string());

    let steam_id = existing.get("steam_id").cloned().unwrap_or_default();
    let hotkey = existing
        .get("toggle_hotkey_vk")
        .cloned()
        .unwrap_or_else(|| "122".to_string());
    let start_vis = existing
        .get("overlay_start_visible")
        .cloned()
        .unwrap_or_else(|| "0".to_string());
    let lock_wall = existing
        .get("lockscreen_wallpaper")
        .cloned()
        .unwrap_or_else(|| "preset:ultramarine".to_string());
    let home_wall = existing
        .get("homescreen_wallpaper")
        .cloned()
        .unwrap_or_else(|| "preset:midnight".to_string());
    let frame_col = existing
        .get("phone_frame_color")
        .cloned()
        .unwrap_or_else(|| "0".to_string());
    let auto_lock = existing
        .get("auto_lock_on_close")
        .cloned()
        .unwrap_or_else(|| "1".to_string());

    let serialized = format!(
        "# Nismara OS iPhone 16 Simulation Unit Configuration\n\
         driver_token={}\n\
         api_base_url={}\n\
         steam_id={}\n\
         toggle_hotkey_vk={}\n\
         overlay_start_visible={}\n\
         lockscreen_wallpaper={}\n\
         homescreen_wallpaper={}\n\
         phone_frame_color={}\n\
         auto_lock_on_close={}\n",
        driver_token.trim(),
        final_url,
        steam_id,
        hotkey,
        start_vis,
        lock_wall,
        home_wall,
        frame_col,
        auto_lock
    );

    fs::write(path, serialized)
        .map_err(|e| format!("Gagal menyimpan {:?}: {}", path, e))
}

#[tauri::command]
async fn get_plugin_config() -> Result<PluginConfigPayload, String> {
    tauri::async_runtime::spawn_blocking(move || {
        let ets2_dir = get_primary_game_user_dir("ets2");
        let ats_dir = get_primary_game_user_dir("ats");
        let ets2_cfg = ets2_dir.join("nismara_config.dat");
        let ats_cfg = ats_dir.join("nismara_config.dat");

        let mut map = read_kv_config(&ets2_cfg);
        if map.is_empty() {
            map = read_kv_config(&ats_cfg);
        }

        let driver_token = map.get("driver_token").cloned().unwrap_or_default();
        let raw_url = map.get("api_base_url").cloned().unwrap_or_default();
        let clean_raw = raw_url.trim().trim_end_matches('/');
        let api_base_url = if clean_raw.is_empty()
            || clean_raw == "http://localhost:3100"
            || clean_raw == "http://localhost:3000"
            || clean_raw == "http://127.0.0.1:3100"
        {
            DEFAULT_API_BASE_URL.to_string()
        } else {
            clean_raw.to_string()
        };

        PluginConfigPayload {
            driver_token,
            api_base_url,
            ets2_config_path: ets2_cfg.to_string_lossy().to_string(),
            ats_config_path: ats_cfg.to_string_lossy().to_string(),
        }
    })
    .await
    .map_err(|e| format!("Failed to read plugin config: {}", e))
}

#[tauri::command]
async fn save_plugin_config(
    driver_token: String,
    api_base_url: String,
) -> Result<PluginConfigPayload, String> {
    tauri::async_runtime::spawn_blocking(move || {
        let steam_opt = get_steam_path();
        let libraries = steam_opt
            .as_ref()
            .map(|sp| get_all_steam_library_folders(sp))
            .unwrap_or_default();

        let clean_url = api_base_url.trim().trim_end_matches('/');
        let final_api_url = if clean_url.is_empty()
            || clean_url == "http://localhost:3100"
            || clean_url == "http://localhost:3000"
            || clean_url == "http://127.0.0.1:3100"
        {
            DEFAULT_API_BASE_URL.to_string()
        } else {
            clean_url.to_string()
        };

        for game_id in ["ets2", "ats"] {
            for dir in get_all_game_user_dirs(game_id, &libraries) {
                let cfg_path = dir.join("nismara_config.dat");
                write_kv_config(&cfg_path, &driver_token, &final_api_url)?;
            }
        }

        let ets2_cfg = get_primary_game_user_dir("ets2").join("nismara_config.dat");
        let ats_cfg = get_primary_game_user_dir("ats").join("nismara_config.dat");

        Ok(PluginConfigPayload {
            driver_token: driver_token.trim().to_string(),
            api_base_url: final_api_url,
            ets2_config_path: ets2_cfg.to_string_lossy().to_string(),
            ats_config_path: ats_cfg.to_string_lossy().to_string(),
        })
    })
    .await
    .map_err(|e| format!("Failed to save plugin config: {}", e))?
}

#[tauri::command]
async fn get_system_status(
    custom_ets2_path: Option<String>,
    custom_ats_path: Option<String>,
) -> Result<SystemStatus, String> {
    tauri::async_runtime::spawn_blocking(move || {
        let steam_path_opt = get_steam_path();
        let steam_installed = steam_path_opt.is_some();
        let steam_path_str = steam_path_opt
            .as_ref()
            .map(|p| p.to_string_lossy().to_string())
            .unwrap_or_default();

        let libraries = if let Some(ref sp) = steam_path_opt {
            get_all_steam_library_folders(sp)
        } else {
            Vec::new()
        };

        let games = detect_games(
            &libraries,
            custom_ets2_path.as_deref(),
            custom_ats_path.as_deref(),
        );

        SystemStatus {
            companion_version: env!("CARGO_PKG_VERSION").to_string(),
            os_platform: std::env::consts::OS.to_string(),
            steam_installed,
            steam_path: steam_path_str,
            games,
            gatekeeper_active: true,
        }
    })
    .await
    .map_err(|e| format!("Failed to query system status: {}", e))
}

#[tauri::command]
async fn install_plugin(
    game_id: String,
    custom_game_path: Option<String>,
) -> Result<String, String> {
    tauri::async_runtime::spawn_blocking(move || {
        let steam_path_opt = get_steam_path();
        let libraries = if let Some(ref sp) = steam_path_opt {
            get_all_steam_library_folders(sp)
        } else {
            Vec::new()
        };

        let custom_ets2 = if game_id == "ets2" {
            custom_game_path.as_deref()
        } else {
            None
        };
        let custom_ats = if game_id == "ats" {
            custom_game_path.as_deref()
        } else {
            None
        };

        let games = detect_games(&libraries, custom_ets2, custom_ats);

        let target_game = games
            .into_iter()
            .find(|g| g.id == game_id)
            .ok_or_else(|| {
                format!(
                    "Game '{}' belum terdeteksi. Silakan tentukan direktori game di Settings.",
                    game_id
                )
            })?;

        if !target_game.is_installed {
            return Err(format!(
                "Direktori {} tidak valid atau executable game tidak ditemukan.",
                target_game.name
            ));
        }

        let target_plugin_file = PathBuf::from(&target_game.plugin_path);
        let binary_name = target_plugin_file
            .file_name()
            .and_then(|n| n.to_str())
            .unwrap_or("NismaraTracker.dll");

        let src = find_bundled_plugin_source(binary_name).ok_or_else(|| {
            format!(
                "Source binary '{}' tidak ditemukan dalam paket companion.",
                binary_name
            )
        })?;

        let dest_dir = PathBuf::from(&target_game.plugin_dir);
        if !dest_dir.exists() {
            fs::create_dir_all(&dest_dir)
                .map_err(|e| format!("Gagal membuat direktori plugins: {}", e))?;
        }

        fs::copy(&src, &target_plugin_file).map_err(|e| {
            format!(
                "Gagal menyalin {} (game mungkin sedang berjalan dan me-lock file): {}",
                binary_name, e
            )
        })?;

        // Ensure any old config/wallpapers in plugins/ are migrated to Documents/<Game>/
        let user_data_dir = PathBuf::from(&target_game.user_data_dir);
        migrate_legacy_plugin_files(&dest_dir, &user_data_dir);

        Ok(format!(
            "Berhasil memasang {} ke {}!",
            binary_name, target_game.name
        ))
    })
    .await
    .map_err(|e| format!("Install plugin error: {}", e))?
}

#[tauri::command]
async fn auto_sync_plugins(
    custom_ets2_path: Option<String>,
    custom_ats_path: Option<String>,
) -> Result<Vec<String>, String> {
    tauri::async_runtime::spawn_blocking(move || {
        let steam_path_opt = get_steam_path();
        let libraries = if let Some(ref sp) = steam_path_opt {
            get_all_steam_library_folders(sp)
        } else {
            Vec::new()
        };

        let games = detect_games(
            &libraries,
            custom_ets2_path.as_deref(),
            custom_ats_path.as_deref(),
        );

        let mut reports = Vec::new();

        for game in games {
            if !game.is_installed {
                continue;
            }

            let binary_name = Path::new(&game.plugin_path)
                .file_name()
                .and_then(|n| n.to_str())
                .unwrap_or("NismaraTracker.dll");

            if let Some(src) = find_bundled_plugin_source(binary_name) {
                if game.is_running {
                    if game.plugin_outdated {
                        reports.push(format!("{}: Game sedang berjalan (tutup game untuk memperbarui {}).", game.name, binary_name));
                    }
                    continue;
                }

                if !game.plugin_installed || game.plugin_outdated {
                    let dest_dir = PathBuf::from(&game.plugin_dir);
                    let dest_file = PathBuf::from(&game.plugin_path);
                    if let Err(e) = fs::create_dir_all(&dest_dir) {
                        reports.push(format!("{}: Gagal membuat folder plugins: {}", game.name, e));
                        continue;
                    }

                    match fs::copy(&src, &dest_file) {
                        Ok(_) => {
                            let user_data_dir = PathBuf::from(&game.user_data_dir);
                            migrate_legacy_plugin_files(&dest_dir, &user_data_dir);
                            reports.push(format!("{}: Plugin {} otomatis diperbarui ke versi terbaru!", game.name, binary_name));
                        }
                        Err(e) => {
                            reports.push(format!("{}: Gagal menyalin {}: {}", game.name, binary_name, e));
                        }
                    }
                }
            }
        }

        Ok(reports)
    })
    .await
    .map_err(|e| format!("Auto sync error: {}", e))?
}

#[tauri::command]
fn launch_game(game_id: String) -> Result<String, String> {
    let app_id = match game_id.as_str() {
        "ets2" => 227300u32,
        "ats" => 270880u32,
        _ => return Err("Unsupported game identifier".to_string()),
    };

    let steam_uri = format!("steam://rungameid/{}", app_id);

    #[cfg(target_os = "windows")]
    {
        std::process::Command::new("cmd")
            .args(["/c", "start", &steam_uri])
            .spawn()
            .map_err(|e| format!("Failed to launch game via Steam: {}", e))?;
    }

    #[cfg(target_os = "macos")]
    {
        std::process::Command::new("open")
            .arg(&steam_uri)
            .spawn()
            .map_err(|e| format!("Failed to launch game via Steam: {}", e))?;
    }

    #[cfg(not(any(target_os = "windows", target_os = "macos")))]
    {
        std::process::Command::new("xdg-open")
            .arg(&steam_uri)
            .spawn()
            .map_err(|e| format!("Failed to launch game via Steam: {}", e))?;
    }

    Ok(format!("Sent launch command to Steam for App ID {}", app_id))
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let gatekeeper_guard = create_companion_gatekeeper();

    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_process::init())
        .plugin(tauri_plugin_updater::Builder::new().build())
        .setup(|app| {
            app.manage(gatekeeper_guard);
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            get_system_status,
            install_plugin,
            auto_sync_plugins,
            launch_game,
            get_plugin_config,
            save_plugin_config
        ])
        .run(tauri::generate_context!())
        .expect("error while running Nismara Companion");
}
