#include "ui/texture_manager.hpp"
#include "ui/nismara_logo_data.hpp"
#include "core/config_manager.hpp"

#include <windows.h>
#include <wincodec.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>

namespace nismara::ui {

namespace {

std::string ToLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

bool IsSupportedImageExtension(const std::filesystem::path& p) {
    const std::string ext = ToLowerAscii(p.extension().string());
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" || ext == ".bmp" || ext == ".gif";
}

bool StartsWithPreset(const std::string& spec) {
    return spec.empty() || spec.rfind("preset:", 0) == 0;
}

struct RgbFloat {
    float r;
    float g;
    float b;
};

RgbFloat LerpColor(const RgbFloat& a, const RgbFloat& b, float t) {
    return {
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t
    };
}

} // namespace

TextureManager& TextureManager::Instance() {
    static TextureManager instance;
    return instance;
}

void TextureManager::SetDevice(ID3D11Device* device) {
    if (device_ != device) {
        ReleaseAll();
        device_ = device;
        reload_requested_.store(true);
    }
}

void TextureManager::ReleaseAll() {
    lock_tex_.Release();
    lock_tex_.loaded_spec.clear();
    home_tex_.Release();
    home_tex_.loaded_spec.clear();
}

void TextureManager::RefreshWallpaperFileList() {
    available_files_.clear();
    const auto dir = nismara::core::ConfigManager::Instance().GetWallpapersDirectory();

    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) {
        std::filesystem::create_directories(dir, ec);
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.is_regular_file(ec) && IsSupportedImageExtension(entry.path())) {
            available_files_.push_back(entry.path().filename().string());
        }
    }

    std::sort(available_files_.begin(), available_files_.end());
}

void TextureManager::SyncTextures(const std::string& lockscreen_spec, const std::string& homescreen_spec) {
    if (device_ == nullptr) {
        return;
    }

    const bool force = reload_requested_.exchange(false);
    if (force) {
        RefreshWallpaperFileList();
        lock_tex_.loaded_spec.clear();
        home_tex_.loaded_spec.clear();
    }

    if (lock_tex_.loaded_spec != lockscreen_spec || lock_tex_.srv == nullptr) {
        UpdateSlotTexture(lockscreen_spec, lock_tex_);
    }

    if (home_tex_.loaded_spec != homescreen_spec || home_tex_.srv == nullptr) {
        UpdateSlotTexture(homescreen_spec, home_tex_);
    }
}

void TextureManager::UpdateSlotTexture(const std::string& spec, WallpaperTexture& slot) {
    slot.Release();
    slot.loaded_spec = spec;
    slot.last_error.clear();

    if (StartsWithPreset(spec)) {
        CreateProceduralPresetTexture(spec, slot);
        return;
    }

    std::filesystem::path resolved_path(spec);
    std::error_code ec;
    if (!resolved_path.is_absolute() || !std::filesystem::exists(resolved_path, ec)) {
        resolved_path = nismara::core::ConfigManager::Instance().GetWallpapersDirectory() / spec;
    }

    if (!std::filesystem::exists(resolved_path, ec)) {
        slot.last_error = "File not found: " + spec;
        CreateProceduralPresetTexture("preset:ultramarine", slot);
        slot.loaded_spec = spec;
        return;
    }

    if (!LoadWicTextureFromFile(resolved_path, slot)) {
        const std::string err = slot.last_error;
        CreateProceduralPresetTexture("preset:ultramarine", slot);
        slot.loaded_spec = spec;
        slot.last_error = err;
    }
}

bool TextureManager::CreateProceduralPresetTexture(
    const std::string& preset_spec,
    WallpaperTexture& out_tex) {
    // Match iPhone 16 OLED screen aspect ratio (350:758) at high resolution
    constexpr UINT kWidth = 512;
    constexpr UINT kHeight = 1108;

    RgbFloat c_top{18.0f, 24.0f, 82.0f};
    RgbFloat c_mid{38.0f, 52.0f, 138.0f};
    RgbFloat c_bot{8.0f, 12.0f, 32.0f};
    RgbFloat orb1{88.0f, 115.0f, 255.0f};
    RgbFloat orb2{34.0f, 195.0f, 248.0f};

    if (preset_spec == "preset:midnight") {
        c_top = {10.0f, 14.0f, 26.0f};
        c_mid = {20.0f, 30.0f, 54.0f};
        c_bot = {6.0f, 8.0f, 16.0f};
        orb1 = {56.0f, 189.0f, 248.0f};
        orb2 = {99.0f, 102.0f, 241.0f};
    } else if (preset_spec == "preset:gold") {
        c_top = {26.0f, 20.0f, 10.0f};
        c_mid = {48.0f, 36.0f, 14.0f};
        c_bot = {10.0f, 10.0f, 14.0f};
        orb1 = {239.0f, 191.0f, 4.0f};
        orb2 = {217.0f, 119.0f, 6.0f};
    } else if (preset_spec == "preset:cyberpunk") {
        c_top = {38.0f, 10.0f, 64.0f};
        c_mid = {16.0f, 28.0f, 84.0f};
        c_bot = {8.0f, 12.0f, 28.0f};
        orb1 = {236.0f, 72.0f, 153.0f};
        orb2 = {6.0f, 182.0f, 212.0f};
    } else if (preset_spec == "preset:sunset") {
        c_top = {64.0f, 20.0f, 44.0f};
        c_mid = {92.0f, 38.0f, 28.0f};
        c_bot = {14.0f, 12.0f, 28.0f};
        orb1 = {249.0f, 115.0f, 22.0f};
        orb2 = {168.0f, 85.0f, 247.0f};
    }

    std::vector<uint8_t> pixels(kWidth * kHeight * 4);

    // Center the Nismara Group Logo horizontally and in the central open wallpaper area
    const int logo_x0 = static_cast<int>(kWidth - kNismaraLogoWidth) / 2;
    const int logo_y0 = static_cast<int>(kHeight * 0.55f) - static_cast<int>(kNismaraLogoHeight) / 2;

    for (UINT y = 0; y < kHeight; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(kHeight - 1);
        const RgbFloat base_row = (v < 0.5f)
                                      ? LerpColor(c_top, c_mid, v * 2.0f)
                                      : LerpColor(c_mid, c_bot, (v - 0.5f) * 2.0f);

        for (UINT x = 0; x < kWidth; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(kWidth - 1);

            // Smooth radial iOS 18 aurora bloom 1 (upper left-center)
            const float dx1 = u - 0.32f;
            const float dy1 = (v - 0.28f) * 1.6f;
            const float d1_sq = dx1 * dx1 + dy1 * dy1;
            const float w1 = std::exp(-d1_sq * 7.5f) * 0.42f;

            // Smooth radial iOS 18 aurora bloom 2 (lower right-center)
            const float dx2 = u - 0.72f;
            const float dy2 = (v - 0.65f) * 1.6f;
            const float d2_sq = dx2 * dx2 + dy2 * dy2;
            const float w2 = std::exp(-d2_sq * 6.8f) * 0.36f;

            float r = base_row.r + orb1.r * w1 + orb2.r * w2;
            float g = base_row.g + orb1.g * w1 + orb2.g * w2;
            float b = base_row.b + orb1.b * w1 + orb2.b * w2;

            // Soft drop shadow behind Nismara Group Logo (offset by +2px X, +3px Y)
            const int sx = static_cast<int>(x) - (logo_x0 + 2);
            const int sy = static_cast<int>(y) - (logo_y0 + 3);
            if (sx >= 0 && sx < static_cast<int>(kNismaraLogoWidth) &&
                sy >= 0 && sy < static_cast<int>(kNismaraLogoHeight)) {
                const float shadow_a =
                    (static_cast<float>(kNismaraLogoAlpha[sy * kNismaraLogoWidth + sx]) / 255.0f) * 0.45f;
                r *= (1.0f - shadow_a);
                g *= (1.0f - shadow_a);
                b *= (1.0f - shadow_a);
            }

            // Crisp white Nismara Group Logo composite
            const int lx = static_cast<int>(x) - logo_x0;
            const int ly = static_cast<int>(y) - logo_y0;
            if (lx >= 0 && lx < static_cast<int>(kNismaraLogoWidth) &&
                ly >= 0 && ly < static_cast<int>(kNismaraLogoHeight)) {
                const float logo_a =
                    (static_cast<float>(kNismaraLogoAlpha[ly * kNismaraLogoWidth + lx]) / 255.0f) * 0.94f;
                r = r * (1.0f - logo_a) + 255.0f * logo_a;
                g = g * (1.0f - logo_a) + 255.0f * logo_a;
                b = b * (1.0f - logo_a) + 255.0f * logo_a;
            }

            const size_t idx = (static_cast<size_t>(y) * kWidth + x) * 4;
            pixels[idx + 0] = static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f));
            pixels[idx + 1] = static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f));
            pixels[idx + 2] = static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f));
            pixels[idx + 3] = 255;
        }
    }

    return CreateTextureFromRgbaBuffer(pixels.data(), kWidth, kHeight, out_tex);
}

bool TextureManager::CreateTextureFromRgbaBuffer(
    const uint8_t* pixels,
    UINT width,
    UINT height,
    WallpaperTexture& out_tex) {
    if (device_ == nullptr || pixels == nullptr || width == 0 || height == 0) {
        return false;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem = pixels;
    init_data.SysMemPitch = width * 4;
    init_data.SysMemSlicePitch = 0;

    ID3D11Texture2D* tex2d = nullptr;
    HRESULT hr = device_->CreateTexture2D(&desc, &init_data, &tex2d);
    if (FAILED(hr) || tex2d == nullptr) {
        out_tex.last_error = "Failed to create DX11 Texture2D.";
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = desc.MipLevels;
    srv_desc.Texture2D.MostDetailedMip = 0;

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device_->CreateShaderResourceView(tex2d, &srv_desc, &srv);
    tex2d->Release();

    if (FAILED(hr) || srv == nullptr) {
        out_tex.last_error = "Failed to create DX11 ShaderResourceView.";
        return false;
    }

    out_tex.srv = srv;
    out_tex.width = static_cast<int>(width);
    out_tex.height = static_cast<int>(height);
    out_tex.last_error.clear();
    return true;
}

bool TextureManager::LoadWicTextureFromFile(
    const std::filesystem::path& file_path,
    WallpaperTexture& out_tex) {
    if (device_ == nullptr) {
        out_tex.last_error = "DX11 device not ready.";
        return false;
    }

    HRESULT hr_com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool should_uninit_com = (hr_com == S_OK || hr_com == S_FALSE);

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));

    if (FAILED(hr) || factory == nullptr) {
        if (should_uninit_com) {
            CoUninitialize();
        }
        out_tex.last_error = "Failed to initialize WIC factory.";
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromFilename(
        file_path.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder);

    if (FAILED(hr) || decoder == nullptr) {
        factory->Release();
        if (should_uninit_com) {
            CoUninitialize();
        }
        out_tex.last_error = "Unsupported or corrupted image format.";
        return false;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr) || frame == nullptr) {
        decoder->Release();
        factory->Release();
        if (should_uninit_com) {
            CoUninitialize();
        }
        out_tex.last_error = "Could not decode image frame.";
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    frame->GetSize(&width, &height);

    if (width == 0 || height == 0 || width > 8192 || height > 8192) {
        frame->Release();
        decoder->Release();
        factory->Release();
        if (should_uninit_com) {
            CoUninitialize();
        }
        out_tex.last_error = "Invalid image dimensions.";
        return false;
    }

    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr) || converter == nullptr) {
        frame->Release();
        decoder->Release();
        factory->Release();
        if (should_uninit_com) {
            CoUninitialize();
        }
        out_tex.last_error = "WIC format converter failed.";
        return false;
    }

    hr = converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);

    if (FAILED(hr)) {
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        if (should_uninit_com) {
            CoUninitialize();
        }
        out_tex.last_error = "Could not convert image to 32-bit RGBA.";
        return false;
    }

    const UINT row_pitch = width * 4;
    const UINT image_size = row_pitch * height;
    std::vector<uint8_t> pixels(image_size);

    hr = converter->CopyPixels(nullptr, row_pitch, image_size, pixels.data());

    converter->Release();
    frame->Release();
    decoder->Release();
    factory->Release();
    if (should_uninit_com) {
        CoUninitialize();
    }

    if (FAILED(hr)) {
        out_tex.last_error = "Failed to copy decoded image pixels.";
        return false;
    }

    return CreateTextureFromRgbaBuffer(pixels.data(), width, height, out_tex);
}

} // namespace nismara::ui
