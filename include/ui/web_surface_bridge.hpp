#pragma once

#include <d3d11.h>
#include <memory>
#include <mutex>
#include <string>

namespace nismara::ui {

/**
 * @brief Manages the Ultralight WebKit runtime, D3D11 texture surface,
 *        input translation, and JavaScript-to-C++ telemetry bridge for
 *        the in-game iPhone 16 OLED screen.
 */
class WebSurfaceBridge {
public:
    static constexpr uint32_t kScreenWidth = 350;
    static constexpr uint32_t kScreenHeight = 758;

    static WebSurfaceBridge& Instance();

    WebSurfaceBridge(const WebSurfaceBridge&) = delete;
    WebSurfaceBridge& operator=(const WebSurfaceBridge&) = delete;

    /**
     * @brief Initializes the Ultralight Platform, FileSystem, FontLoader, Renderer,
     *        and View. Returns false if runtime DLLs or web-ui assets are missing.
     */
    bool Initialize();

    /**
     * @brief Shuts down Ultralight and releases all D3D11 resources.
     */
    void Shutdown();

    /**
     * @brief Checks if Ultralight engine and view are initialized.
     */
    bool IsInitialized() const noexcept;

    /**
     * @brief Checks if the D3D11 texture surface is allocated and ready.
     */
    bool HasTexture() const noexcept;

    /**
     * @brief Checks if Ultralight is initialized and has a valid texture SRV.
     */
    bool IsReady() const noexcept;

    /**
     * @brief Updates the Ultralight engine loop and transfers rendered bitmap pixels
     *        into the DirectX 11 texture. Call once per frame during IDXGISwapChain::Present.
     */
    void UpdateTexture(ID3D11Device* device, ID3D11DeviceContext* context);

    /**
     * @brief Returns the Shader Resource View (SRV) for ImGui::Image or AddImageRounded.
     */
    ID3D11ShaderResourceView* GetTextureSRV() const noexcept;

    /**
     * @brief Forwards mouse input events from the hooked window procedure to the web view.
     *        Coordinates must be in local screen coordinates [0, kScreenWidth) x [0, kScreenHeight).
     */
    void FireMouseMove(int local_x, int local_y);
    void FireMouseDown(int local_x, int local_y, int button); // 0: Left, 1: Middle, 2: Right
    void FireMouseUp(int local_x, int local_y, int button);
    void FireMouseScroll(int delta_y);

    /**
     * @brief Serializes current TelemetryState and dispatches to JavaScript window event
     *        window.__NISMARA_DISPATCH_TELEMETRY__(data).
     */
    void DispatchTelemetry();

    /**
     * @brief Reloads the web page from the local web-ui directory.
     */
    void Reload();

    /**
     * @brief Releases D3D11 texture when swap chain resizes (ResizeBuffers).
     */
    void ReleaseD3DResources();

private:
    WebSurfaceBridge();
    ~WebSurfaceBridge();

    bool CreateD3DResources(ID3D11Device* device);
    void RegisterNativeBindings();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nismara::ui

