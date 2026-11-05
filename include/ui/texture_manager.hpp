#pragma once

#include <d3d11.h>
#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

namespace nismara::ui {

struct WallpaperTexture {
    ID3D11ShaderResourceView* srv{nullptr};
    int width{0};
    int height{0};
    std::string loaded_spec{};
    std::string last_error{};

    void Release() noexcept {
        if (srv != nullptr) {
            srv->Release();
            srv = nullptr;
        }
        width = 0;
        height = 0;
    }
};

/**
 * @brief Manages DirectX 11 ShaderResourceView textures for both procedural iOS 18
 *        preset wallpapers and custom Lock/Home Screen image files (via Windows WIC).
 */
class TextureManager {
public:
    static TextureManager& Instance();

    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    /**
     * @brief Binds the active DirectX 11 device used to create 2D textures.
     */
    void SetDevice(ID3D11Device* device);

    /**
     * @brief Releases all DX11 ShaderResourceViews prior to device teardown.
     */
    void ReleaseAll();

    /**
     * @brief Scans plugins/wallpapers/ for supported image files (.png, .jpg, .jpeg, .webp, .bmp).
     */
    void RefreshWallpaperFileList();

    /**
     * @brief Returns the list of discovered image filenames inside plugins/wallpapers/.
     */
    const std::vector<std::string>& GetAvailableFiles() const noexcept { return available_files_; }

    /**
     * @brief Called on the DX11 render thread to ensure Lock Screen and Home Screen textures
     *        match the configured specs.
     */
    void SyncTextures(const std::string& lockscreen_spec, const std::string& homescreen_spec);

    /**
     * @brief Thread-safe signal from background workers to reload textures on the next frame.
     */
    void RequestReload() noexcept { reload_requested_.store(true); }

    const WallpaperTexture& GetLockscreenTexture() const noexcept { return lock_tex_; }
    const WallpaperTexture& GetHomescreenTexture() const noexcept { return home_tex_; }

private:
    TextureManager() = default;
    ~TextureManager() = default;

    bool LoadWicTextureFromFile(
        const std::filesystem::path& file_path,
        WallpaperTexture& out_tex);

    bool CreateProceduralPresetTexture(
        const std::string& preset_spec,
        WallpaperTexture& out_tex);

    bool CreateTextureFromRgbaBuffer(
        const uint8_t* pixels,
        UINT width,
        UINT height,
        WallpaperTexture& out_tex);

    void UpdateSlotTexture(const std::string& spec, WallpaperTexture& slot);

    ID3D11Device* device_{nullptr};
    WallpaperTexture lock_tex_{};
    WallpaperTexture home_tex_{};
    std::vector<std::string> available_files_{};
    std::atomic<bool> reload_requested_{true};
};

} // namespace nismara::ui
