use serde::{Deserialize, Serialize};
use std::ffi::OsStr;
use std::fs;
use std::os::windows::ffi::OsStrExt;
use std::path::{Path, PathBuf};
use tauri::Manager;
use winreg::enums::*;
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
    pub is_running: bool,
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct SystemStatus {
    pub companion_version: String,
    pub steam_installed: bool,
    pub steam_path: String,
    pub games: Vec<GameInfo>,
    pub gatekeeper_active: bool,
}

pub struct CompanionMutexGuard {
    handle: windows_sys::Win32::Foundation::HANDLE,
}

unsafe impl Send for CompanionMutexGuard {}
unsafe impl Sync for CompanionMutexGuard {}

impl Drop for CompanionMutexGuard {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                windows_sys::Win32::Foundation::CloseHandle(self.handle);
            }
        }
    }
}

fn create_companion_mutex() -> Option<CompanionMutexGuard> {
    let global_name: Vec<u16> = OsStr::new("Global\\NismaraCompanionRunning")
        .encode_wide()
        .chain(std::iter::once(0))
        .collect();

    let handle = unsafe {
        windows_sys::Win32::System::Threading::CreateMutexW(
            std::ptr::null_mut(),
            0,
            global_name.as_ptr(),
        )
    };

    if !handle.is_null() {
        return Some(CompanionMutexGuard { handle });
    }

    let local_name: Vec<u16> = OsStr::new("Local\\NismaraCompanionRunning")
        .encode_wide()
        .chain(std::iter::once(0))
        .collect();

    let local_handle = unsafe {
        windows_sys::Win32::System::Threading::CreateMutexW(
            std::ptr::null_mut(),
            0,
            local_name.as_ptr(),
        )
    };

    if !local_handle.is_null() {
        Some(CompanionMutexGuard { handle: local_handle })
    } else {
        None
    }
}

fn get_steam_path() -> Option<PathBuf> {
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

fn is_process_running(exe_name: &str) -> bool {
    use windows_sys::Win32::System::Diagnostics::ToolHelp::{
        CreateToolhelp32Snapshot, Process32FirstW, Process32NextW, PROCESSENTRY32W, TH32CS_SNAPPROCESS,
    };
    use windows_sys::Win32::Foundation::CloseHandle;

    unsafe {
        let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if snapshot.is_null() || snapshot == -1isize as _ {
            return false;
        }

        let mut entry: PROCESSENTRY32W = std::mem::zeroed();
        entry.dwSize = std::mem::size_of::<PROCESSENTRY32W>() as u32;

        let target_lower = exe_name.to_lowercase();
        let mut found = false;

        if Process32FirstW(snapshot, &mut entry) != 0 {
            loop {
                let name_len = entry
                    .szExeFile
                    .iter()
                    .position(|&c| c == 0)
                    .unwrap_or(entry.szExeFile.len());
                let process_name = String::from_utf16_lossy(&entry.szExeFile[..name_len]).to_lowercase();

                if process_name == target_lower {
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

fn detect_games(
    libraries: &[PathBuf],
    custom_ets2_path: Option<&str>,
    custom_ats_path: Option<&str>,
) -> Vec<GameInfo> {
    let game_targets = [
        ("ets2", "Euro Truck Simulator 2", 227300u32, "eurotrucks2.exe", custom_ets2_path),
        ("ats", "American Truck Simulator", 270880u32, "amtrucks.exe", custom_ats_path),
    ];

    let mut result = Vec::new();

    for (id, name, app_id, exe_name, custom_path_opt) in game_targets {
        let mut installed = false;
        let mut game_root = PathBuf::new();
        let mut exe_file = PathBuf::new();
        let mut plugin_dir = PathBuf::new();
        let mut plugin_file = PathBuf::new();
        let mut plugin_found = false;

        // 1. Direct manual check if user specified custom directory
        if let Some(custom) = custom_path_opt {
            let trimmed = custom.trim();
            if !trimmed.is_empty() {
                let candidate_dir = PathBuf::from(trimmed);
                let cand_exe1 = candidate_dir.join("bin").join("win_x64").join(exe_name);
                let cand_exe2 = candidate_dir.join(exe_name);

                if cand_exe1.exists() {
                    installed = true;
                    game_root = candidate_dir;
                    exe_file = cand_exe1;
                    plugin_dir = game_root.join("bin").join("win_x64").join("plugins");
                    plugin_file = plugin_dir.join("NismaraTracker.dll");
                    plugin_found = plugin_file.exists();
                } else if cand_exe2.exists() {
                    installed = true;
                    game_root = candidate_dir
                        .parent()
                        .and_then(|p| p.parent())
                        .map(Path::to_path_buf)
                        .unwrap_or_else(|| candidate_dir.clone());
                    exe_file = cand_exe2;
                    plugin_dir = candidate_dir.join("plugins");
                    plugin_file = plugin_dir.join("NismaraTracker.dll");
                    plugin_found = plugin_file.exists();
                }
            }
        }

        // 2. Fallback: check auto-detected libraries
        if !installed {
            for lib in libraries {
                let candidate_root = lib.join("steamapps").join("common").join(name);
                let candidate_exe = candidate_root.join("bin").join("win_x64").join(exe_name);

                if candidate_exe.exists() {
                    installed = true;
                    game_root = candidate_root;
                    exe_file = candidate_exe;
                    plugin_dir = game_root.join("bin").join("win_x64").join("plugins");
                    plugin_file = plugin_dir.join("NismaraTracker.dll");
                    plugin_found = plugin_file.exists();
                    break;
                }
            }
        }

        let is_running = if installed {
            is_process_running(exe_name)
        } else {
            false
        };

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
            is_running,
        });
    }

    result
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
            companion_version: "1.0.0".to_string(),
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

        let custom_ets2 = if game_id == "ets2" { custom_game_path.as_deref() } else { None };
        let custom_ats = if game_id == "ats" { custom_game_path.as_deref() } else { None };

        let games = detect_games(&libraries, custom_ets2, custom_ats);

        let target_game = games
            .into_iter()
            .find(|g| g.id == game_id)
            .ok_or_else(|| format!("Game '{}' belum terdeteksi. Silakan tentukan direktori game di Settings.", game_id))?;

        if !target_game.is_installed {
            return Err(format!("Direktori {} tidak valid atau executable game tidak ditemukan.", target_game.name));
        }

        // Candidates for source NismaraTracker.dll
        let candidate_sources = [
            PathBuf::from("../bin/NismaraTracker.dll"),
            PathBuf::from("bin/NismaraTracker.dll"),
            PathBuf::from("../../bin/NismaraTracker.dll"),
            PathBuf::from("NismaraTracker.dll"),
            PathBuf::from("resources/NismaraTracker.dll"),
            PathBuf::from("../resources/NismaraTracker.dll"),
        ];

        let mut source_dll = None;
        for cand in &candidate_sources {
            if cand.exists() {
                source_dll = Some(cand.clone());
                break;
            }
        }

        // Fallback: check current directory of companion app & resources subfolder
        if source_dll.is_none() {
            if let Ok(exe_path) = std::env::current_exe() {
                if let Some(parent) = exe_path.parent() {
                    let candidate1 = parent.join("NismaraTracker.dll");
                    let candidate2 = parent.join("resources").join("NismaraTracker.dll");
                    if candidate1.exists() {
                        source_dll = Some(candidate1);
                    } else if candidate2.exists() {
                        source_dll = Some(candidate2);
                    }
                }
            }
        }

        let src = source_dll.ok_or_else(|| {
            "Source binary 'NismaraTracker.dll' tidak ditemukan dalam paket companion.".to_string()
        })?;

        let dest_dir = PathBuf::from(&target_game.plugin_dir);
        if !dest_dir.exists() {
            fs::create_dir_all(&dest_dir)
                .map_err(|e| format!("Gagal membuat direktori plugins: {}", e))?;
        }

        let dest_file = PathBuf::from(&target_game.plugin_path);
        fs::copy(&src, &dest_file).map_err(|e| {
            format!(
                "Gagal menyalin NismaraTracker.dll (game mungkin sedang berjalan dan me-lock file): {}",
                e
            )
        })?;

        Ok(format!(
            "Berhasil memasang NismaraTracker.dll ke {}!",
            target_game.name
        ))
    })
    .await
    .map_err(|e| format!("Install plugin error: {}", e))?
}

#[tauri::command]
fn launch_game(game_id: String) -> Result<String, String> {
    let app_id = match game_id.as_str() {
        "ets2" => 227300u32,
        "ats" => 270880u32,
        _ => return Err("Unsupported game identifier".to_string()),
    };

    let steam_uri = format!("steam://rungameid/{}", app_id);
    std::process::Command::new("cmd")
        .args(["/c", "start", &steam_uri])
        .spawn()
        .map_err(|e| format!("Failed to launch game via Steam: {}", e))?;

    Ok(format!("Sent launch command to Steam for App ID {}", app_id))
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let mutex_guard = create_companion_mutex();

    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_process::init())
        .plugin(tauri_plugin_updater::Builder::new().build())
        .setup(|app| {
            if let Some(guard) = mutex_guard {
                app.manage(guard);
            }
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            get_system_status,
            install_plugin,
            launch_game
        ])
        .run(tauri::generate_context!())
        .expect("error while running Nismara Companion");
}
