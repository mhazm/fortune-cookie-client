#include "ui/tablet_gui.hpp"
#include "ui/texture_manager.hpp"
#include "hooks/dx11_hook.hpp"
#include "core/api_client.hpp"
#include "core/companion_gate.hpp"
#include "core/config_manager.hpp"
#include "core/server_detector.hpp"

#include <windows.h>
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>

namespace nismara::ui {

namespace {

struct PhoneFramePalette {
    const char* name;
    ImU32 outer_body;
    ImU32 outer_border;
    ImU32 inner_highlight;
    ImU32 antenna_band;
    ImU32 button_base;
    ImU32 button_highlight;
};

constexpr PhoneFramePalette kFramePalettes[5] = {
    // 0: Ultramarine (Exact match to IPhone_16_Vector.svg: #525ca8, #414888, #a2ade5, #b7bbda, #363c71)
    {
        "Ultramarine (iPhone 16)",
        IM_COL32(82, 92, 168, 255),
        IM_COL32(65, 72, 136, 255),
        IM_COL32(162, 173, 229, 235),
        IM_COL32(183, 187, 218, 130),
        IM_COL32(54, 60, 113, 255),
        IM_COL32(118, 128, 205, 255)
    },
    // 1: Black Titanium
    {
        "Black Titanium",
        IM_COL32(38, 41, 46, 255),
        IM_COL32(22, 24, 28, 255),
        IM_COL32(92, 98, 108, 225),
        IM_COL32(70, 74, 82, 130),
        IM_COL32(28, 30, 34, 255),
        IM_COL32(72, 77, 86, 255)
    },
    // 2: Natural Titanium
    {
        "Natural Titanium",
        IM_COL32(132, 127, 119, 255),
        IM_COL32(95, 90, 84, 255),
        IM_COL32(195, 190, 182, 235),
        IM_COL32(165, 160, 152, 130),
        IM_COL32(102, 97, 90, 255),
        IM_COL32(168, 162, 154, 255)
    },
    // 3: Teal
    {
        "Teal (iPhone 16)",
        IM_COL32(58, 112, 112, 255),
        IM_COL32(40, 82, 82, 255),
        IM_COL32(130, 188, 188, 235),
        IM_COL32(150, 198, 198, 130),
        IM_COL32(42, 86, 86, 255),
        IM_COL32(98, 156, 156, 255)
    },
    // 4: Rose Pink
    {
        "Pink (iPhone 16)",
        IM_COL32(184, 106, 128, 255),
        IM_COL32(140, 74, 94, 255),
        IM_COL32(232, 168, 186, 235),
        IM_COL32(220, 175, 188, 130),
        IM_COL32(142, 76, 96, 255),
        IM_COL32(212, 140, 162, 255)
    }
};

struct PresetWallpaperItem {
    const char* spec;
    const char* title;
};

constexpr PresetWallpaperItem kPresetWallpapers[5] = {
    {"preset:ultramarine", "iOS 18 Ultramarine"},
    {"preset:midnight",    "Midnight Titanium"},
    {"preset:gold",        "Nismara Executive Gold"},
    {"preset:cyberpunk",   "Cyberpunk Night"},
    {"preset:sunset",      "Sunset Convoy"}
};

std::string FormatGearText(int32_t gear) {
    if (gear == 0) {
        return "N";
    }
    if (gear < 0) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "R%d", -gear);
        return buf;
    }
    char buf[16];
    std::snprintf(buf, sizeof(buf), "D%d", gear);
    return buf;
}

void GetLocalClockStrings(char* out_time, size_t time_sz, char* out_date, size_t date_sz) {
    std::time_t now = std::time(nullptr);
    std::tm local_tm = {};
    localtime_s(&local_tm, &now);
    std::strftime(out_time, time_sz, "%H:%M", &local_tm);
    std::strftime(out_date, date_sz, "%A, %b %d", &local_tm);
}

ImVec4 WearFractionToColor(float wear_fraction) {
    const float clamped = std::clamp(wear_fraction, 0.0f, 1.0f);
    if (clamped < 0.15f) {
        return ImVec4(0.20f, 0.83f, 0.60f, 1.00f);
    }
    if (clamped < 0.40f) {
        return ImVec4(0.98f, 0.75f, 0.14f, 1.00f);
    }
    return ImVec4(0.97f, 0.44f, 0.44f, 1.00f);
}

ImVec4 ParseHexColor(const std::string& hex, const ImVec4& fallback) {
    if (hex.size() == 7 && hex[0] == '#') {
        unsigned int r = 255, g = 255, b = 255;
        if (std::sscanf(hex.c_str() + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
            return ImVec4(
                static_cast<float>(r) / 255.0f,
                static_cast<float>(g) / 255.0f,
                static_cast<float>(b) / 255.0f,
                1.0f);
        }
    }
    return fallback;
}

/**
 * @brief Draws horizontally centered single-line text using a specific ImFont.
 */
void DrawTextCenteredX(
    ImDrawList* dl,
    ImFont* font,
    float font_size,
    float center_x,
    float y,
    ImU32 col,
    const char* text) {
    if (text == nullptr || text[0] == '\0') {
        return;
    }
    ImFont* f = (font != nullptr) ? font : ImGui::GetFont();
    const float sz = (font_size > 0.0f) ? font_size : ImGui::GetFontSize();
    const ImVec2 text_size = f->CalcTextSizeA(sz, FLT_MAX, 0.0f, text);
    dl->AddText(f, sz, ImVec2( std::floor(center_x - text_size.x * 0.5f), std::floor(y) ), col, text);
}

/**
 * @brief Draws text that automatically wraps to the next line if it exceeds max_width,
 *        returning the vertical height consumed.
 */
float DrawTextWrappedBox(
    ImDrawList* dl,
    ImFont* font,
    float font_size,
    const ImVec2& pos,
    float max_width,
    ImU32 col,
    const char* text) {
    if (text == nullptr || text[0] == '\0' || max_width <= 4.0f) {
        return 0.0f;
    }
    ImFont* f = (font != nullptr) ? font : ImGui::GetFont();
    const float sz = (font_size > 0.0f) ? font_size : ImGui::GetFontSize();
    const ImVec2 text_size = f->CalcTextSizeA(sz, FLT_MAX, max_width, text);
    dl->AddText(f, sz, ImVec2(std::floor(pos.x), std::floor(pos.y)), col, text, nullptr, max_width);
    return text_size.y;
}

void RenderCompactProgressBar(const char* label, float fraction, const char* overlay_right, const ImVec4& fill_col) {
    ImGui::TextColored(ImVec4(0.78f, 0.84f, 0.92f, 1.0f), "%s", label);
    const float right_w = ImGui::CalcTextSize(overlay_right).x;
    const float avail_w = ImGui::GetContentRegionAvail().x;
    if (avail_w > right_w + 10.0f) {
        ImGui::SameLine(ImGui::GetCursorPosX() + avail_w - right_w);
    }
    ImGui::TextColored(ImVec4(0.96f, 0.98f, 1.0f, 1.0f), "%s", overlay_right);

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, fill_col);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.06f, 0.09f, 0.15f, 0.90f));
    ImGui::ProgressBar(std::clamp(fraction, 0.0f, 1.0f), ImVec2(-1.0f, 7.0f), "");
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Solace ImGui Modern Design Tokens & Glassmorphism Primitives
// ============================================================================
namespace SolaceTokens {
    constexpr ImU32 c_card_bg          = IM_COL32(18, 22, 32, 220); // Frosted dark glass
    constexpr ImU32 c_card_border      = IM_COL32(255, 255, 255, 24); // Crisp 1px highlight
    constexpr ImU32 c_card_sub_bg      = IM_COL32(12, 16, 24, 190); // Recessed card surface
    constexpr ImU32 c_primary          = IM_COL32(56, 189, 248, 255); // Sky Blue
    constexpr ImU32 c_emerald          = IM_COL32(52, 211, 153, 255); // Emerald Green
    constexpr ImU32 c_amber            = IM_COL32(251, 191, 36, 255); // Amber Gold
    constexpr ImU32 c_rose             = IM_COL32(248, 113, 113, 255); // Rose Crimson
    constexpr ImU32 c_purple           = IM_COL32(168, 85, 247, 255); // Royal Purple
    constexpr ImU32 c_text_bright      = IM_COL32(248, 250, 252, 255); // Slate 50
    constexpr ImU32 c_text_muted       = IM_COL32(148, 163, 184, 220); // Slate 400
    constexpr ImU32 c_text_dim         = IM_COL32(100, 116, 139, 190); // Slate 500
}

void DrawSolaceGlassCard(
    ImDrawList* dl,
    const ImVec2& min,
    const ImVec2& max,
    float radius = 14.0f,
    ImU32 bg_col = SolaceTokens::c_card_bg,
    ImU32 border_col = SolaceTokens::c_card_border) {
    dl->AddRectFilled(min, max, bg_col, radius);
    dl->AddRect(min, max, border_col, radius, 0, 1.0f);
}

float DrawSolacePillBadge(
    ImDrawList* dl,
    ImFont* font,
    float font_size,
    const ImVec2& pos,
    const char* text,
    ImU32 text_col,
    ImU32 bg_col,
    float pad_x = 9.0f,
    float pad_y = 3.5f) {
    ImFont* f = (font != nullptr) ? font : ImGui::GetFont();
    const ImVec2 text_sz = f->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text);
    const float h = text_sz.y + pad_y * 2.0f;
    const float w = text_sz.x + pad_x * 2.0f;
    const ImVec2 p_max(pos.x + w, pos.y + h);
    dl->AddRectFilled(pos, p_max, bg_col, h * 0.5f);
    dl->AddRect(pos, p_max, IM_COL32(255, 255, 255, 22), h * 0.5f, 0, 1.0f);
    dl->AddText(f, font_size, ImVec2(pos.x + pad_x, pos.y + pad_y), text_col, text);
    return w;
}

void DrawSolaceMeter(
    ImDrawList* dl,
    const ImVec2& min,
    const ImVec2& max,
    float fraction,
    ImU32 fill_col,
    ImU32 track_col = IM_COL32(24, 32, 48, 180),
    float radius = 4.0f) {
    dl->AddRectFilled(min, max, track_col, radius);
    const float frac = std::clamp(fraction, 0.0f, 1.0f);
    if (frac > 0.005f) {
        const float fill_w = (max.x - min.x) * frac;
        dl->AddRectFilled(min, ImVec2(min.x + fill_w, max.y), fill_col, radius);
    }
}

void DrawSolaceCircularArc(
    ImDrawList* dl,
    const ImVec2& center,
    float radius,
    float thickness,
    float fraction,
    ImU32 fill_col,
    ImU32 track_col = IM_COL32(30, 41, 59, 140)) {
    // 360-degree background track
    dl->PathArcTo(center, radius, 0.0f, 6.2831853f, 32);
    dl->PathStroke(track_col, 0, thickness);

    // Active arc starting from top (-PI/2)
    const float frac = std::clamp(fraction, 0.0f, 1.0f);
    if (frac > 0.005f) {
        constexpr float start_angle = -1.5707963f; // -PI / 2
        const float end_angle = start_angle + frac * 6.2831853f;
        dl->PathArcTo(center, radius, start_angle, end_angle, 32);
        dl->PathStroke(fill_col, 0, thickness);
    }
}

void DrawSolaceEcoRpmBar(
    ImDrawList* dl,
    const ImVec2& min,
    const ImVec2& max,
    float current_rpm,
    float max_rpm) {
    const float w = max.x - min.x;
    const float radius = 5.0f;

    // Background track
    dl->AddRectFilled(min, max, IM_COL32(16, 22, 32, 210), radius);
    dl->AddRect(min, max, IM_COL32(255, 255, 255, 20), radius, 0, 1.0f);

    const float limit = (max_rpm > 500.0f) ? max_rpm : 2500.0f;
    const float clamped_rpm = std::clamp(current_rpm, 0.0f, limit);

    // Multi-zone color bands:
    // 0 -> 800 RPM (Idle): Slate
    // 800 -> 1500 RPM (Eco Torque): Emerald Green
    // 1500 -> 2100 RPM (High Power): Amber
    // 2100 -> limit (Redline): Crimson
    const float x_idle = min.x + w * std::clamp(800.0f / limit, 0.0f, 1.0f);
    const float x_eco  = min.x + w * std::clamp(1500.0f / limit, 0.0f, 1.0f);
    const float x_pow  = min.x + w * std::clamp(2100.0f / limit, 0.0f, 1.0f);

    // Soft band zone guides
    dl->AddRectFilled(ImVec2(min.x + 1.0f, min.y + 1.0f), ImVec2(x_idle, max.y - 1.0f), IM_COL32(100, 116, 139, 40));
    dl->AddRectFilled(ImVec2(x_idle, min.y + 1.0f), ImVec2(x_eco, max.y - 1.0f), IM_COL32(52, 211, 153, 50));
    dl->AddRectFilled(ImVec2(x_eco, min.y + 1.0f), ImVec2(x_pow, max.y - 1.0f), IM_COL32(251, 191, 36, 50));
    dl->AddRectFilled(ImVec2(x_pow, min.y + 1.0f), ImVec2(max.x - 1.0f, max.y - 1.0f), IM_COL32(248, 113, 113, 60));

    // Fill current RPM
    const float cur_x = min.x + w * (clamped_rpm / limit);
    if (cur_x > min.x + 2.0f) {
        ImU32 fill_col = IM_COL32(100, 116, 139, 210); // idle
        if (clamped_rpm >= 800.0f && clamped_rpm <= 1500.0f) {
            fill_col = IM_COL32(52, 211, 153, 230); // eco
        } else if (clamped_rpm > 1500.0f && clamped_rpm <= 2100.0f) {
            fill_col = IM_COL32(251, 191, 36, 230); // power
        } else if (clamped_rpm > 2100.0f) {
            fill_col = IM_COL32(248, 113, 113, 240); // redline
        }
        dl->AddRectFilled(min, ImVec2(cur_x, max.y), fill_col, radius);
    }

    // Glowing vertical needle cursor
    if (clamped_rpm > 50.0f) {
        dl->AddLine(ImVec2(cur_x, min.y - 2.0f), ImVec2(cur_x, max.y + 2.0f), IM_COL32(255, 255, 255, 255), 2.5f);
        dl->AddCircleFilled(ImVec2(cur_x, min.y - 2.0f), 3.0f, IM_COL32(255, 255, 255, 255));
    }
}

void DrawSolaceTelltaleLamp(
    ImDrawList* dl,
    ImFont* font,
    const ImVec2& min,
    const ImVec2& max,
    const char* label,
    bool active,
    ImU32 active_col,
    float radius = 7.0f) {
    ImFont* f = (font != nullptr) ? font : ImGui::GetFont();
    const ImVec2 sz = f->CalcTextSizeA(11.0f, FLT_MAX, 0.0f, label);
    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);

    if (active) {
        // Glowing active lamp
        const ImU32 bg = (active_col & 0x00FFFFFF) | (0x35 << 24); // ~20% alpha
        dl->AddRectFilled(min, max, bg, radius);
        dl->AddRect(min, max, active_col, radius, 0, 1.4f);
        dl->AddText(f, 11.0f, ImVec2(std::floor(center.x - sz.x * 0.5f), std::floor(center.y - sz.y * 0.5f)), active_col, label);
    } else {
        // Inactive dim lamp
        dl->AddRectFilled(min, max, IM_COL32(14, 18, 26, 180), radius);
        dl->AddRect(min, max, IM_COL32(255, 255, 255, 12), radius, 0, 1.0f);
        dl->AddText(f, 11.0f, ImVec2(std::floor(center.x - sz.x * 0.5f), std::floor(center.y - sz.y * 0.5f)), IM_COL32(100, 116, 139, 130), label);
    }
}

/**
 * @brief Native iOS-style Segmented Control for switching sub-tabs inside apps.
 */
bool RenderSegmentedControl(const char* id, int* current_tab, const char* const items[], int count, float height = 30.0f) {
    ImGui::PushID(id);
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float item_w = (avail_w - (count - 1) * 4.0f) / static_cast<float>(count);
    bool changed = false;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(cursor, ImVec2(cursor.x + avail_w, cursor.y + height), IM_COL32(14, 18, 26, 220), 8.0f);
    dl->AddRect(cursor, ImVec2(cursor.x + avail_w, cursor.y + height), IM_COL32(255, 255, 255, 20), 8.0f, 0, 1.0f);

    for (int i = 0; i < count; ++i) {
        ImGui::SetCursorScreenPos(ImVec2(cursor.x + i * (item_w + 4.0f), cursor.y));
        const bool selected = (*current_tab == i);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.90f, 0.88f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.50f, 0.95f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.40f, 0.85f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.12f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.68f, 0.80f, 1.0f));
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        char btn_id[32];
        std::snprintf(btn_id, sizeof(btn_id), "%s##tab_%d", items[i], i);
        if (ImGui::Button(btn_id, ImVec2(item_w, height))) {
            if (*current_tab != i) {
                *current_tab = i;
                changed = true;
            }
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }

    ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + height + 8.0f));
    ImGui::PopID();
    return changed;
}

void DrawAppSquircleIcon(
    ImDrawList* dl,
    const ImVec2& pos,
    float size,
    ImU32 col_top,
    ImU32 col_bot,
    PhoneAppId app_id,
    bool is_lock_shortcut = false,
    bool hovered = false) {
    const float expand = hovered ? 2.0f : 0.0f;
    const ImVec2 p_min(pos.x - expand, pos.y - expand);
    const ImVec2 p_max(pos.x + size + expand, pos.y + size + expand);
    const float rounding = (size + expand * 2.0f) * 0.24f;

    // Soft drop shadow
    dl->AddRectFilled(
        ImVec2(p_min.x + 1.0f, p_min.y + 3.0f),
        ImVec2(p_max.x + 1.0f, p_max.y + 3.0f),
        IM_COL32(0, 0, 0, 115),
        rounding);

    // Base icon fill
    dl->AddRectFilled(p_min, p_max, col_bot, rounding);
    // Upper glossy overlay
    dl->AddRectFilled(
        p_min,
        ImVec2(p_max.x, p_min.y + (p_max.y - p_min.y) * 0.50f),
        col_top,
        rounding,
        ImDrawFlags_RoundCornersTop);

    // Subtle glass border
    dl->AddRect(p_min, p_max, IM_COL32(255, 255, 255, hovered ? 165 : 70), rounding, 0, 1.2f);

    const ImVec2 center((p_min.x + p_max.x) * 0.5f, (p_min.y + p_max.y) * 0.5f);
    const ImU32 white = IM_COL32(255, 255, 255, 248);

    if (is_lock_shortcut) {
        dl->AddRectFilled(
            ImVec2(center.x - 9.0f, center.y - 2.0f),
            ImVec2(center.x + 9.0f, center.y + 11.0f),
            white,
            3.0f);
        dl->PathArcTo(ImVec2(center.x, center.y - 2.0f), 6.0f, 3.14159f, 6.28318f, 12);
        dl->PathStroke(white, 0, 2.2f);
        return;
    }

    switch (app_id) {
        case PhoneAppId::Telemetry: {
            dl->PathArcTo(ImVec2(center.x, center.y + 3.0f), 12.0f, 2.35f, 7.07f, 16);
            dl->PathStroke(white, 0, 2.4f);
            dl->AddLine(
                ImVec2(center.x, center.y + 3.0f),
                ImVec2(center.x + 7.5f, center.y - 5.5f),
                IM_COL32(255, 230, 120, 255),
                2.4f);
            dl->AddCircleFilled(ImVec2(center.x, center.y + 3.0f), 2.8f, white);
            break;
        }
        case PhoneAppId::Dispatch: {
            dl->AddRect(
                ImVec2(center.x - 10.0f, center.y - 10.0f),
                ImVec2(center.x + 10.0f, center.y + 11.0f),
                white,
                3.0f,
                0,
                2.2f);
            dl->AddLine(ImVec2(center.x - 10.0f, center.y - 3.0f), ImVec2(center.x + 10.0f, center.y - 3.0f), white, 1.8f);
            dl->AddLine(ImVec2(center.x, center.y - 10.0f), ImVec2(center.x, center.y - 3.0f), white, 1.8f);
            dl->AddLine(ImVec2(center.x - 5.0f, center.y + 4.0f), ImVec2(center.x + 5.0f, center.y + 4.0f), white, 2.0f);
            break;
        }
        case PhoneAppId::NismaraId: {
            dl->AddCircleFilled(ImVec2(center.x, center.y - 4.5f), 5.0f, white);
            dl->PathArcTo(ImVec2(center.x, center.y + 11.0f), 9.5f, 3.45f, 5.97f, 12);
            dl->PathStroke(white, 0, 2.5f);
            break;
        }
        case PhoneAppId::Garage: {
            dl->AddTriangle(
                ImVec2(center.x, center.y - 12.0f),
                ImVec2(center.x + 13.0f, center.y - 3.0f),
                ImVec2(center.x - 13.0f, center.y - 3.0f),
                white,
                2.2f);
            dl->AddRect(
                ImVec2(center.x - 9.0f, center.y - 3.0f),
                ImVec2(center.x + 9.0f, center.y + 10.0f),
                white,
                1.5f,
                0,
                2.0f);
            dl->AddLine(ImVec2(center.x - 5.0f, center.y + 2.0f), ImVec2(center.x + 5.0f, center.y + 2.0f), white, 1.6f);
            dl->AddLine(ImVec2(center.x - 5.0f, center.y + 6.0f), ImVec2(center.x + 5.0f, center.y + 6.0f), white, 1.6f);
            break;
        }
        case PhoneAppId::PenaltyPoints: {
            dl->PathLineTo(ImVec2(center.x, center.y - 11.0f));
            dl->PathLineTo(ImVec2(center.x + 10.0f, center.y - 6.0f));
            dl->PathLineTo(ImVec2(center.x + 8.0f, center.y + 4.0f));
            dl->PathLineTo(ImVec2(center.x, center.y + 11.0f));
            dl->PathLineTo(ImVec2(center.x - 8.0f, center.y + 4.0f));
            dl->PathLineTo(ImVec2(center.x - 10.0f, center.y - 6.0f));
            dl->PathStroke(white, ImDrawFlags_Closed, 2.2f);
            dl->AddLine(ImVec2(center.x, center.y - 5.0f), ImVec2(center.x, center.y + 2.0f), IM_COL32(255, 230, 120, 255), 2.2f);
            dl->AddCircleFilled(ImVec2(center.x, center.y + 6.0f), 1.5f, IM_COL32(255, 230, 120, 255));
            break;
        }
        case PhoneAppId::Wallpapers: {
            dl->AddRect(
                ImVec2(center.x - 11.0f, center.y - 9.0f),
                ImVec2(center.x + 11.0f, center.y + 9.0f),
                white,
                3.0f,
                0,
                2.0f);
            dl->AddCircleFilled(ImVec2(center.x - 4.5f, center.y - 3.0f), 2.5f, IM_COL32(255, 230, 110, 255));
            dl->AddTriangleFilled(
                ImVec2(center.x - 9.0f, center.y + 7.5f),
                ImVec2(center.x - 1.0f, center.y - 1.0f),
                ImVec2(center.x + 5.0f, center.y + 7.5f),
                white);
            dl->AddTriangleFilled(
                ImVec2(center.x - 1.0f, center.y + 7.5f),
                ImVec2(center.x + 4.5f, center.y + 1.0f),
                ImVec2(center.x + 9.5f, center.y + 7.5f),
                white);
            break;
        }
        case PhoneAppId::Settings:
        default: {
            dl->AddCircle(center, 7.5f, white, 16, 2.4f);
            dl->AddCircleFilled(center, 3.0f, white);
            for (int i = 0; i < 6; ++i) {
                const float angle = static_cast<float>(i) * (3.1415926f / 3.0f);
                const ImVec2 p1(center.x + std::cos(angle) * 7.0f, center.y + std::sin(angle) * 7.0f);
                const ImVec2 p2(center.x + std::cos(angle) * 11.0f, center.y + std::sin(angle) * 11.0f);
                dl->AddLine(p1, p2, white, 2.6f);
            }
            break;
        }
    }
}

} // namespace

TabletGui& TabletGui::Instance() {
    static TabletGui instance;
    return instance;
}

TabletGui::TabletGui() = default;

void TabletGui::ToggleVisible() {
    const bool now_visible = !visible_.load();
    if (now_visible) {
        const auto cfg = nismara::core::ConfigManager::Instance().GetConfig();
        if (cfg.auto_lock_on_close) {
            screen_state_ = PhoneScreenState::LockScreen;
            unlock_anim_progress_ = 0.0f;
            unlocking_in_progress_ = false;
        }
    }
    visible_.store(now_visible);
}

void TabletGui::SetVisible(bool visible) noexcept {
    if (visible && !visible_.load()) {
        const auto cfg = nismara::core::ConfigManager::Instance().GetConfig();
        if (cfg.auto_lock_on_close) {
            screen_state_ = PhoneScreenState::LockScreen;
            unlock_anim_progress_ = 0.0f;
            unlocking_in_progress_ = false;
        }
    }
    visible_.store(visible);
}

void TabletGui::LockPhone() noexcept {
    screen_state_ = PhoneScreenState::LockScreen;
    unlock_anim_progress_ = 0.0f;
    unlocking_in_progress_ = false;
    dynamic_island_expanded_ = false;
}

void TabletGui::UnlockToHome() noexcept {
    screen_state_ = PhoneScreenState::HomeScreen;
    unlock_anim_progress_ = 0.0f;
    unlocking_in_progress_ = false;
}

void TabletGui::OpenApp(PhoneAppId app) noexcept {
    active_app_ = app;
    selected_fleet_idx_ = -1;
    screen_state_ = PhoneScreenState::ActiveApp;
}

void TabletGui::LoadTrueTypeFonts() {
    ImGuiIO& io = ImGui::GetIO();

    wchar_t win_dir[MAX_PATH] = {0};
    std::filesystem::path fonts_dir = "C:\\Windows\\Fonts";
    if (GetWindowsDirectoryW(win_dir, MAX_PATH) > 0) {
        fonts_dir = std::filesystem::path(win_dir) / "Fonts";
    }

    const auto pick_existing = [&fonts_dir](std::initializer_list<const char*> candidates) -> std::string {
        std::error_code ec;
        for (const char* name : candidates) {
            const auto p = fonts_dir / name;
            if (std::filesystem::exists(p, ec)) {
                return p.string();
            }
        }
        return {};
    };

    // Prefer Segoe UI Semibold / Regular / Bold (clean modern sans-serif matching iOS SF Pro aesthetics)
    const std::string regular_path  = pick_existing({"seguisb.ttf", "segoeui.ttf", "arial.ttf"});
    const std::string semibold_path = pick_existing({"segoeuib.ttf", "seguisb.ttf", "arialbd.ttf"});
    const std::string clock_path    = pick_existing({"segoeuib.ttf", "seguisb.ttf", "segoeui.ttf"});

    ImFontConfig cfg = {};
    cfg.OversampleH = 3;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = true;

    if (!regular_path.empty()) {
        font_regular_ = io.Fonts->AddFontFromFileTTF(regular_path.c_str(), 15.5f, &cfg);
        font_small_   = io.Fonts->AddFontFromFileTTF(regular_path.c_str(), 13.0f, &cfg);
    }
    if (!semibold_path.empty()) {
        font_semibold_  = io.Fonts->AddFontFromFileTTF(semibold_path.c_str(), 17.0f, &cfg);
        font_large_num_ = io.Fonts->AddFontFromFileTTF(semibold_path.c_str(), 30.0f, &cfg);
    }
    if (!clock_path.empty()) {
        font_clock_ = io.Fonts->AddFontFromFileTTF(clock_path.c_str(), 62.0f, &cfg);
    }

    if (font_regular_ == nullptr) {
        font_regular_ = io.Fonts->AddFontDefault();
    }
    if (font_small_ == nullptr) {
        font_small_ = font_regular_;
    }
    if (font_semibold_ == nullptr) {
        font_semibold_ = font_regular_;
    }
    if (font_large_num_ == nullptr) {
        font_large_num_ = font_semibold_;
    }
    if (font_clock_ == nullptr) {
        font_clock_ = font_large_num_;
    }

    io.FontDefault = font_regular_;
}

void TabletGui::InitializeStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 54.0f;
    style.ChildRounding = 14.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 10.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 8.0f;

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowPadding = ImVec2(0.0f, 0.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 8.0f);
    style.ScrollbarSize = 6.0f;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4(0.96f, 0.98f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.58f, 0.65f, 0.76f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.07f, 0.10f, 0.17f, 0.84f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.07f, 0.09f, 0.15f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(1.00f, 1.00f, 1.00f, 0.14f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.05f, 0.08f, 0.14f, 0.90f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.12f, 0.18f, 0.30f, 0.92f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.15f, 0.24f, 0.38f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.20f, 0.44f, 0.86f, 0.90f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.26f, 0.54f, 0.98f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.16f, 0.36f, 0.74f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.18f, 0.36f, 0.68f, 0.65f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.24f, 0.46f, 0.85f, 0.80f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.28f, 0.54f, 0.96f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(1.00f, 1.00f, 1.00f, 0.12f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.15f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.55f, 0.65f, 0.82f, 0.45f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.70f, 0.80f, 0.95f, 0.70f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.85f, 0.92f, 1.00f, 0.90f);

    LoadTrueTypeFonts();

    if (!config_loaded_) {
        const auto cfg = nismara::core::ConfigManager::Instance().GetConfig();
        std::snprintf(token_buffer_, sizeof(token_buffer_), "%s", cfg.driver_token.c_str());
        std::snprintf(api_url_buffer_, sizeof(api_url_buffer_), "%s", cfg.api_base_url.c_str());
        lockscreen_wallpaper_spec_ = cfg.lockscreen_wallpaper;
        homescreen_wallpaper_spec_ = cfg.homescreen_wallpaper;
        frame_color_idx_ = std::clamp(cfg.phone_frame_color, 0, 4);
        auto_lock_on_close_ = cfg.auto_lock_on_close;
        config_loaded_ = true;
    }
}

void TabletGui::Render() {
    if (!visible_.load()) {
        return;
    }

    // Safety check: If Companion App is closed while phone is open, auto-close the phone
    static uint32_t s_gatekeeper_frame = 0;
    if ((++s_gatekeeper_frame % 60) == 0) {
        HANDLE hMutex = OpenMutexW(SYNCHRONIZE, FALSE, L"Global\\NismaraCompanionRunning");
        if (hMutex == nullptr) {
            hMutex = OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\NismaraCompanionRunning");
        }
        if (hMutex != nullptr) {
            CloseHandle(hMutex);
        } else {
            visible_.store(false);
            return;
        }
    }

    hover_pill_text_.clear();

    if (!config_loaded_) {
        const auto cfg = nismara::core::ConfigManager::Instance().GetConfig();
        std::snprintf(token_buffer_, sizeof(token_buffer_), "%s", cfg.driver_token.c_str());
        std::snprintf(api_url_buffer_, sizeof(api_url_buffer_), "%s", cfg.api_base_url.c_str());
        lockscreen_wallpaper_spec_ = cfg.lockscreen_wallpaper;
        homescreen_wallpaper_spec_ = cfg.homescreen_wallpaper;
        frame_color_idx_ = std::clamp(cfg.phone_frame_color, 0, 4);
        auto_lock_on_close_ = cfg.auto_lock_on_close;
        config_loaded_ = true;
    } else {
        const auto cfg = nismara::core::ConfigManager::Instance().GetConfig();
        lockscreen_wallpaper_spec_ = cfg.lockscreen_wallpaper;
        homescreen_wallpaper_spec_ = cfg.homescreen_wallpaper;
    }

    // Ensure preset or custom wallpaper DX11 textures are ready
    TextureManager::Instance().SyncTextures(lockscreen_wallpaper_spec_, homescreen_wallpaper_spec_);

    const auto snap = nismara::core::TelemetryState::Instance().GetSnapshot();
    const auto account = nismara::core::TelemetryState::Instance().GetDriverAccount();

    // iPhone 16 proportional dimensions (360:730 ratio from IPhone_16_Vector.svg -> 392 x 794 px)
    constexpr float kPhoneWidth = 392.0f;
    constexpr float kPhoneHeight = 794.0f;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(kPhoneWidth, kPhoneHeight), ImGuiCond_Always);
    if (io.DisplaySize.x > 0.0f && io.DisplaySize.y > 0.0f) {
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x - kPhoneWidth - 48.0f, (io.DisplaySize.y - kPhoneHeight) * 0.5f),
            ImGuiCond_FirstUseEver);
    }

    constexpr ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground;

    if (ImGui::Begin("NISMARA iPhone 16 OS", nullptr, window_flags)) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImVec2 win_pos = ImGui::GetWindowPos();
        const ImVec2 win_size = ImGui::GetWindowSize();

        // 1. Render iPhone 16 Backplate & Physical Side Buttons
        RenderPhoneBackplate(draw_list, win_pos, win_size, frame_color_idx_);

        // Inner OLED Screen Bounds (matching rect14 in IPhone_16_Vector.svg)
        const ImVec2 screen_min(win_pos.x + 21.0f, win_pos.y + 18.0f);
        const ImVec2 screen_max(win_pos.x + win_size.x - 21.0f, win_pos.y + win_size.y - 18.0f);

        // Advance unlock animation if triggered
        if (unlocking_in_progress_) {
            unlock_anim_progress_ += io.DeltaTime * 5.2f;
            if (unlock_anim_progress_ >= 1.0f) {
                UnlockToHome();
            }
        }

        // 2. Render Rounded Screen Wallpaper (AddImageRounded with 46px corner radius -> zero corner bleeding!)
        draw_list->PushClipRect(screen_min, screen_max, true);

        if (screen_state_ == PhoneScreenState::LockScreen) {
            // 1. NATIVE LOCK SCREEN (Giant Clock, Date, Widgets, Notification Card, Wallpaper)
            RenderScreenWallpaper(draw_list, screen_min, screen_max, true);
            RenderLockScreen(draw_list, screen_min, screen_max, snap, account);
            RenderStatusBarAndDynamicIsland(draw_list, screen_min, screen_max, snap, account);
            RenderHomeIndicatorBar(draw_list, screen_min, screen_max, true);
        } else if (screen_state_ == PhoneScreenState::HomeScreen) {
            // 2. NATIVE HOME SCREEN (Wallpapers, Smart Driver Widget, 8 App Icons, Frosted Dock)
            RenderScreenWallpaper(draw_list, screen_min, screen_max, false);
            RenderHomeScreen(draw_list, screen_min, screen_max, snap, account);
            RenderStatusBarAndDynamicIsland(draw_list, screen_min, screen_max, snap, account);
            RenderHomeIndicatorBar(draw_list, screen_min, screen_max, false);
        } else {
            // 3. NATIVE IMGUI APP VIEW (Telemetry, Waybill, Nismara ID, Garage, Penalty, Wallpapers, Settings)
            RenderScreenWallpaper(draw_list, screen_min, screen_max, false);
            draw_list->AddRectFilled(
                screen_min,
                screen_max,
                IM_COL32(8, 11, 20, 228),
                46.0f);
            RenderActiveAppView(draw_list, screen_min, screen_max, snap, account);
            RenderStatusBarAndDynamicIsland(draw_list, screen_min, screen_max, snap, account);
            RenderHomeIndicatorBar(draw_list, screen_min, screen_max, false);
        }

        draw_list->PopClipRect();

        // 3. Render Hardware Bezel Ring & Titanium/Ultramarine Frame Overlay ON TOP OF the screen perimeter
        //    so no pixel inside the screen can ever spill outside the iPhone frame!
        RenderPhoneBezelOverlay(draw_list, win_pos, win_size, frame_color_idx_);

        // 4. Render Centered iOS Hover Pill Tooltip (if any icon/button is hovered)
        RenderActiveHoverPill(draw_list);
    }
    ImGui::End();
}

void TabletGui::RenderPhoneBackplate(
    ImDrawList* draw_list,
    const ImVec2& win_pos,
    const ImVec2& win_size,
    int frame_color_idx) {
    const auto& pal = kFramePalettes[std::clamp(frame_color_idx, 0, 4)];

    // Left Side Buttons: Action + Volume Up + Volume Down
    const float left_btn_x0 = win_pos.x + 1.0f;
    const float left_btn_x1 = win_pos.x + 6.0f;
    draw_list->AddRectFilled(ImVec2(left_btn_x0, win_pos.y + 158.0f), ImVec2(left_btn_x1, win_pos.y + 188.0f), pal.button_base, 2.5f);
    draw_list->AddRectFilled(ImVec2(left_btn_x0, win_pos.y + 160.0f), ImVec2(left_btn_x0 + 2.5f, win_pos.y + 186.0f), pal.button_highlight, 2.0f);

    draw_list->AddRectFilled(ImVec2(left_btn_x0, win_pos.y + 214.0f), ImVec2(left_btn_x1, win_pos.y + 272.0f), pal.button_base, 2.5f);
    draw_list->AddRectFilled(ImVec2(left_btn_x0, win_pos.y + 216.0f), ImVec2(left_btn_x0 + 2.5f, win_pos.y + 270.0f), pal.button_highlight, 2.0f);

    draw_list->AddRectFilled(ImVec2(left_btn_x0, win_pos.y + 288.0f), ImVec2(left_btn_x1, win_pos.y + 346.0f), pal.button_base, 2.5f);
    draw_list->AddRectFilled(ImVec2(left_btn_x0, win_pos.y + 290.0f), ImVec2(left_btn_x0 + 2.5f, win_pos.y + 344.0f), pal.button_highlight, 2.0f);

    // Right Side: Power / Lock Button (Interactive!)
    const float right_btn_x0 = win_pos.x + win_size.x - 6.0f;
    const float right_btn_x1 = win_pos.x + win_size.x - 1.0f;
    const ImVec2 pwr_min(right_btn_x0, win_pos.y + 236.0f);
    const ImVec2 pwr_max(right_btn_x1, win_pos.y + 328.0f);

    ImGui::SetCursorScreenPos(ImVec2(right_btn_x0 - 4.0f, pwr_min.y));
    if (ImGui::InvisibleButton("##SidePowerButton", ImVec2(14.0f, pwr_max.y - pwr_min.y))) {
        if (screen_state_ == PhoneScreenState::LockScreen) {
            unlocking_in_progress_ = true;
        } else {
            LockPhone();
        }
    }
    const bool pwr_hovered = ImGui::IsItemHovered();
    if (pwr_hovered) {
        hover_pill_text_ = (screen_state_ == PhoneScreenState::LockScreen)
                               ? "Power: Click to Unlock"
                               : "Power: Click to Lock";
        hover_pill_cx_ = win_pos.x + win_size.x - 95.0f;
        hover_pill_y_ = pwr_min.y + 30.0f;
    }

    draw_list->AddRectFilled(pwr_min, pwr_max, pwr_hovered ? pal.button_highlight : pal.button_base, 2.5f);
    draw_list->AddRectFilled(
        ImVec2(pwr_max.x - 2.5f, pwr_min.y + 2.0f),
        ImVec2(pwr_max.x, pwr_max.y - 2.0f),
        pal.button_highlight,
        2.0f);

    // Outer Phone Body Drop Shadow
    const ImVec2 body_min(win_pos.x + 5.0f, win_pos.y + 2.0f);
    const ImVec2 body_max(win_pos.x + win_size.x - 5.0f, win_pos.y + win_size.y - 2.0f);
    draw_list->AddRectFilled(
        ImVec2(body_min.x + 3.0f, body_min.y + 6.0f),
        ImVec2(body_max.x + 3.0f, body_max.y + 6.0f),
        IM_COL32(0, 0, 0, 150),
        62.0f);

    // Outer Frame Fill + Black Glass Bezel Fill + Inner OLED Screen Base
    draw_list->AddRectFilled(body_min, body_max, pal.outer_body, 60.0f);

    const ImVec2 bezel_min(win_pos.x + 11.0f, win_pos.y + 8.0f);
    const ImVec2 bezel_max(win_pos.x + win_size.x - 11.0f, win_pos.y + win_size.y - 8.0f);
    draw_list->AddRectFilled(bezel_min, bezel_max, IM_COL32(8, 8, 10, 255), 54.0f);

    const ImVec2 screen_min(win_pos.x + 21.0f, win_pos.y + 18.0f);
    const ImVec2 screen_max(win_pos.x + win_size.x - 21.0f, win_pos.y + win_size.y - 18.0f);
    draw_list->AddRectFilled(screen_min, screen_max, IM_COL32(10, 14, 24, 255), 46.0f);
}

void TabletGui::RenderPhoneBezelOverlay(
    ImDrawList* draw_list,
    const ImVec2& win_pos,
    const ImVec2& win_size,
    int frame_color_idx) {
    const auto& pal = kFramePalettes[std::clamp(frame_color_idx, 0, 4)];

    const ImVec2 body_min(win_pos.x + 5.0f, win_pos.y + 2.0f);
    const ImVec2 body_max(win_pos.x + win_size.x - 5.0f, win_pos.y + win_size.y - 2.0f);
    const ImVec2 rim_min(win_pos.x + 9.0f, win_pos.y + 6.0f);
    const ImVec2 rim_max(win_pos.x + win_size.x - 9.0f, win_pos.y + win_size.y - 6.0f);
    const ImVec2 screen_min(win_pos.x + 21.0f, win_pos.y + 18.0f);
    const ImVec2 screen_max(win_pos.x + win_size.x - 21.0f, win_pos.y + win_size.y - 18.0f);

    // 1. Thick Black Inner Bezel Mask Ring around the OLED Screen (seals any inner corner edge)
    draw_list->AddRect(
        ImVec2(screen_min.x - 5.5f, screen_min.y - 5.5f),
        ImVec2(screen_max.x + 5.5f, screen_max.y + 5.5f),
        IM_COL32(8, 8, 10, 255),
        50.0f,
        0,
        12.0f);

    // Subtle glass screen rim highlight
    draw_list->AddRect(screen_min, screen_max, IM_COL32(255, 255, 255, 32), 46.0f, 0, 1.0f);

    // 2. Outer Aluminum / Titanium Casing Border Ring
    draw_list->AddRect(
        ImVec2(body_min.x + 2.5f, body_min.y + 2.5f),
        ImVec2(body_max.x - 2.5f, body_max.y - 2.5f),
        pal.outer_body,
        58.0f,
        0,
        6.0f);
    draw_list->AddRect(body_min, body_max, pal.outer_border, 60.0f, 0, 1.6f);

    // 3. Antenna Bands (rect3-2 in IPhone_16_Vector.svg)
    draw_list->AddRectFilled(
        ImVec2(body_min.x, body_min.y + 78.0f),
        ImVec2(body_min.x + 5.0f, body_min.y + 85.0f),
        pal.antenna_band);
    draw_list->AddRectFilled(
        ImVec2(body_max.x - 5.0f, body_min.y + 78.0f),
        ImVec2(body_max.x, body_min.y + 85.0f),
        pal.antenna_band);
    draw_list->AddRectFilled(
        ImVec2(body_min.x, body_max.y - 85.0f),
        ImVec2(body_min.x + 5.0f, body_max.y - 78.0f),
        pal.antenna_band);
    draw_list->AddRectFilled(
        ImVec2(body_max.x - 5.0f, body_max.y - 85.0f),
        ImVec2(body_max.x, body_max.y - 78.0f),
        pal.antenna_band);

    // 4. Inner Metallic Highlight Rim (rect4-3 in IPhone_16_Vector.svg)
    draw_list->AddRect(rim_min, rim_max, pal.inner_highlight, 55.0f, 0, 2.0f);

    // 5. Top Earpiece Speaker Slit (path14224 in IPhone_16_Vector.svg)
    const float center_x = win_pos.x + win_size.x * 0.5f;
    draw_list->AddRectFilled(
        ImVec2(center_x - 32.0f, win_pos.y + 10.5f),
        ImVec2(center_x + 32.0f, win_pos.y + 13.5f),
        IM_COL32(36, 38, 46, 255),
        1.5f);
}

void TabletGui::RenderScreenWallpaper(
    ImDrawList* draw_list,
    const ImVec2& screen_min,
    const ImVec2& screen_max,
    bool is_lockscreen) {
    const auto& tex = is_lockscreen
                          ? TextureManager::Instance().GetLockscreenTexture()
                          : TextureManager::Instance().GetHomescreenTexture();

    if (tex.srv != nullptr && tex.width > 0 && tex.height > 0) {
        const float screen_w = screen_max.x - screen_min.x;
        const float screen_h = screen_max.y - screen_min.y;
        const float screen_aspect = screen_w / screen_h;
        const float tex_aspect = static_cast<float>(tex.width) / static_cast<float>(tex.height);

        ImVec2 uv_min(0.0f, 0.0f);
        ImVec2 uv_max(1.0f, 1.0f);
        if (tex_aspect > screen_aspect) {
            const float visible_w = screen_aspect / tex_aspect;
            uv_min.x = (1.0f - visible_w) * 0.5f;
            uv_max.x = 1.0f - uv_min.x;
        } else if (tex_aspect < screen_aspect) {
            const float visible_h = tex_aspect / screen_aspect;
            uv_min.y = (1.0f - visible_h) * 0.5f;
            uv_max.y = 1.0f - uv_min.y;
        }

        draw_list->AddImageRounded(
            reinterpret_cast<ImTextureID>(tex.srv),
            screen_min,
            screen_max,
            uv_min,
            uv_max,
            IM_COL32(255, 255, 255, 255),
            46.0f);

        draw_list->AddRectFilled(
            screen_min,
            screen_max,
            IM_COL32(6, 9, 16, is_lockscreen ? 52 : 68),
            46.0f);
    } else {
        draw_list->AddRectFilled(screen_min, screen_max, IM_COL32(16, 22, 58, 255), 46.0f);
    }
}

void TabletGui::RenderActiveHoverPill(ImDrawList* draw_list) {
    if (hover_pill_text_.empty()) {
        return;
    }

    ImFont* f = (font_small_ != nullptr) ? font_small_ : ImGui::GetFont();
    const float f_sz = 13.0f;
    const ImVec2 text_sz = f->CalcTextSizeA(f_sz, FLT_MAX, 0.0f, hover_pill_text_.c_str());

    const float pad_x = 12.0f;
    const float pad_y = 5.0f;
    const float pill_w = text_sz.x + pad_x * 2.0f;
    const float pill_h = text_sz.y + pad_y * 2.0f;

    const ImVec2 p_min(std::floor(hover_pill_cx_ - pill_w * 0.5f), std::floor(hover_pill_y_ - pill_h * 0.5f));
    const ImVec2 p_max(p_min.x + pill_w, p_min.y + pill_h);

    draw_list->AddRectFilled(
        ImVec2(p_min.x + 1.0f, p_min.y + 2.0f),
        ImVec2(p_max.x + 1.0f, p_max.y + 2.0f),
        IM_COL32(0, 0, 0, 140),
        pill_h * 0.5f);
    draw_list->AddRectFilled(p_min, p_max, IM_COL32(15, 23, 42, 242), pill_h * 0.5f);
    draw_list->AddRect(p_min, p_max, IM_COL32(148, 163, 184, 110), pill_h * 0.5f, 0, 1.0f);

    draw_list->AddText(
        f,
        f_sz,
        ImVec2(std::floor(p_min.x + (pill_w - text_sz.x) * 0.5f), std::floor(p_min.y + (pill_h - text_sz.y) * 0.5f)),
        IM_COL32(248, 250, 255, 255),
        hover_pill_text_.c_str());
}

void TabletGui::RenderStatusBarAndDynamicIsland(
    ImDrawList* draw_list,
    const ImVec2& screen_min,
    const ImVec2& screen_max,
    const nismara::core::TelemetrySnapshot& snap,
    const nismara::core::DriverAccountProfile& account) {
    char clock_buf[16] = {0};
    char date_buf[32] = {0};
    GetLocalClockStrings(clock_buf, sizeof(clock_buf), date_buf, sizeof(date_buf));

    const float center_x = (screen_min.x + screen_max.x) * 0.5f;
    const float top_y = screen_min.y + 10.0f;

    // Left Status Bar: Time (using clean Semibold/Small font)
    ImFont* sb_font = (font_semibold_ != nullptr) ? font_semibold_ : ImGui::GetFont();
    draw_list->AddText(
        sb_font,
        14.0f,
        ImVec2(screen_min.x + 26.0f, top_y + 7.0f),
        IM_COL32(255, 255, 255, 245),
        clock_buf);

    // Right Status Bar: Signal Bars + Battery Icon
    const float right_x = screen_max.x - 24.0f;
    draw_list->AddRect(
        ImVec2(right_x - 22.0f, top_y + 9.0f),
        ImVec2(right_x - 2.0f, top_y + 20.0f),
        IM_COL32(255, 255, 255, 215),
        3.0f,
        0,
        1.2f);
    draw_list->AddRectFilled(
        ImVec2(right_x - 20.0f, top_y + 11.0f),
        ImVec2(right_x - 4.0f, top_y + 18.0f),
        account.status == nismara::core::AccountSyncStatus::Authenticated
            ? IM_COL32(52, 211, 153, 255)
            : IM_COL32(255, 255, 255, 235),
        1.5f);
    draw_list->AddRectFilled(
        ImVec2(right_x - 1.5f, top_y + 12.5f),
        ImVec2(right_x, top_y + 16.5f),
        IM_COL32(255, 255, 255, 200),
        1.0f);

    for (int b = 0; b < 4; ++b) {
        const float bh = 3.5f + b * 2.2f;
        const float bx = right_x - 44.0f + b * 4.5f;
        draw_list->AddRectFilled(
            ImVec2(bx, top_y + 20.0f - bh),
            ImVec2(bx + 2.8f, top_y + 20.0f),
            IM_COL32(255, 255, 255, 235),
            1.0f);
    }

    // Dynamic Island (rect16441 & g1832 in IPhone_16_Vector.svg)
    const bool has_matched_fleet =
        (account.active_fleet_index >= 0 &&
         account.active_fleet_index < static_cast<int>(account.fleets.size()));
    const nismara::core::DriverFleetVehicle* active_fleet =
        has_matched_fleet ? &account.fleets[account.active_fleet_index] : nullptr;

    const float island_w = dynamic_island_expanded_ ? 232.0f : (has_matched_fleet ? 150.0f : 116.0f);
    const float island_h = dynamic_island_expanded_ ? 52.0f : 30.0f;
    const ImVec2 island_min(center_x - island_w * 0.5f, top_y);
    const ImVec2 island_max(center_x + island_w * 0.5f, top_y + island_h);

    ImGui::SetCursorScreenPos(island_min);
    if (ImGui::InvisibleButton("##DynamicIslandBtn", ImVec2(island_w, island_h))) {
        dynamic_island_expanded_ = !dynamic_island_expanded_;
    }

    draw_list->AddRectFilled(island_min, island_max, IM_COL32(0, 0, 0, 255), island_h * 0.5f);
    draw_list->AddRect(island_min, island_max, IM_COL32(65, 75, 95, 110), island_h * 0.5f, 0, 1.0f);

    // Front Camera Lens inside Dynamic Island
    const ImVec2 cam_center(island_max.x - 15.0f, top_y + 15.0f);
    draw_list->AddCircleFilled(cam_center, 7.2f, IM_COL32(10, 10, 12, 255));
    draw_list->AddCircleFilled(cam_center, 4.6f, IM_COL32(10, 16, 28, 255));
    draw_list->AddCircleFilled(cam_center, 3.0f, IM_COL32(12, 26, 52, 255));
    draw_list->AddCircleFilled(ImVec2(cam_center.x - 1.4f, cam_center.y - 1.4f), 1.2f, IM_COL32(56, 138, 220, 185));

    ImFont* sm_font = (font_small_ != nullptr) ? font_small_ : ImGui::GetFont();
    if (!dynamic_island_expanded_) {
        char speed_pill[40];
        if (active_fleet != nullptr) {
            std::snprintf(speed_pill, sizeof(speed_pill), "%s | %.0f km/h", active_fleet->fleet_number.c_str(), snap.speed_kmh);
        } else {
            std::snprintf(speed_pill, sizeof(speed_pill), "%.0f km/h", snap.speed_kmh);
        }
        draw_list->AddCircleFilled(
            ImVec2(island_min.x + 12.0f, top_y + 15.0f),
            3.6f,
            snap.sdk_active ? IM_COL32(52, 211, 153, 255) : IM_COL32(251, 191, 36, 255));

        const ImVec2 sp_sz = sm_font->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, speed_pill);
        draw_list->AddText(
            sm_font,
            12.0f,
            ImVec2(island_min.x + 20.0f, top_y + (island_h - sp_sz.y) * 0.5f),
            IM_COL32(230, 240, 255, 245),
            speed_pill);
    } else {
        char line1[64];
        const std::string gear_str = FormatGearText(snap.displayed_gear);
        if (active_fleet != nullptr) {
            std::snprintf(
                line1,
                sizeof(line1),
                "[%s] %.0f km/h [%s]",
                active_fleet->fleet_number.c_str(),
                snap.speed_kmh,
                gear_str.c_str());
        } else {
            std::snprintf(line1, sizeof(line1), "LIVE: %.0f km/h [%s]", snap.speed_kmh, gear_str.c_str());
        }
        draw_list->AddText(sm_font, 13.0f, ImVec2(island_min.x + 14.0f, top_y + 8.0f), IM_COL32(56, 189, 248, 255), line1);

        char line2[64];
        if (snap.job_active) {
            std::snprintf(
                line2,
                sizeof(line2),
                "-> %s (%.0f km)",
                snap.destination_city.c_str(),
                snap.navigation_distance_m / 1000.0f);
        } else {
            const float fuel_pct = (snap.fuel_capacity > 0.0f) ? (snap.fuel_current / snap.fuel_capacity) * 100.0f : 0.0f;
            std::snprintf(line2, sizeof(line2), "Fuel: %.0f%%  |  %.0f RPM", fuel_pct, snap.engine_rpm);
        }
        draw_list->AddText(sm_font, 12.5f, ImVec2(island_min.x + 14.0f, top_y + 27.0f), IM_COL32(215, 225, 240, 240), line2);
    }
}

void TabletGui::RenderHomeIndicatorBar(
    ImDrawList* draw_list,
    const ImVec2& screen_min,
    const ImVec2& screen_max,
    bool is_lockscreen) {
    const float center_x = (screen_min.x + screen_max.x) * 0.5f;
    const float bar_w = 124.0f;
    const float bar_h = 5.0f;
    const float bar_y = screen_max.y - 13.0f;

    const ImVec2 btn_min(center_x - 80.0f, screen_max.y - 26.0f);
    const ImVec2 btn_max(center_x + 80.0f, screen_max.y - 2.0f);

    ImGui::SetCursorScreenPos(btn_min);
    if (ImGui::InvisibleButton("##HomeIndicatorBarBtn", ImVec2(btn_max.x - btn_min.x, btn_max.y - btn_min.y))) {
        if (is_lockscreen) {
            unlocking_in_progress_ = true;
        } else if (screen_state_ == PhoneScreenState::ActiveApp) {
            UnlockToHome();
        }
    }
    const bool hovered = ImGui::IsItemHovered();
    if (hovered && !is_lockscreen && screen_state_ == PhoneScreenState::ActiveApp) {
        hover_pill_text_ = "Return to Home Screen";
        hover_pill_cx_ = center_x;
        hover_pill_y_ = screen_max.y - 36.0f;
    }

    draw_list->AddRectFilled(
        ImVec2(center_x - bar_w * 0.5f, bar_y),
        ImVec2(center_x + bar_w * 0.5f, bar_y + bar_h),
        hovered ? IM_COL32(56, 189, 248, 255) : IM_COL32(255, 255, 255, 220),
        3.0f);
}

void TabletGui::RenderLockScreen(
    ImDrawList* draw_list,
    const ImVec2& screen_min,
    const ImVec2& screen_max,
    const nismara::core::TelemetrySnapshot& snap,
    const nismara::core::DriverAccountProfile& account) {
    const float screen_w = screen_max.x - screen_min.x;
    const float center_x = (screen_min.x + screen_max.x) * 0.5f;
    const float slide_offset_y = -unlock_anim_progress_ * 140.0f;
    const int alpha_mod = static_cast<int>((1.0f - unlock_anim_progress_) * 255.0f);

    char clock_buf[16] = {0};
    char date_buf[32] = {0};
    GetLocalClockStrings(clock_buf, sizeof(clock_buf), date_buf, sizeof(date_buf));

    // Padlock icon under Dynamic Island
    const float lock_y = screen_min.y + 52.0f + slide_offset_y;
    draw_list->AddRectFilled(
        ImVec2(center_x - 6.0f, lock_y + 5.0f),
        ImVec2(center_x + 6.0f, lock_y + 14.0f),
        IM_COL32(255, 255, 255, alpha_mod),
        2.0f);
    draw_list->PathArcTo(ImVec2(center_x, lock_y + 5.0f), 4.2f, 3.14159f, 6.28318f, 10);
    draw_list->PathStroke(IM_COL32(255, 255, 255, alpha_mod), 0, 1.8f);

    // Date String (Centered)
    DrawTextCenteredX(
        draw_list,
        font_semibold_,
        16.0f,
        center_x,
        screen_min.y + 76.0f + slide_offset_y,
        IM_COL32(235, 242, 255, alpha_mod),
        date_buf);

    // Large Native 62px TrueType iOS 18 Lock Screen Clock
    ImFont* clk_font = (font_clock_ != nullptr) ? font_clock_ : ImGui::GetFont();
    const float clk_size = 62.0f;
    const ImVec2 clock_sz = clk_font->CalcTextSizeA(clk_size, FLT_MAX, 0.0f, clock_buf);
    draw_list->AddText(
        clk_font,
        clk_size,
        ImVec2(std::floor(center_x - clock_sz.x * 0.5f + 2.0f), std::floor(screen_min.y + 94.0f + slide_offset_y + 3.0f)),
        IM_COL32(0, 0, 0, alpha_mod / 2),
        clock_buf);
    draw_list->AddText(
        clk_font,
        clk_size,
        ImVec2(std::floor(center_x - clock_sz.x * 0.5f), std::floor(screen_min.y + 94.0f + slide_offset_y)),
        IM_COL32(255, 255, 255, alpha_mod),
        clock_buf);

    // Driver Greeting Pill (Centered & Clamped)
    std::string greeting = (account.status == nismara::core::AccountSyncStatus::Authenticated)
                               ? (account.name + "  |  Lv." + std::to_string(account.level))
                               : "Nismara OS v2.0  |  Tap to Unlock";
    ImFont* reg_font = (font_regular_ != nullptr) ? font_regular_ : ImGui::GetFont();
    const ImVec2 greet_sz = reg_font->CalcTextSizeA(14.5f, FLT_MAX, 0.0f, greeting.c_str());
    const float pill_w = std::min(greet_sz.x + 28.0f, screen_w - 36.0f);
    const float pill_y = screen_min.y + 168.0f + slide_offset_y;

    draw_list->AddRectFilled(
        ImVec2(center_x - pill_w * 0.5f, pill_y),
        ImVec2(center_x + pill_w * 0.5f, pill_y + 26.0f),
        IM_COL32(12, 18, 34, (alpha_mod * 175) / 255),
        13.0f);
    draw_list->AddRect(
        ImVec2(center_x - pill_w * 0.5f, pill_y),
        ImVec2(center_x + pill_w * 0.5f, pill_y + 26.0f),
        IM_COL32(255, 255, 255, (alpha_mod * 45) / 255),
        13.0f);
    DrawTextCenteredX(
        draw_list,
        reg_font,
        14.5f,
        center_x,
        pill_y + (26.0f - greet_sz.y) * 0.5f,
        IM_COL32(190, 225, 255, alpha_mod),
        greeting.c_str());

    // Two iOS 18 Frosted Lock Screen Widgets
    const float widget_y = screen_min.y + 210.0f + slide_offset_y;
    const float widget_w = (screen_w - 42.0f) * 0.5f;
    const float widget_h = 74.0f;

    // Left Widget: Live Truck Telemetry
    const ImVec2 w1_min(screen_min.x + 16.0f, widget_y);
    const ImVec2 w1_max(w1_min.x + widget_w, widget_y + widget_h);
    draw_list->AddRectFilled(w1_min, w1_max, IM_COL32(12, 18, 32, (alpha_mod * 195) / 255), 16.0f);
    draw_list->AddRect(w1_min, w1_max, IM_COL32(255, 255, 255, (alpha_mod * 45) / 255), 16.0f);
    draw_list->AddText(font_small_, 12.0f, ImVec2(w1_min.x + 12.0f, w1_min.y + 10.0f), IM_COL32(56, 189, 248, alpha_mod), "TRUCK STATUS");

    char w1_val[48];
    const float fuel_pct = (snap.fuel_capacity > 0.0f) ? (snap.fuel_current / snap.fuel_capacity) * 100.0f : 0.0f;
    std::snprintf(w1_val, sizeof(w1_val), "%.0f km/h | %.0f%% Fuel", snap.speed_kmh, fuel_pct);
    draw_list->AddText(font_semibold_, 15.0f, ImVec2(w1_min.x + 12.0f, w1_min.y + 28.0f), IM_COL32(255, 255, 255, alpha_mod), w1_val);

    char w1_sub[48];
    if (account.active_fleet_index >= 0 &&
        account.active_fleet_index < static_cast<int>(account.fleets.size())) {
        std::snprintf(
            w1_sub,
            sizeof(w1_sub),
            "%s  |  %s",
            account.fleets[account.active_fleet_index].fleet_number.c_str(),
            snap.engine_running ? "Online" : "Standby");
    } else {
        std::snprintf(w1_sub, sizeof(w1_sub), "%s", snap.engine_running ? "Engine Online" : "Engine Standby");
    }
    draw_list->AddText(
        font_small_,
        12.5f,
        ImVec2(w1_min.x + 12.0f, w1_min.y + 49.0f),
        IM_COL32(165, 180, 205, alpha_mod),
        w1_sub);

    // Right Widget: Nismara Wallet NC
    const ImVec2 w2_min(w1_max.x + 10.0f, widget_y);
    const ImVec2 w2_max(w2_min.x + widget_w, widget_y + widget_h);
    draw_list->AddRectFilled(w2_min, w2_max, IM_COL32(12, 18, 32, (alpha_mod * 195) / 255), 16.0f);
    draw_list->AddRect(w2_min, w2_max, IM_COL32(255, 255, 255, (alpha_mod * 45) / 255), 16.0f);
    draw_list->AddText(font_small_, 12.0f, ImVec2(w2_min.x + 12.0f, w2_min.y + 10.0f), IM_COL32(250, 204, 21, alpha_mod), "NISMARA WALLET");

    char w2_val[48];
    if (account.status == nismara::core::AccountSyncStatus::Authenticated) {
        std::snprintf(w2_val, sizeof(w2_val), "%.0f NC", account.total_nc);
    } else {
        std::snprintf(w2_val, sizeof(w2_val), "Not Synced");
    }
    draw_list->AddText(font_semibold_, 15.0f, ImVec2(w2_min.x + 12.0f, w2_min.y + 28.0f), IM_COL32(255, 255, 255, alpha_mod), w2_val);
    draw_list->AddText(
        font_small_,
        12.5f,
        ImVec2(w2_min.x + 12.0f, w2_min.y + 49.0f),
        IM_COL32(165, 180, 205, alpha_mod),
        account.nismara_plus_active ? "Nismara+ Active" : "Standard Driver");

    // Full-Width iOS 18 Frosted Server Status Widget on Lock Screen
    const float srv_widget_y = widget_y + widget_h + 10.0f;
    const ImVec2 ws_min(screen_min.x + 16.0f, srv_widget_y);
    const ImVec2 ws_max(screen_max.x - 16.0f, srv_widget_y + 72.0f);
    draw_list->AddRectFilled(ws_min, ws_max, IM_COL32(12, 18, 32, (alpha_mod * 195) / 255), 16.0f);
    draw_list->AddRect(ws_min, ws_max, IM_COL32(255, 255, 255, (alpha_mod * 45) / 255), 16.0f);

    draw_list->AddText(
        font_small_,
        12.0f,
        ImVec2(ws_min.x + 14.0f, ws_min.y + 10.0f),
        IM_COL32(52, 211, 153, alpha_mod),
        "SERVER STATUS");

    const ImU32 conn_dot_col = snap.sdk_active
                                   ? IM_COL32(52, 211, 153, alpha_mod)
                                   : IM_COL32(251, 191, 36, alpha_mod);
    draw_list->AddCircleFilled(ImVec2(ws_min.x + 19.0f, ws_min.y + 36.0f), 4.2f, conn_dot_col);
    draw_list->AddText(
        font_semibold_,
        15.0f,
        ImVec2(ws_min.x + 28.0f, ws_min.y + 27.0f),
        IM_COL32(255, 255, 255, alpha_mod),
        snap.sdk_active ? "Connected" : "Standby");

    ImU32 play_mode_col = IM_COL32(165, 195, 235, alpha_mod);
    if (snap.server_mode == "TruckersMP") {
        play_mode_col = IM_COL32(251, 113, 133, alpha_mod);
    } else if (snap.server_mode == "HaulMP") {
        play_mode_col = IM_COL32(192, 132, 252, alpha_mod);
    } else if (snap.server_mode == "SCS Convoy") {
        play_mode_col = IM_COL32(52, 211, 153, alpha_mod);
    }
    const std::string playing_line = "Playing " + snap.server_mode;
    draw_list->AddText(
        font_small_,
        13.0f,
        ImVec2(ws_min.x + 14.0f, ws_min.y + 49.0f),
        play_mode_col,
        playing_line.c_str());

    // Notification Card at Bottom of Lock Screen (with automatic text wrapping)
    const float notif_y = screen_max.y - 168.0f + slide_offset_y;
    const ImVec2 n_min(screen_min.x + 16.0f, notif_y);
    const ImVec2 n_max(screen_max.x - 16.0f, notif_y + 84.0f);
    const float n_inner_w = (n_max.x - n_min.x) - 28.0f;

    draw_list->AddRectFilled(n_min, n_max, IM_COL32(14, 20, 36, (alpha_mod * 212) / 255), 18.0f);
    draw_list->AddRect(n_min, n_max, IM_COL32(255, 255, 255, (alpha_mod * 50) / 255), 18.0f);

    if (snap.job_active) {
        draw_list->AddText(font_small_, 12.0f, ImVec2(n_min.x + 14.0f, n_min.y + 10.0f), IM_COL32(52, 211, 153, alpha_mod), "ACTIVE JOB CONTRACT");
        std::string route = snap.origin_city + " -> " + snap.destination_city;
        DrawTextWrappedBox(draw_list, font_semibold_, 15.0f, ImVec2(n_min.x + 14.0f, n_min.y + 28.0f), n_inner_w, IM_COL32(255, 255, 255, alpha_mod), route.c_str());
        char sub[80];
        std::snprintf(
            sub,
            sizeof(sub),
            "%s  |  %.1f km remaining",
            snap.cargo_name.c_str(),
            snap.navigation_distance_m / 1000.0f);
        DrawTextWrappedBox(draw_list, font_small_, 13.0f, ImVec2(n_min.x + 14.0f, n_min.y + 52.0f), n_inner_w, IM_COL32(180, 195, 220, alpha_mod), sub);
    } else {
        draw_list->AddText(font_small_, 12.0f, ImVec2(n_min.x + 14.0f, n_min.y + 10.0f), IM_COL32(56, 189, 248, alpha_mod), "NISMARA LOGISTICS OS");
        draw_list->AddText(font_semibold_, 15.0f, ImVec2(n_min.x + 14.0f, n_min.y + 28.0f), IM_COL32(255, 255, 255, alpha_mod), "No Active Cargo Contract");
        DrawTextWrappedBox(
            draw_list,
            font_small_,
            13.0f,
            ImVec2(n_min.x + 14.0f, n_min.y + 50.0f),
            n_inner_w,
            IM_COL32(180, 195, 220, alpha_mod),
            "Accept a freight job in-game to start tracking.");
    }

    // Unlock Prompt Text at Bottom (Centered)
    DrawTextCenteredX(
        draw_list,
        font_regular_,
        14.5f,
        center_x,
        screen_max.y - 44.0f,
        IM_COL32(235, 242, 255, alpha_mod),
        "Click anywhere to Unlock");

    // Full Lock Screen Click / Swipe Unlock Zone
    ImGui::SetCursorScreenPos(ImVec2(screen_min.x + 10.0f, screen_min.y + 46.0f));
    if (ImGui::InvisibleButton(
            "##LockScreenUnlockSurface",
            ImVec2(screen_w - 20.0f, (screen_max.y - screen_min.y) - 62.0f))) {
        unlocking_in_progress_ = true;
    }
}

void TabletGui::RenderHomeScreen(
    ImDrawList* draw_list,
    const ImVec2& screen_min,
    const ImVec2& screen_max,
    const nismara::core::TelemetrySnapshot& snap,
    const nismara::core::DriverAccountProfile& account) {
    const float screen_w = screen_max.x - screen_min.x;

    // 1. Top iOS 18 Smart Widget (with auto-wrapping text & dual stat sub-cards, zero '?' characters!)
    const ImVec2 banner_min(screen_min.x + 16.0f, screen_min.y + 54.0f);
    const ImVec2 banner_max(screen_max.x - 16.0f, screen_min.y + 172.0f);
    const float banner_inner_w = (banner_max.x - banner_min.x) - 28.0f;

    draw_list->AddRectFilled(banner_min, banner_max, IM_COL32(11, 17, 30, 205), 20.0f);
    draw_list->AddRect(banner_min, banner_max, IM_COL32(255, 255, 255, 48), 20.0f);

    // Top-right Sync Status Indicator Pill inside widget
    const bool synced = (account.status == nismara::core::AccountSyncStatus::Authenticated);
    draw_list->AddCircleFilled(
        ImVec2(banner_max.x - 16.0f, banner_min.y + 18.0f),
        4.5f,
        synced ? IM_COL32(52, 211, 153, 255) : IM_COL32(251, 191, 36, 255));

    const std::string drv_title = synced
                                      ? (account.name + " (" + account.trucky_rank + ")")
                                      : "Nismara Driver Unit";
    DrawTextWrappedBox(
        draw_list,
        font_semibold_,
        15.5f,
        ImVec2(banner_min.x + 14.0f, banner_min.y + 10.0f),
        banner_inner_w - 18.0f,
        IM_COL32(56, 189, 248, 255),
        drv_title.c_str());

    const std::string base_truck =
        (!snap.truck_brand.empty() || !snap.truck_name.empty())
            ? (snap.truck_brand + " " + snap.truck_name)
            : snap.game_name;
    const bool has_matched_fleet =
        (account.active_fleet_index >= 0 &&
         account.active_fleet_index < static_cast<int>(account.fleets.size()));

    std::string truck_label = base_truck;
    ImU32 truck_label_col = IM_COL32(240, 245, 255, 245);
    if (has_matched_fleet) {
        const auto& fl = account.fleets[account.active_fleet_index];
        truck_label = "[" + fl.fleet_number + "] " + base_truck;
        if (!fl.custom_name.empty()) {
            truck_label += " \"" + fl.custom_name + "\"";
        }
        truck_label_col = IM_COL32(52, 211, 153, 255);
    } else if (synced && !account.fleets.empty() && (!snap.truck_brand.empty() || !snap.truck_name.empty())) {
        truck_label = "[UNREGISTERED] " + base_truck;
        truck_label_col = IM_COL32(251, 191, 36, 255);
    }

    DrawTextWrappedBox(
        draw_list,
        has_matched_fleet ? font_semibold_ : font_regular_,
        13.8f,
        ImVec2(banner_min.x + 14.0f, banner_min.y + 32.0f),
        banner_inner_w,
        truck_label_col,
        truck_label.c_str());

    // Two structured mini-pills inside the Home Widget for Speed & Wallet NC
    const float sub_y = banner_min.y + 58.0f;
    const float sub_w = (banner_inner_w - 8.0f) * 0.5f;
    const float sub_h = 46.0f;

    const ImVec2 s1_min(banner_min.x + 14.0f, sub_y);
    const ImVec2 s1_max(s1_min.x + sub_w, sub_y + sub_h);
    draw_list->AddRectFilled(s1_min, s1_max, IM_COL32(20, 30, 52, 190), 10.0f);
    draw_list->AddText(font_small_, 11.5f, ImVec2(s1_min.x + 10.0f, s1_min.y + 6.0f), IM_COL32(148, 163, 184, 240), "SPEED & GEAR");
    char speed_box_str[32];
    std::snprintf(speed_box_str, sizeof(speed_box_str), "%.0f km/h [%s]", snap.speed_kmh, FormatGearText(snap.displayed_gear).c_str());
    draw_list->AddText(font_semibold_, 14.5f, ImVec2(s1_min.x + 10.0f, s1_min.y + 22.0f), IM_COL32(255, 255, 255, 255), speed_box_str);

    const ImVec2 s2_min(s1_max.x + 8.0f, sub_y);
    const ImVec2 s2_max(s2_min.x + sub_w, sub_y + sub_h);
    draw_list->AddRectFilled(s2_min, s2_max, IM_COL32(20, 30, 52, 190), 10.0f);
    draw_list->AddText(font_small_, 11.5f, ImVec2(s2_min.x + 10.0f, s2_min.y + 6.0f), IM_COL32(250, 204, 21, 240), "WALLET BALANCE");
    char wallet_box_str[32];
    std::snprintf(wallet_box_str, sizeof(wallet_box_str), "%.0f NC", account.total_nc);
    draw_list->AddText(font_semibold_, 14.5f, ImVec2(s2_min.x + 10.0f, s2_min.y + 22.0f), IM_COL32(250, 204, 21, 255), wallet_box_str);

    // 2. iOS App Icon Grid (4 columns x 2 rows = 8 icons)
    struct HomeAppEntry {
        const char* label;
        PhoneAppId app_id;
        ImU32 col_top;
        ImU32 col_bot;
        bool coming_soon;
        bool is_lock_btn;
    };

    constexpr HomeAppEntry kGridApps[8] = {
        {"Telemetry",  PhoneAppId::Telemetry,      IM_COL32(56, 189, 248, 255),  IM_COL32(2, 132, 199, 255),   false, false},
        {"Job",        PhoneAppId::Dispatch,       IM_COL32(251, 191, 36, 255),  IM_COL32(217, 119, 6, 255),   false, false},
        {"Nismara ID", PhoneAppId::NismaraId,      IM_COL32(167, 139, 250, 255), IM_COL32(109, 40, 217, 255),  false, false},
        {"Garage",     PhoneAppId::Garage,         IM_COL32(52, 211, 153, 255),  IM_COL32(5, 150, 105, 255),   false, false},
        {"Penalty",    PhoneAppId::PenaltyPoints,  IM_COL32(239, 68, 68, 255),   IM_COL32(185, 28, 28, 255),   false, false},
        {"Wallpapers", PhoneAppId::Wallpapers,     IM_COL32(232, 121, 249, 255), IM_COL32(147, 51, 234, 255),  false, false},
        {"Settings",   PhoneAppId::Settings,       IM_COL32(148, 163, 184, 255), IM_COL32(71, 85, 105, 255),   false, false},
        {"Lock",       PhoneAppId::None,           IM_COL32(75, 85, 99, 255),    IM_COL32(31, 41, 55, 255),    false, true}
    };

    constexpr int kCols = 4;
    constexpr float kIconSize = 56.0f;
    const float grid_pad_x = 22.0f;
    const float avail_grid_w = screen_w - grid_pad_x * 2.0f;
    const float col_spacing = (avail_grid_w - kCols * kIconSize) / static_cast<float>(kCols - 1);
    const float grid_start_y = screen_min.y + 194.0f;
    const float row_step_y = 94.0f;

    ImFont* sm_font = (font_small_ != nullptr) ? font_small_ : ImGui::GetFont();

    for (int i = 0; i < 8; ++i) {
        const int row = i / kCols;
        const int col = i % kCols;
        const float ix = screen_min.x + grid_pad_x + col * (kIconSize + col_spacing);
        const float iy = grid_start_y + row * row_step_y;

        ImGui::SetCursorScreenPos(ImVec2(ix, iy));
        char btn_id[32];
        std::snprintf(btn_id, sizeof(btn_id), "##HomeGridIcon_%d", i);
        if (ImGui::InvisibleButton(btn_id, ImVec2(kIconSize, kIconSize + 20.0f))) {
            if (kGridApps[i].is_lock_btn) {
                LockPhone();
            } else {
                OpenApp(kGridApps[i].app_id);
            }
        }
        const bool hovered = ImGui::IsItemHovered();

        DrawAppSquircleIcon(
            draw_list,
            ImVec2(ix, iy),
            kIconSize,
            kGridApps[i].col_top,
            kGridApps[i].col_bot,
            kGridApps[i].app_id,
            kGridApps[i].is_lock_btn,
            hovered);

        // Pixel-perfect centered "SOON" mini badge on Events
        if (kGridApps[i].coming_soon) {
            const char* soon_txt = "SOON";
            const ImVec2 soon_sz = sm_font->CalcTextSizeA(10.5f, FLT_MAX, 0.0f, soon_txt);
            const float bw = soon_sz.x + 10.0f;
            const float bh = soon_sz.y + 4.0f;
            const ImVec2 badge_min(ix + kIconSize - bw + 4.0f, iy - 4.0f);
            const ImVec2 badge_max(badge_min.x + bw, badge_min.y + bh);

            draw_list->AddRectFilled(badge_min, badge_max, IM_COL32(239, 68, 68, 248), bh * 0.5f);
            draw_list->AddText(
                sm_font,
                10.5f,
                ImVec2(std::floor(badge_min.x + (bw - soon_sz.x) * 0.5f), std::floor(badge_min.y + (bh - soon_sz.y) * 0.5f)),
                IM_COL32(255, 255, 255, 255),
                soon_txt);
        }

        // App Label centered underneath icon
        DrawTextCenteredX(
            draw_list,
            sm_font,
            12.5f,
            ix + kIconSize * 0.5f,
            iy + kIconSize + 6.0f,
            IM_COL32(248, 250, 255, 250),
            kGridApps[i].label);
    }

    // 3. Page Indicator Dots above Dock
    const float center_x = (screen_min.x + screen_max.x) * 0.5f;
    draw_list->AddCircleFilled(ImVec2(center_x - 6.0f, screen_max.y - 114.0f), 3.2f, IM_COL32(255, 255, 255, 235));
    draw_list->AddCircleFilled(ImVec2(center_x + 6.0f, screen_max.y - 114.0f), 3.0f, IM_COL32(255, 255, 255, 90));

    // 4. Bottom Frosted Glass iOS Dock (4 Core Apps)
    const ImVec2 dock_min(screen_min.x + 16.0f, screen_max.y - 98.0f);
    const ImVec2 dock_max(screen_max.x - 16.0f, screen_max.y - 24.0f);
    draw_list->AddRectFilled(dock_min, dock_max, IM_COL32(16, 23, 40, 195), 26.0f);
    draw_list->AddRect(dock_min, dock_max, IM_COL32(255, 255, 255, 52), 26.0f);

    constexpr HomeAppEntry kDockApps[4] = {
        {"Telemetry",  PhoneAppId::Telemetry, IM_COL32(56, 189, 248, 255),  IM_COL32(2, 132, 199, 255),  false, false},
        {"Job",        PhoneAppId::Dispatch,  IM_COL32(251, 191, 36, 255),  IM_COL32(217, 119, 6, 255),  false, false},
        {"Nismara ID", PhoneAppId::NismaraId, IM_COL32(167, 139, 250, 255), IM_COL32(109, 40, 217, 255), false, false},
        {"Settings",   PhoneAppId::Settings,  IM_COL32(148, 163, 184, 255), IM_COL32(71, 85, 105, 255),  false, false}
    };

    constexpr float kDockIconSize = 50.0f;
    const float dock_inner_w = (dock_max.x - dock_min.x) - 28.0f;
    const float dock_spacing = (dock_inner_w - 4 * kDockIconSize) / 3.0f;
    const float dock_icon_y = dock_min.y + ((dock_max.y - dock_min.y) - kDockIconSize) * 0.5f;

    for (int d = 0; d < 4; ++d) {
        const float dx = dock_min.x + 14.0f + d * (kDockIconSize + dock_spacing);
        ImGui::SetCursorScreenPos(ImVec2(dx, dock_icon_y));
        char dock_id[32];
        std::snprintf(dock_id, sizeof(dock_id), "##DockApp_%d", d);
        if (ImGui::InvisibleButton(dock_id, ImVec2(kDockIconSize, kDockIconSize))) {
            OpenApp(kDockApps[d].app_id);
        }
        const bool hovered = ImGui::IsItemHovered();
        if (hovered) {
            hover_pill_text_ = kDockApps[d].label;
            hover_pill_cx_ = dx + kDockIconSize * 0.5f;
            hover_pill_y_ = dock_min.y - 16.0f;
        }

        DrawAppSquircleIcon(
            draw_list,
            ImVec2(dx, dock_icon_y),
            kDockIconSize,
            kDockApps[d].col_top,
            kDockApps[d].col_bot,
            kDockApps[d].app_id,
            false,
            hovered);
    }
}

void TabletGui::RenderActiveAppView(
    ImDrawList* draw_list,
    const ImVec2& screen_min,
    const ImVec2& screen_max,
    const nismara::core::TelemetrySnapshot& snap,
    const nismara::core::DriverAccountProfile& account) {
    const float screen_w = screen_max.x - screen_min.x;

    // App Navigation Bar below Dynamic Island
    const float nav_y = screen_min.y + 48.0f;
    ImGui::SetCursorScreenPos(ImVec2(screen_min.x + 14.0f, nav_y));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.23f, 0.38f, 0.80f));
    if (active_app_ == PhoneAppId::Garage && selected_fleet_idx_ >= 0) {
        if (ImGui::Button("< Garage", ImVec2(76.0f, 26.0f))) {
            selected_fleet_idx_ = -1;
        }
    } else {
        if (ImGui::Button("< Home", ImVec2(72.0f, 26.0f))) {
            UnlockToHome();
        }
    }
    ImGui::PopStyleColor();

    const char* app_title = "Nismara App";
    if (active_app_ == PhoneAppId::Garage && selected_fleet_idx_ >= 0) {
        app_title = "Vehicle Detail";
    } else {
        switch (active_app_) {
            case PhoneAppId::Telemetry:      app_title = "Telemetry"; break;
            case PhoneAppId::Dispatch:       app_title = "Job"; break;
            case PhoneAppId::NismaraId:      app_title = "Nismara ID"; break;
            case PhoneAppId::Garage:         app_title = "My Garage"; break;
            case PhoneAppId::PenaltyPoints:  app_title = "Penalty Points"; break;
            case PhoneAppId::Wallpapers:     app_title = "Wallpapers"; break;
            case PhoneAppId::Settings:       app_title = "Settings"; break;
            default: break;
        }
    }

    DrawTextCenteredX(
        draw_list,
        font_semibold_,
        16.5f,
        (screen_min.x + screen_max.x) * 0.5f,
        nav_y + 4.0f,
        IM_COL32(255, 255, 255, 250),
        app_title);

    ImGui::SetCursorScreenPos(ImVec2(screen_max.x - 62.0f, nav_y));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.20f, 0.30f, 0.72f));
    if (ImGui::Button("Lock", ImVec2(48.0f, 26.0f))) {
        LockPhone();
    }
    ImGui::PopStyleColor();

    draw_list->AddLine(
        ImVec2(screen_min.x + 14.0f, nav_y + 31.0f),
        ImVec2(screen_max.x - 14.0f, nav_y + 31.0f),
        IM_COL32(255, 255, 255, 28));

    // Scrollable App Viewport inside straight vertical area of OLED screen
    const ImVec2 content_pos(screen_min.x + 12.0f, nav_y + 36.0f);
    const ImVec2 content_size(screen_w - 24.0f, (screen_max.y - 32.0f) - content_pos.y);

    ImGui::SetCursorScreenPos(content_pos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    if (ImGui::BeginChild("##PhoneAppViewport", content_size, false)) {
        // Restore proper inner WindowPadding (14px x 12px) for all bordered cards inside apps!
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.10f, 0.18f, 0.86f));

        switch (active_app_) {
            case PhoneAppId::Telemetry:
                RenderAppTelemetry(snap);
                break;
            case PhoneAppId::Dispatch:
                RenderAppDispatch(snap);
                break;
            case PhoneAppId::NismaraId:
                RenderAppNismaraId(account);
                break;
            case PhoneAppId::Garage:
                RenderAppGarage(snap, account);
                break;
            case PhoneAppId::PenaltyPoints:
                RenderAppPenaltyPoints(snap, account);
                break;
            case PhoneAppId::Wallpapers:
                RenderAppWallpapers(account);
                break;
            case PhoneAppId::Settings:
            default:
                RenderAppSettings(account);
                break;
        }

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void TabletGui::RenderAppTelemetry(const nismara::core::TelemetrySnapshot& snap) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);

    // Segmented Control: [Cockpit] [Powertrain] [Diagnostik]
    const char* const kTelTabs[3] = {"Cockpit", "Powertrain", "Diagnostik"};
    RenderSegmentedControl("##TelTabs", &telemetry_tab_, kTelTabs, 3, 30.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float avail_w = ImGui::GetContentRegionAvail().x;

    if (telemetry_tab_ == 0) {
        // ====================================================================
        // TAB 0: COCKPIT CLUSTER (Solace Modern Cockpit)
        // ====================================================================

        // --- 1. HERO SPEEDOMETER & ECO-RPM GLASS CARD ---
        const ImVec2 hero_min = ImGui::GetCursorScreenPos();
        const float hero_h = 194.0f;
        const ImVec2 hero_max(hero_min.x + avail_w, hero_min.y + hero_h);
        DrawSolaceGlassCard(dl, hero_min, hero_max, 14.0f);

        // Header Pill & Badges
        DrawSolacePillBadge(dl, font_semibold_, 11.5f, ImVec2(hero_min.x + 12.0f, hero_min.y + 10.0f),
                            "DIGITAL CLUSTER", SolaceTokens::c_primary, IM_COL32(56, 189, 248, 35));

        // Speed limit sign (if navigation speed limit active)
        float cur_right = hero_max.x - 12.0f;
        if (snap.speed_limit_kmh > 0.0f) {
            char limit_buf[16];
            std::snprintf(limit_buf, sizeof(limit_buf), "%.0f", snap.speed_limit_kmh);
            const float pill_w = DrawSolacePillBadge(dl, font_semibold_, 12.0f,
                                ImVec2(cur_right - 42.0f, hero_min.y + 9.0f),
                                limit_buf, IM_COL32(255, 255, 255, 255), IM_COL32(239, 68, 68, 220), 8.0f, 2.5f);
            cur_right -= (pill_w + 6.0f);
        }

        // Cruise Control Pill
        if (snap.cruise_control_kmh > 0.0f) {
            char cc_buf[32];
            std::snprintf(cc_buf, sizeof(cc_buf), "CRUISE %.0f", snap.cruise_control_kmh);
            DrawSolacePillBadge(dl, font_semibold_, 11.5f, ImVec2(cur_right - 92.0f, hero_min.y + 10.0f),
                                cc_buf, SolaceTokens::c_emerald, IM_COL32(52, 211, 153, 40));
        } else {
            DrawSolacePillBadge(dl, font_small_, 11.0f, ImVec2(cur_right - 80.0f, hero_min.y + 10.0f),
                                "CRUISE OFF", SolaceTokens::c_text_dim, IM_COL32(100, 116, 139, 30));
        }

        // Speedometer Digital Number & Transmission Box
        const float num_y = hero_min.y + 42.0f;
        char speed_str[16];
        std::snprintf(speed_str, sizeof(speed_str), "%.0f", snap.speed_kmh);
        
        // Large speed number
        ImFont* num_font = (font_large_num_ != nullptr) ? font_large_num_ : font_semibold_;
        dl->AddText(num_font, 36.0f, ImVec2(hero_min.x + 14.0f, num_y), SolaceTokens::c_text_bright, speed_str);
        const float num_w = num_font->CalcTextSizeA(36.0f, FLT_MAX, 0.0f, speed_str).x;

        // "km/h" label next to speed number
        dl->AddText(font_small_, 13.0f, ImVec2(hero_min.x + 18.0f + num_w, num_y + 18.0f),
                    SolaceTokens::c_text_muted, "KM/H");

        // Transmission Box on the right
        const float gear_box_w = 68.0f;
        const float gear_box_h = 44.0f;
        const ImVec2 gear_min(hero_max.x - gear_box_w - 14.0f, num_y - 2.0f);
        const ImVec2 gear_max(gear_min.x + gear_box_w, gear_min.y + gear_box_h);
        dl->AddRectFilled(gear_min, gear_max, SolaceTokens::c_card_sub_bg, 8.0f);
        dl->AddRect(gear_min, gear_max, IM_COL32(251, 191, 36, 140), 8.0f, 0, 1.2f);
        
        const std::string gear_text = FormatGearText(snap.displayed_gear);
        const ImVec2 g_sz = font_semibold_->CalcTextSizeA(18.0f, FLT_MAX, 0.0f, gear_text.c_str());
        dl->AddText(font_semibold_, 18.0f,
                    ImVec2(std::floor((gear_min.x + gear_max.x) * 0.5f - g_sz.x * 0.5f),
                           std::floor((gear_min.y + gear_max.y) * 0.5f - g_sz.y * 0.5f)),
                    SolaceTokens::c_amber, gear_text.c_str());

        // Dynamic Eco-Band RPM Meter
        const float rpm_y = hero_min.y + 98.0f;
        const float rpm_limit = (snap.engine_rpm_limit > 500.0f) ? snap.engine_rpm_limit : 2500.0f;
        DrawSolaceEcoRpmBar(dl, ImVec2(hero_min.x + 14.0f, rpm_y),
                            ImVec2(hero_max.x - 14.0f, rpm_y + 10.0f),
                            snap.engine_rpm, rpm_limit);

        // RPM Legend / Metrics Row
        const float rpm_lbl_y = rpm_y + 15.0f;
        char rpm_readout[32];
        std::snprintf(rpm_readout, sizeof(rpm_readout), "%.0f / %.0f RPM", snap.engine_rpm, rpm_limit);
        dl->AddText(font_small_, 12.0f, ImVec2(hero_min.x + 14.0f, rpm_lbl_y), SolaceTokens::c_text_muted, rpm_readout);

        // RPM Mode Pill
        const char* rpm_mode_str = "IDLE";
        ImU32 rpm_mode_col = SolaceTokens::c_text_muted;
        if (snap.engine_rpm >= 800.0f && snap.engine_rpm <= 1500.0f) {
            rpm_mode_str = "ECO PEAK";
            rpm_mode_col = SolaceTokens::c_emerald;
        } else if (snap.engine_rpm > 1500.0f && snap.engine_rpm <= 2100.0f) {
            rpm_mode_str = "HIGH POWER";
            rpm_mode_col = SolaceTokens::c_amber;
        } else if (snap.engine_rpm > 2100.0f) {
            rpm_mode_str = "REDLINE";
            rpm_mode_col = SolaceTokens::c_rose;
        }
        DrawSolacePillBadge(dl, font_small_, 11.0f, ImVec2(hero_max.x - 90.0f, rpm_lbl_y - 2.0f),
                            rpm_mode_str, rpm_mode_col, (rpm_mode_col & 0x00FFFFFF) | (0x28 << 24), 7.0f, 2.0f);

        // Sub-bar Trip Stats
        const float stat_y = hero_min.y + 148.0f;
        const float stat_w = (avail_w - 36.0f) * 0.5f;
        
        // Left mini box: Odometer
        const ImVec2 b1_min(hero_min.x + 12.0f, stat_y);
        const ImVec2 b1_max(b1_min.x + stat_w, stat_y + 34.0f);
        dl->AddRectFilled(b1_min, b1_max, SolaceTokens::c_card_sub_bg, 7.0f);
        dl->AddRect(b1_min, b1_max, IM_COL32(255, 255, 255, 14), 7.0f, 0, 1.0f);
        dl->AddText(font_small_, 11.0f, ImVec2(b1_min.x + 8.0f, b1_min.y + 4.0f), SolaceTokens::c_text_dim, "ODOMETER");
        char odo_buf[32];
        std::snprintf(odo_buf, sizeof(odo_buf), "%.0f km", snap.odometer_km);
        dl->AddText(font_semibold_, 13.5f, ImVec2(b1_min.x + 8.0f, b1_min.y + 16.0f), SolaceTokens::c_text_bright, odo_buf);

        // Right mini box: Fuel Range
        const ImVec2 b2_min(hero_max.x - stat_w - 12.0f, stat_y);
        const ImVec2 b2_max(b2_min.x + stat_w, stat_y + 34.0f);
        dl->AddRectFilled(b2_min, b2_max, SolaceTokens::c_card_sub_bg, 7.0f);
        dl->AddRect(b2_min, b2_max, IM_COL32(255, 255, 255, 14), 7.0f, 0, 1.0f);
        dl->AddText(font_small_, 11.0f, ImVec2(b2_min.x + 8.0f, b2_min.y + 4.0f), SolaceTokens::c_text_dim, "FUEL RANGE");
        char rng_buf[32];
        std::snprintf(rng_buf, sizeof(rng_buf), "%.0f km", snap.fuel_range_km);
        dl->AddText(font_semibold_, 13.5f, ImVec2(b2_min.x + 8.0f, b2_min.y + 16.0f), SolaceTokens::c_emerald, rng_buf);

        ImGui::Dummy(ImVec2(0.0f, hero_h + 10.0f));

        // --- 2. AUTOMOTIVE TELLTALE DASHBOARD LAMPS ---
        const ImVec2 tell_min = ImGui::GetCursorScreenPos();
        const float tell_h = 66.0f;
        const ImVec2 tell_max(tell_min.x + avail_w, tell_min.y + tell_h);
        DrawSolaceGlassCard(dl, tell_min, tell_max, 12.0f);

        dl->AddText(font_small_, 11.0f, ImVec2(tell_min.x + 12.0f, tell_min.y + 7.0f),
                    SolaceTokens::c_text_dim, "INSTRUMENT CLUSTER TELLTALES");

        const float lamp_y = tell_min.y + 24.0f;
        const float lamp_h = 32.0f;
        const float lamp_w = (avail_w - 24.0f - 5 * 6.0f) / 6.0f;

        // 6 Telltale lamps: (P), ENG, RET, BEAM, HAZD, DIFF
        const auto draw_lamp_at = [&](int idx, const char* label, bool active, ImU32 col) {
            const ImVec2 l_min(tell_min.x + 12.0f + idx * (lamp_w + 6.0f), lamp_y);
            const ImVec2 l_max(l_min.x + lamp_w, lamp_y + lamp_h);
            DrawSolaceTelltaleLamp(dl, font_semibold_, l_min, l_max, label, active, col);
        };

        draw_lamp_at(0, "(P)", snap.parking_brake, SolaceTokens::c_rose);
        draw_lamp_at(1, "ENG", snap.motor_brake, SolaceTokens::c_amber);
        char ret_buf[8] = "RET";
        if (snap.retarder_level > 0) std::snprintf(ret_buf, sizeof(ret_buf), "R%u", snap.retarder_level);
        draw_lamp_at(2, ret_buf, (snap.retarder_level > 0), SolaceTokens::c_amber);
        draw_lamp_at(3, "BEAM", snap.light_high_beam, SolaceTokens::c_primary);
        draw_lamp_at(4, "HAZD", snap.hazard_warning, SolaceTokens::c_amber);
        draw_lamp_at(5, "DIFF", snap.differential_lock, SolaceTokens::c_amber);

        ImGui::Dummy(ImVec2(0.0f, tell_h + 10.0f));

        // --- 3. NAVIGATION & ORIENTATION GLASS CARD ---
        const ImVec2 nav_min = ImGui::GetCursorScreenPos();
        const float nav_h = 138.0f;
        const ImVec2 nav_max(nav_min.x + avail_w, nav_min.y + nav_h);
        DrawSolaceGlassCard(dl, nav_min, nav_max, 14.0f);

        DrawSolacePillBadge(dl, font_semibold_, 11.5f, ImVec2(nav_min.x + 12.0f, nav_min.y + 10.0f),
                            "NAVIGATION & GPS", SolaceTokens::c_purple, IM_COL32(168, 85, 247, 35));

        // Compass orientation badge
        float deg = snap.heading_deg;
        while (deg < 0.0f) deg += 360.0f;
        while (deg >= 360.0f) deg -= 360.0f;
        const char* dir = "N";
        if (deg >= 337.5f || deg < 22.5f) dir = "N";
        else if (deg < 67.5f) dir = "NE";
        else if (deg < 112.5f) dir = "E";
        else if (deg < 157.5f) dir = "SE";
        else if (deg < 202.5f) dir = "S";
        else if (deg < 247.5f) dir = "SW";
        else if (deg < 292.5f) dir = "W";
        else dir = "NW";

        char comp_buf[32];
        std::snprintf(comp_buf, sizeof(comp_buf), "%s %.0f deg", dir, snap.heading_deg);
        DrawSolacePillBadge(dl, font_semibold_, 11.5f, ImVec2(nav_max.x - 100.0f, nav_min.y + 10.0f),
                            comp_buf, SolaceTokens::c_text_bright, IM_COL32(255, 255, 255, 22), 8.0f, 2.5f);

        // Active Route or Free Roam
        const float r_y = nav_min.y + 40.0f;
        if (snap.job_active) {
            std::string route_str = snap.origin_city + "  ->  " + snap.destination_city;
            dl->AddText(font_semibold_, 14.5f, ImVec2(nav_min.x + 14.0f, r_y), SolaceTokens::c_text_bright, route_str.c_str());

            char nav_dist_str[48];
            const float rem_km = snap.navigation_distance_m / 1000.0f;
            std::snprintf(nav_dist_str, sizeof(nav_dist_str), "Sisa Jarak: %.1f km", rem_km);
            dl->AddText(font_small_, 12.5f, ImVec2(nav_min.x + 14.0f, r_y + 22.0f), SolaceTokens::c_emerald, nav_dist_str);

            const float plan_km = (snap.planned_distance_km > 0) ? static_cast<float>(snap.planned_distance_km) : (rem_km + 1.0f);
            const float progress = 1.0f - std::clamp(rem_km / plan_km, 0.0f, 1.0f);
            DrawSolaceMeter(dl, ImVec2(nav_min.x + 14.0f, r_y + 42.0f), ImVec2(nav_max.x - 14.0f, r_y + 48.0f),
                            progress, SolaceTokens::c_emerald);
        } else {
            dl->AddText(font_semibold_, 14.0f, ImVec2(nav_min.x + 14.0f, r_y), SolaceTokens::c_amber, "MODE JELAJAH BEBAS (FREE ROAM)");
            dl->AddText(font_small_, 12.0f, ImVec2(nav_min.x + 14.0f, r_y + 22.0f), SolaceTokens::c_text_muted, "Tidak ada rute navigasi pekerjaan aktif");
        }

        // Monospace World GPS Coordinates
        char coord_buf[64];
        std::snprintf(coord_buf, sizeof(coord_buf), "GPS: X: %.1f | Y: %.1f | Z: %.1f", snap.pos_x, snap.pos_y, snap.pos_z);
        dl->AddText(font_small_, 11.5f, ImVec2(nav_min.x + 14.0f, nav_max.y - 24.0f), SolaceTokens::c_text_dim, coord_buf);

        ImGui::Dummy(ImVec2(0.0f, nav_h + 10.0f));

    } else if (telemetry_tab_ == 1) {
        // ====================================================================
        // TAB 1: POWERTRAIN & SENSORS
        // ====================================================================

        // 1. Truck Identity Card
        const ImVec2 trk_min = ImGui::GetCursorScreenPos();
        const float trk_h = 86.0f;
        const ImVec2 trk_max(trk_min.x + avail_w, trk_min.y + trk_h);
        DrawSolaceGlassCard(dl, trk_min, trk_max, 14.0f);

        DrawSolacePillBadge(dl, font_semibold_, 11.5f, ImVec2(trk_min.x + 12.0f, trk_min.y + 10.0f),
                            "ARMADA AKTIF", SolaceTokens::c_primary, IM_COL32(56, 189, 248, 35));

        const std::string truck_title = (!snap.truck_brand.empty() || !snap.truck_name.empty())
                                            ? (snap.truck_brand + " " + snap.truck_name)
                                            : "No Active Truck";
        dl->AddText(font_semibold_, 14.5f, ImVec2(trk_min.x + 14.0f, trk_min.y + 36.0f), SolaceTokens::c_text_bright, truck_title.c_str());

        // Engine Status badge on the right
        if (snap.engine_running) {
            DrawSolacePillBadge(dl, font_small_, 11.0f, ImVec2(trk_max.x - 90.0f, trk_min.y + 10.0f),
                                "RUNNING", SolaceTokens::c_emerald, IM_COL32(52, 211, 153, 35));
        } else {
            DrawSolacePillBadge(dl, font_small_, 11.0f, ImVec2(trk_max.x - 76.0f, trk_min.y + 10.0f),
                                "OFF", SolaceTokens::c_rose, IM_COL32(248, 113, 113, 35));
        }

        char sub_info[64];
        std::snprintf(sub_info, sizeof(sub_info), "Gear: %s  |  Odo: %.0f km",
                      FormatGearText(snap.displayed_gear).c_str(), snap.odometer_km);
        dl->AddText(font_small_, 12.0f, ImVec2(trk_min.x + 14.0f, trk_min.y + 58.0f), SolaceTokens::c_text_muted, sub_info);

        ImGui::Dummy(ImVec2(0.0f, trk_h + 10.0f));

        // 2. Fuel Tank & Consumption Card
        const ImVec2 fuel_min = ImGui::GetCursorScreenPos();
        const float fuel_h = 138.0f;
        const ImVec2 fuel_max(fuel_min.x + avail_w, fuel_min.y + fuel_h);
        DrawSolaceGlassCard(dl, fuel_min, fuel_max, 14.0f);

        DrawSolacePillBadge(dl, font_semibold_, 11.5f, ImVec2(fuel_min.x + 12.0f, fuel_min.y + 10.0f),
                            "BAHAN BAKAR", SolaceTokens::c_emerald, IM_COL32(52, 211, 153, 35));

        if (snap.fuel_warning) {
            DrawSolacePillBadge(dl, font_semibold_, 11.0f, ImVec2(fuel_max.x - 94.0f, fuel_min.y + 10.0f),
                                "! LOW FUEL", SolaceTokens::c_rose, IM_COL32(248, 113, 113, 40));
        }

        const float fuel_cap = (snap.fuel_capacity > 0.0f) ? snap.fuel_capacity : 700.0f;
        const float fuel_frac = std::clamp(snap.fuel_current / fuel_cap, 0.0f, 1.0f);

        // Big Liters
        char fuel_liters_buf[32];
        std::snprintf(fuel_liters_buf, sizeof(fuel_liters_buf), "%.0f L", snap.fuel_current);
        dl->AddText(font_semibold_, 26.0f, ImVec2(fuel_min.x + 14.0f, fuel_min.y + 36.0f), SolaceTokens::c_text_bright, fuel_liters_buf);

        char fuel_pct_buf[32];
        std::snprintf(fuel_pct_buf, sizeof(fuel_pct_buf), "%.0f%% dari %.0f L", fuel_frac * 100.0f, fuel_cap);
        dl->AddText(font_small_, 12.5f, ImVec2(fuel_min.x + 14.0f, fuel_min.y + 68.0f), SolaceTokens::c_text_muted, fuel_pct_buf);

        // Fuel Progress Bar
        const ImU32 fuel_bar_col = (fuel_frac < 0.15f) ? SolaceTokens::c_rose : SolaceTokens::c_emerald;
        DrawSolaceMeter(dl, ImVec2(fuel_min.x + 14.0f, fuel_min.y + 88.0f),
                        ImVec2(fuel_max.x - 14.0f, fuel_min.y + 96.0f), fuel_frac, fuel_bar_col);

        // Range & Consumption stats row
        char fuel_stats[64];
        std::snprintf(fuel_stats, sizeof(fuel_stats), "Range: %.0f km  |  Konsumsi: %.1f L/100km",
                      snap.fuel_range_km, snap.fuel_avg_consumption);
        dl->AddText(font_small_, 11.5f, ImVec2(fuel_min.x + 14.0f, fuel_min.y + 108.0f), SolaceTokens::c_text_dim, fuel_stats);

        ImGui::Dummy(ImVec2(0.0f, fuel_h + 10.0f));

        // 3. Fluid & Sensor 4-Grid
        const ImVec2 sens_min = ImGui::GetCursorScreenPos();
        const float sens_h = 138.0f;
        const ImVec2 sens_max(sens_min.x + avail_w, sens_min.y + sens_h);
        DrawSolaceGlassCard(dl, sens_min, sens_max, 14.0f);

        DrawSolacePillBadge(dl, font_semibold_, 11.5f, ImVec2(sens_min.x + 12.0f, sens_min.y + 10.0f),
                            "SENSOR & ELEKTRIKAL", SolaceTokens::c_amber, IM_COL32(251, 191, 36, 35));

        const float grid_y = sens_min.y + 36.0f;
        const float card_w = (avail_w - 36.0f) * 0.5f;
        const float card_h = 42.0f;

        const auto draw_sensor_cell = [&](const ImVec2& pos, const char* label, const char* val, ImU32 val_col) {
            dl->AddRectFilled(pos, ImVec2(pos.x + card_w, pos.y + card_h), SolaceTokens::c_card_sub_bg, 8.0f);
            dl->AddRect(pos, ImVec2(pos.x + card_w, pos.y + card_h), IM_COL32(255, 255, 255, 14), 8.0f, 0, 1.0f);
            dl->AddText(font_small_, 11.0f, ImVec2(pos.x + 8.0f, pos.y + 4.0f), SolaceTokens::c_text_dim, label);
            dl->AddText(font_semibold_, 15.0f, ImVec2(pos.x + 8.0f, pos.y + 19.0f), val_col, val);
        };

        // Oil Temp
        char oil_buf[16];
        std::snprintf(oil_buf, sizeof(oil_buf), "%.0f °C", snap.oil_temperature_c > 0.0f ? snap.oil_temperature_c : 85.0f);
        draw_sensor_cell(ImVec2(sens_min.x + 12.0f, grid_y), "OIL TEMP", oil_buf, SolaceTokens::c_text_bright);

        // Water Temp
        char water_buf[16];
        std::snprintf(water_buf, sizeof(water_buf), "%.0f °C", snap.water_temperature_c > 0.0f ? snap.water_temperature_c : 82.0f);
        draw_sensor_cell(ImVec2(sens_max.x - card_w - 12.0f, grid_y), "COOLANT TEMP", water_buf, SolaceTokens::c_text_bright);

        // Brake Air
        char brake_buf[16];
        std::snprintf(brake_buf, sizeof(brake_buf), "%.1f psi", snap.brake_air_pressure_psi > 0.0f ? snap.brake_air_pressure_psi : 115.0f);
        draw_sensor_cell(ImVec2(sens_min.x + 12.0f, grid_y + card_h + 8.0f), "BRAKE AIR", brake_buf, SolaceTokens::c_emerald);

        // Battery
        char batt_buf[16];
        std::snprintf(batt_buf, sizeof(batt_buf), "%.1f V", snap.battery_voltage > 0.0f ? snap.battery_voltage : 24.2f);
        draw_sensor_cell(ImVec2(sens_max.x - card_w - 12.0f, grid_y + card_h + 8.0f), "BATTERY", batt_buf, SolaceTokens::c_primary);

        ImGui::Dummy(ImVec2(0.0f, sens_h + 10.0f));

    } else {
        // ====================================================================
        // TAB 2: WEAR DIAGNOSTICS (Diagnostik Kerusakan & Kesehatan Armada)
        // ====================================================================

        // 1. Vehicle Health Overview Card
        const ImVec2 ov_min = ImGui::GetCursorScreenPos();
        const float ov_h = 92.0f;
        const ImVec2 ov_max(ov_min.x + avail_w, ov_min.y + ov_h);
        DrawSolaceGlassCard(dl, ov_min, ov_max, 14.0f);

        DrawSolacePillBadge(dl, font_semibold_, 11.5f, ImVec2(ov_min.x + 12.0f, ov_min.y + 10.0f),
                            "KESEHATAN STRUKTURAL", SolaceTokens::c_amber, IM_COL32(251, 191, 36, 35));

        const float max_wear = std::max({snap.wear_engine, snap.wear_transmission, snap.wear_chassis, snap.wear_cabin, snap.wear_wheels});
        const float health_pct = std::clamp((1.0f - max_wear) * 100.0f, 0.0f, 100.0f);

        // Big Health %
        char hlth_buf[32];
        std::snprintf(hlth_buf, sizeof(hlth_buf), "%.0f%%", health_pct);
        const ImU32 hlth_col = (health_pct > 85.0f) ? SolaceTokens::c_emerald : ((health_pct > 60.0f) ? SolaceTokens::c_amber : SolaceTokens::c_rose);
        dl->AddText(font_semibold_, 30.0f, ImVec2(ov_min.x + 14.0f, ov_min.y + 36.0f), hlth_col, hlth_buf);

        const char* hlth_desc = "KONDISI PRIMA";
        if (health_pct < 60.0f) hlth_desc = "PERLU SERVIS KRITIS!";
        else if (health_pct < 85.0f) hlth_desc = "TERDETEKSI KEAUSAN";
        dl->AddText(font_semibold_, 12.5f, ImVec2(ov_min.x + 100.0f, ov_min.y + 45.0f), SolaceTokens::c_text_bright, hlth_desc);
        dl->AddText(font_small_, 11.0f, ImVec2(ov_min.x + 100.0f, ov_min.y + 62.0f), SolaceTokens::c_text_dim, "Berdasarkan keausan suku cadang tertinggi");

        ImGui::Dummy(ImVec2(0.0f, ov_h + 10.0f));

        // 2. Mechanical Parts Wear Cards
        const ImVec2 wear_min = ImGui::GetCursorScreenPos();
        const float wear_h = 230.0f;
        const ImVec2 wear_max(wear_min.x + avail_w, wear_min.y + wear_h);
        DrawSolaceGlassCard(dl, wear_min, wear_max, 14.0f);

        DrawSolacePillBadge(dl, font_semibold_, 11.5f, ImVec2(wear_min.x + 12.0f, wear_min.y + 10.0f),
                            "RINCIAN SUKU CADANG", SolaceTokens::c_primary, IM_COL32(56, 189, 248, 35));

        const auto draw_wear_row = [&](float y, const char* name, float wear_val) {
            dl->AddText(font_small_, 12.5f, ImVec2(wear_min.x + 14.0f, y), SolaceTokens::c_text_bright, name);
            char pct_str[16];
            std::snprintf(pct_str, sizeof(pct_str), "%.1f%%", wear_val * 100.0f);
            const ImU32 bar_col = (wear_val < 0.15f) ? SolaceTokens::c_emerald : ((wear_val < 0.40f) ? SolaceTokens::c_amber : SolaceTokens::c_rose);
            
            const ImVec2 sz = font_semibold_->CalcTextSizeA(12.5f, FLT_MAX, 0.0f, pct_str);
            dl->AddText(font_semibold_, 12.5f, ImVec2(wear_max.x - sz.x - 14.0f, y), bar_col, pct_str);

            DrawSolaceMeter(dl, ImVec2(wear_min.x + 14.0f, y + 17.0f),
                            ImVec2(wear_max.x - 14.0f, y + 22.0f), wear_val, bar_col);
        };

        float cur_y = wear_min.y + 36.0f;
        draw_wear_row(cur_y, "Mesin (Engine)", snap.wear_engine); cur_y += 36.0f;
        draw_wear_row(cur_y, "Transmisi (Gearbox)", snap.wear_transmission); cur_y += 36.0f;
        draw_wear_row(cur_y, "Sasis (Chassis)", snap.wear_chassis); cur_y += 36.0f;
        draw_wear_row(cur_y, "Kabin (Cabin)", snap.wear_cabin); cur_y += 36.0f;
        draw_wear_row(cur_y, "Roda & Ban (Wheels)", snap.wear_wheels);

        ImGui::Dummy(ImVec2(0.0f, wear_h + 10.0f));

        // 3. Trailer & Cargo Wear (if attached)
        if (snap.trailer_connected) {
            const ImVec2 trl_min = ImGui::GetCursorScreenPos();
            const float trl_h = 100.0f;
            const ImVec2 trl_max(trl_min.x + avail_w, trl_min.y + trl_h);
            DrawSolaceGlassCard(dl, trl_min, trl_max, 14.0f);

            DrawSolacePillBadge(dl, font_semibold_, 11.5f, ImVec2(trl_min.x + 12.0f, trl_min.y + 10.0f),
                                "TRAILER & KARGO", SolaceTokens::c_rose, IM_COL32(248, 113, 113, 35));

            draw_wear_row(trl_min.y + 36.0f, "Trailer (Gandengan)", snap.wear_trailer);
            draw_wear_row(trl_min.y + 70.0f, "Kerusakan Kargo", snap.cargo_damage);

            ImGui::Dummy(ImVec2(0.0f, trl_h + 10.0f));
        }
    }

    ImGui::PopStyleVar();
}

void TabletGui::RenderAppDispatch(const nismara::core::TelemetrySnapshot& snap) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);

    // Segmented Control: [Waybill Rute] [Slip Paycheck]
    const char* const kJobTabs[2] = {"Waybill Rute", "Slip Paycheck"};
    RenderSegmentedControl("##JobTabs", &job_tab_, kJobTabs, 2);

    if (job_tab_ == 0) {
        // --- TAB 0: WAYBILL RUTE ---
        if (!snap.job_active) {
            if (ImGui::BeginChild("##NoJobCard", ImVec2(0.0f, 185.0f), true)) {
                ImGui::PushFont(font_semibold_);
                ImGui::TextColored(ImVec4(0.98f, 0.75f, 0.14f, 1.0f), "TIDAK ADA PEKERJAAN AKTIF");
                ImGui::PopFont();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::TextWrapped(
                    "Anda sedang dalam mode free-roam. Ambil pekerjaan di Freight Market, Cargo Market, Quick Job, atau World of Trucks di dalam game.");
                ImGui::Spacing();
                ImGui::TextDisabled("Status Telemetri: %s", snap.active_job_sync_status.c_str());

                if (snap.has_last_job_receipt) {
                    ImGui::Spacing();
                    if (ImGui::Button("Lihat Slip Paycheck Terakhir >", ImVec2(-1.0f, 28.0f))) {
                        job_tab_ = 1;
                    }
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            return;
        }

        // 1. Visual Route Timeline (Origin -> Destination)
        if (ImGui::BeginChild("##JobTimelineCard", ImVec2(0.0f, 195.0f), true)) {
            ImGui::PushFont(font_semibold_);
            ImGui::TextColored(ImVec4(0.20f, 0.83f, 0.60f, 1.0f), "RUTE PENGIRIMAN (%s)", snap.server_mode.c_str());
            ImGui::PopFont();
            ImGui::Spacing();

            // Origin
            ImGui::TextColored(ImVec4(0.20f, 0.83f, 0.60f, 1.0f), "[ORIGIN]");
            ImGui::SameLine();
            ImGui::TextWrapped("%s (%s)", snap.origin_city.c_str(), snap.origin_company.c_str());

            // Connector
            ImGui::TextColored(ImVec4(0.5f, 0.6f, 0.75f, 1.0f), "   | \n   v  (Rencana: %u km)", snap.planned_distance_km);

            // Destination
            ImGui::TextColored(ImVec4(0.98f, 0.44f, 0.52f, 1.0f), "[DESTINATION]");
            ImGui::SameLine();
            ImGui::TextWrapped("%s (%s)", snap.destination_city.c_str(), snap.destination_company.c_str());

            ImGui::Spacing();
            const float rem_km = snap.navigation_distance_m / 1000.0f;
            const float plan_km = static_cast<float>(snap.planned_distance_km);
            float progress = 0.0f;
            if (plan_km > 1.0f && rem_km >= 0.0f) {
                progress = std::clamp(1.0f - (rem_km / plan_km), 0.0f, 1.0f);
            }
            char dist_str[48];
            std::snprintf(dist_str, sizeof(dist_str), "%.1f km tersisa (%.0f%%)", rem_km, progress * 100.0f);
            RenderCompactProgressBar("Progress Navigasi", progress, dist_str, ImVec4(0.20f, 0.83f, 0.60f, 1.0f));
        }
        ImGui::EndChild();

        // 2. Cargo Manifest Card
        if (ImGui::BeginChild("##JobManifestCard", ImVec2(0.0f, 168.0f), true)) {
            ImGui::PushFont(font_semibold_);
            if (!snap.active_job_db_id.empty()) {
                ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "MANIFEST KARGO [JOB #%s]", snap.active_job_db_id.c_str());
            } else {
                ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "MANIFEST KARGO LOGISTIK");
            }
            ImGui::TextWrapped("%s", snap.cargo_name.c_str());
            ImGui::PopFont();

            ImGui::Text("Muatan: %.1f Ton  |  Pasar: %s", snap.cargo_mass_kg / 1000.0f, snap.job_market.c_str());
            const uint32_t max_cap_km = static_cast<uint32_t>(std::round(static_cast<float>(snap.planned_distance_km) * 1.10f));
            ImGui::TextColored(
                ImVec4(0.98f, 0.80f, 0.08f, 1.0f),
                "Tarif: %.2f NC/km  |  Maks Cap: %u km (+10%%)",
                snap.active_job_locked_price,
                max_cap_km);

            if (snap.active_job_is_special) {
                ImGui::TextColored(ImVec4(0.75f, 0.52f, 0.99f, 1.0f), "[KONTRAK SPESIAL VTC - REWARD 2X NC]");
            }
            ImGui::TextDisabled("Sinkronisasi: %s", snap.active_job_sync_status.c_str());
        }
        ImGui::EndChild();

        // 3. Telemetry & Damage Logbook Card
        if (ImGui::BeginChild("##JobLogbookCard", ImVec2(0.0f, 178.0f), true)) {
            ImGui::PushFont(font_semibold_);
            ImGui::TextColored(ImVec4(0.65f, 0.55f, 0.98f, 1.0f), "TELEMETRI TRIP & DAMAGE");
            ImGui::PopFont();
            ImGui::Spacing();

            const float accum_cargo_dmg = std::max(snap.cargo_damage, snap.job_accum_cargo_damage);
            char dmg_str[32];
            std::snprintf(dmg_str, sizeof(dmg_str), "%.1f%%", accum_cargo_dmg * 100.0f);
            RenderCompactProgressBar("Kerusakan Kargo (Akumulatif)", accum_cargo_dmg, dmg_str, WearFractionToColor(accum_cargo_dmg));

            ImGui::Spacing();
            const float accum_veh_dmg = std::max({
                snap.wear_engine,
                snap.wear_transmission,
                snap.wear_cabin,
                snap.wear_chassis,
                snap.wear_wheels,
                snap.job_accum_vehicle_damage
            });
            const float accum_trl_dmg = std::max(snap.wear_trailer, snap.job_accum_trailer_damage);
            ImGui::TextDisabled(
                "Akumulasi: Truk %.1f%%  |  Trailer %.1f%%",
                accum_veh_dmg * 100.0f,
                accum_trl_dmg * 100.0f);

            const float driven_so_far = (snap.job_start_odometer_km > 0.0f && snap.odometer_km >= snap.job_start_odometer_km)
                                            ? (snap.odometer_km - snap.job_start_odometer_km)
                                            : 0.0f;
            ImGui::Text("Jarak Ditempuh: %.1f km  |  BBM: %.1f L", driven_so_far, snap.job_fuel_consumed_liters);

            const bool is_race_miles = (snap.job_max_speed_kmh > 110.0f);
            ImGui::TextColored(
                is_race_miles ? ImVec4(0.98f, 0.44f, 0.52f, 1.0f) : ImVec4(0.20f, 0.83f, 0.60f, 1.0f),
                "Top Speed: %.0f km/h (%s)  |  Denda: %zu",
                snap.job_max_speed_kmh,
                is_race_miles ? "Race Miles" : "Real Miles",
                snap.job_fines.size());
        }
        ImGui::EndChild();
    } else {
        // --- TAB 1: SLIP PAYCHECK ---
        if (snap.has_last_job_receipt) {
            if (ImGui::BeginChild("##PaycheckSlipCard", ImVec2(0.0f, 320.0f), true)) {
                ImGui::PushFont(font_semibold_);
                ImGui::TextColored(ImVec4(0.98f, 0.80f, 0.08f, 1.0f), "SLIP GAJI & LAPORAN JOB #%s", snap.last_receipt_job_id.c_str());
                ImGui::PopFont();
                ImGui::TextDisabled("Mode: %s  |  Jarak: %d km  |  XP: +%d", snap.last_receipt_game_mode.c_str(), snap.last_receipt_distance_km, snap.last_receipt_xp);
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::PushFont(font_large_num_);
                ImGui::TextColored(ImVec4(0.98f, 0.82f, 0.15f, 1.0f), "%+d NC", snap.last_receipt_revenue_nc);
                ImGui::PopFont();
                ImGui::TextDisabled("Total Pendapatan Bersih (Net Revenue)");
                ImGui::Spacing();

                ImGui::Text("Pendapatan Kontrak Dasar:  +%d NC", snap.last_receipt_base_nc);
                if (snap.last_receipt_mp_bonus_nc > 0) {
                    ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "Bonus Server Multiplayer: +%d NC", snap.last_receipt_mp_bonus_nc);
                }
                if (snap.last_receipt_hardcore_nc > 0) {
                    ImGui::TextColored(ImVec4(0.20f, 0.83f, 0.60f, 1.0f), "Bonus Parkir Hardcore:    +%d NC", snap.last_receipt_hardcore_nc);
                }
                if (snap.last_receipt_nplus_nc > 0) {
                    ImGui::TextColored(ImVec4(0.75f, 0.52f, 0.99f, 1.0f), "Perks & Event Booster:    +%d NC", snap.last_receipt_nplus_nc);
                }
                ImGui::TextColored(ImVec4(0.98f, 0.44f, 0.52f, 1.0f), "Biaya Servis & BBM:       -%d NC", snap.last_receipt_cost_nc);
                ImGui::TextDisabled("Pajak Pendapatan VTC:     -%d NC", snap.last_receipt_tax_nc);

                if (snap.last_receipt_penalty_pts > 0) {
                    ImGui::TextColored(ImVec4(0.94f, 0.27f, 0.27f, 1.0f), "Poin Penalti Pelanggaran: +%d Poin", snap.last_receipt_penalty_pts);
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.20f, 0.83f, 0.60f, 1.0f), "STATUS: DIVERIFIKASI OLEH SERVER TRACKER");
            }
            ImGui::EndChild();
        } else {
            if (ImGui::BeginChild("##NoPaycheckCard", ImVec2(0.0f, 160.0f), true)) {
                ImGui::PushFont(font_semibold_);
                ImGui::TextColored(ImVec4(0.65f, 0.55f, 0.98f, 1.0f), "BELUM ADA SLIP PAYCHECK");
                ImGui::PopFont();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::TextWrapped("Slip paycheck akan dicetak dan ditampilkan di sini secara otomatis segera setelah Anda mengantar dan menyelesaikan pengiriman kargo di dalam game.");
            }
            ImGui::EndChild();
        }
    }

    ImGui::PopStyleVar();
}

void TabletGui::RenderAppNismaraId(const nismara::core::DriverAccountProfile& account) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);

    if (account.status != nismara::core::AccountSyncStatus::Authenticated) {
        if (ImGui::BeginChild("##UnauthIdCard", ImVec2(0.0f, 200.0f), true)) {
            ImGui::PushFont(font_semibold_);
            ImGui::TextColored(ImVec4(0.98f, 0.75f, 0.14f, 1.0f), "NISMARA ID NOT LINKED");
            ImGui::PopFont();
            ImGui::Spacing();
            ImGui::TextWrapped("%s", account.status_message.c_str());
            ImGui::Spacing();
            if (ImGui::Button("Open Settings to Link Token", ImVec2(-1.0f, 32.0f))) {
                OpenApp(PhoneAppId::Settings);
            }
            if (ImGui::Button("Retry Sync Now", ImVec2(-1.0f, 30.0f))) {
                nismara::core::ApiClient::Instance().SyncAccountAsync();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        return;
    }

    // 1. Apple Wallet Holographic Driver Pass
    if (ImGui::BeginChild("##DriverPassCard", ImVec2(0.0f, 200.0f), true)) {
        ImGui::PushFont(font_semibold_);
        ImGui::TextColored(ImVec4(0.65f, 0.55f, 0.98f, 1.0f), "NISMARA DIGITAL DRIVER PASS");
        ImGui::PopFont();

        ImGui::PushFont(font_semibold_);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", account.name.c_str());
        ImGui::PopFont();

        const ImVec4 rank_col = ParseHexColor(account.trucky_rank_color, ImVec4(0.85f, 0.65f, 1.0f, 1.0f));
        const ImVec4 role_col = ParseHexColor(account.trucky_role_color, ImVec4(0.98f, 0.75f, 0.14f, 1.0f));
        ImGui::TextColored(role_col, "[%s]", account.trucky_role.c_str());
        ImGui::SameLine();
        ImGui::TextColored(rank_col, "|  %s", account.trucky_rank.c_str());

        ImGui::Spacing();
        ImGui::TextDisabled("Discord ID: %s", account.discord_id.c_str());
        ImGui::TextDisabled("SteamID64:  %s", account.steam_id.empty() ? "Not linked" : account.steam_id.c_str());

        ImGui::Spacing();
        // Barcode simulation using ImDrawList lines
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 cur = ImGui::GetCursorScreenPos();
        const float bc_w = ImGui::GetContentRegionAvail().x;
        const float bc_h = 24.0f;
        dl->AddRectFilled(cur, ImVec2(cur.x + bc_w, cur.y + bc_h), IM_COL32(255, 255, 255, 220), 4.0f);
        // Draw vertical barcode stripes
        float bx = cur.x + 12.0f;
        const float end_bx = cur.x + bc_w - 12.0f;
        int stripe_idx = 0;
        while (bx < end_bx) {
            float sw = ((stripe_idx % 3 == 0) ? 3.0f : ((stripe_idx % 2 == 0) ? 1.5f : 2.0f));
            if (bx + sw > end_bx) sw = end_bx - bx;
            dl->AddRectFilled(ImVec2(bx, cur.y + 3.0f), ImVec2(bx + sw, cur.y + bc_h - 3.0f), IM_COL32(15, 20, 30, 255));
            bx += sw + ((stripe_idx % 4 == 1) ? 3.5f : 2.0f);
            stripe_idx++;
        }
        ImGui::SetCursorScreenPos(ImVec2(cur.x, cur.y + bc_h + 4.0f));
    }
    ImGui::EndChild();

    // 2. Apple Card Nismara Coins Wallet
    if (ImGui::BeginChild("##WalletCard", ImVec2(0.0f, 172.0f), true)) {
        ImGui::PushFont(font_semibold_);
        ImGui::TextColored(ImVec4(0.98f, 0.80f, 0.08f, 1.0f), "NISMARA CASH & EXP CARD");
        ImGui::PopFont();

        ImGui::PushFont(font_large_num_);
        ImGui::TextColored(ImVec4(0.98f, 0.82f, 0.15f, 1.0f), "%.2f NC", account.total_nc);
        ImGui::PopFont();

        ImGui::Text("Level %d Driver   |   XP: %lld", account.level, static_cast<long long>(account.xp));

        const char* pen_status = "Aman";
        ImVec4 pen_col = ImVec4(0.20f, 0.83f, 0.60f, 1.0f);
        if (account.penalty_points >= 50) {
            pen_status = "P3 Kritis";
            pen_col = ImVec4(0.94f, 0.27f, 0.27f, 1.0f);
        } else if (account.penalty_points >= 25) {
            pen_status = "P2 Keras";
            pen_col = ImVec4(0.98f, 0.45f, 0.08f, 1.0f);
        } else if (account.penalty_points >= 10) {
            pen_status = "P1 Peringatan";
            pen_col = ImVec4(0.98f, 0.75f, 0.14f, 1.0f);
        }

        ImGui::Text("Status Penalti:");
        ImGui::SameLine();
        ImGui::TextColored(pen_col, "%d / 50 pts (%s)", account.penalty_points, pen_status);
        ImGui::SameLine();
        ImGui::Text(" | Konvoi: %d", account.joined_convoy);

        ImGui::Spacing();
        if (ImGui::Button("Sinkronkan Saldo", ImVec2(-1.0f, 26.0f))) {
            nismara::core::ApiClient::Instance().SyncAccountAsync();
        }
    }
    ImGui::EndChild();

    // 3. Subscriptions & Perks
    if (ImGui::BeginChild("##PerksCard", ImVec2(0.0f, 168.0f), true)) {
        ImGui::PushFont(font_semibold_);
        ImGui::TextColored(ImVec4(0.20f, 0.83f, 0.60f, 1.0f), "STATUS PERKS & PROTEKSI");
        ImGui::PopFont();
        ImGui::Separator();

        ImGui::Text("Keanggotaan Nismara+:");
        ImGui::SameLine(ImGui::GetWindowWidth() - 95.0f);
        ImGui::TextColored(
            account.nismara_plus_active ? ImVec4(0.20f, 0.83f, 0.60f, 1.0f) : ImVec4(0.6f, 0.65f, 0.75f, 1.0f),
            account.nismara_plus_active ? "AKTIF" : "NONAKTIF");

        ImGui::Text("Asuransi Kargo (%d%%):", account.insurance_rating);
        ImGui::SameLine(ImGui::GetWindowWidth() - 95.0f);
        ImGui::TextColored(
            account.insurance_active ? ImVec4(0.20f, 0.83f, 0.60f, 1.0f) : ImVec4(0.97f, 0.44f, 0.44f, 1.0f),
            account.insurance_active ? "TERLINDUNGI" : "KADALUARSA");

        ImGui::Text("NC Voucher Booster:");
        ImGui::SameLine(ImGui::GetWindowWidth() - 95.0f);
        if (account.nc_boost_active) {
            ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "+%.0f%% NC", account.nc_boost_multiplier * 100.0f);
        } else {
            ImGui::TextDisabled("TIDAK ADA");
        }

        if (account.top_manager_active) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.98f, 0.80f, 0.08f, 1.0f), "[PENGHARGAAN] Top Manager (%s)", account.top_manager_month.c_str());
        }
    }
    ImGui::EndChild();

    ImGui::PopStyleVar();
}

void TabletGui::RenderFleetDetailInspector(
    const nismara::core::DriverFleetVehicle& fl,
    const nismara::core::TelemetrySnapshot& snap,
    bool is_active_in_game) {
    (void)snap;
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);

    // 1. Vehicle Identity & Status Card
    if (ImGui::BeginChild("##InspectorProfileCard", ImVec2(0.0f, 185.0f), true)) {
        ImGui::PushFont(font_semibold_);
        ImGui::TextColored(
            is_active_in_game ? ImVec4(0.20f, 0.83f, 0.60f, 1.0f) : ImVec4(0.22f, 0.74f, 0.97f, 1.0f),
            "[%s] %s %s",
            fl.fleet_number.c_str(),
            fl.brand_name.c_str(),
            fl.model_name.c_str());
        ImGui::PopFont();

        if (!fl.custom_name.empty()) {
            ImGui::TextDisabled("\"%s\"", fl.custom_name.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (is_active_in_game) {
            ImGui::TextColored(ImVec4(0.20f, 0.83f, 0.60f, 1.0f), "[ACTIVE IN-GAME TRUCK]");
        } else {
            ImGui::TextDisabled("[PARKED IN GARAGE BAY]");
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 110.0f);
        ImGui::TextColored(
            fl.has_insurance ? ImVec4(0.20f, 0.83f, 0.60f, 1.0f) : ImVec4(0.97f, 0.44f, 0.44f, 1.0f),
            fl.has_insurance ? "INSURED" : "NO INSURANCE");

        ImGui::Text("Chassis / Axles: %s", fl.wheels.empty() ? "4x2 Standard" : fl.wheels.c_str());
        ImGui::Text("In-Game ID: %s", fl.in_game_id.empty() ? "N/A" : fl.in_game_id.c_str());
        ImGui::TextDisabled(
            "DB Odometer: %.0f km %s",
            fl.odometer,
            is_active_in_game ? "(Live Telemetry Synced)" : "");
    }
    ImGui::EndChild();

    // 2. Component Wear Diagnostics Card
    if (ImGui::BeginChild("##InspectorWearCard", ImVec2(0.0f, 195.0f), true)) {
        ImGui::PushFont(font_semibold_);
        ImGui::TextColored(ImVec4(0.98f, 0.75f, 0.14f, 1.0f), "KOMPONEN & TINGKAT KEHAUSAN");
        ImGui::PopFont();
        ImGui::Spacing();

        auto draw_gauge = [](const char* name, float pct) {
            const float frac = std::clamp(pct / 100.0f, 0.0f, 1.0f);
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.1f%%", pct);
            RenderCompactProgressBar(name, frac, buf, WearFractionToColor(frac));
        };

        draw_gauge("Mesin (Engine)", fl.unfix_engine);
        draw_gauge("Transmisi (Gearbox)", fl.unfix_transmission);
        draw_gauge("Sistem Rem (Brakes)", fl.unfix_brakes);
        draw_gauge("Ban / Roda (Tires)", fl.unfix_tires);
    }
    ImGui::EndChild();

    // 3. Next Service & Maintenance Countdown Card
    if (ImGui::BeginChild("##InspectorMaintCard", ImVec2(0.0f, 260.0f), true)) {
        ImGui::PushFont(font_semibold_);
        ImGui::TextColored(ImVec4(0.65f, 0.55f, 0.98f, 1.0f), "JADWAL SERVIS & PERAWATAN BERKALA");
        ImGui::PopFont();
        ImGui::Spacing();

        if (fl.maintenance_needed) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.27f, 0.27f, 1.0f));
            ImGui::TextWrapped("PERHATIAN: Truk ini telah mencapai batas pemakaian dan memerlukan servis berkala!");
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        auto draw_maint_item = [fl](const char* label, double maint_target_km) {
            const double odo = fl.odometer;
            const double remaining = maint_target_km - odo;
            char target_str[64];
            std::snprintf(target_str, sizeof(target_str), "%s (Target %.0fk km)", label, maint_target_km / 1000.0);

            if (remaining <= 0.0) {
                char right_buf[48];
                std::snprintf(right_buf, sizeof(right_buf), "LEWAT %.0f km!", -remaining);
                RenderCompactProgressBar(target_str, 1.0f, right_buf, ImVec4(0.94f, 0.27f, 0.27f, 1.0f));
            } else {
                const float frac = (maint_target_km > 0.0) ? std::clamp(static_cast<float>(odo / maint_target_km), 0.0f, 1.0f) : 0.0f;
                char right_buf[48];
                std::snprintf(right_buf, sizeof(right_buf), "%.0f km tersisa", remaining);
                const ImVec4 col = (remaining < 5000.0) ? ImVec4(0.98f, 0.75f, 0.14f, 1.0f) : ImVec4(0.20f, 0.83f, 0.60f, 1.0f);
                RenderCompactProgressBar(target_str, frac, right_buf, col);
            }
        };

        draw_maint_item("Servis Mesin", fl.maint_engine);
        draw_maint_item("Servis Transmisi", fl.maint_transmission);
        draw_maint_item("Ganti Kampas Rem", fl.maint_brakes);
        draw_maint_item("Rotasi / Ganti Ban", fl.maint_tires);
    }
    ImGui::EndChild();

    // 4. Quick Actions
    if (ImGui::BeginChild("##InspectorActionsCard", ImVec2(0.0f, 96.0f), true)) {
        if (!is_active_in_game) {
            if (ImGui::Button("Kendarai Truk Ini (Manual Bind)", ImVec2(-1.0f, 32.0f))) {
                nismara::core::TelemetryState::Instance().SetManualFleetOverride(fl.fleet_number);
            }
            ImGui::TextDisabled("Mengaitkan nomor lambung %s ke telemetri truk saat ini.", fl.fleet_number.c_str());
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.20f, 0.83f, 0.60f, 1.0f));
            ImGui::Text("Sedang digunakan sebagai truk aktif di dalam game.");
            ImGui::PopStyleColor();
            if (ImGui::Button("Reset ke Deteksi Otomatis", ImVec2(-1.0f, 26.0f))) {
                nismara::core::TelemetryState::Instance().SetManualFleetOverride("");
            }
        }
    }
    ImGui::EndChild();

    ImGui::PopStyleVar();
}

void TabletGui::RenderAppGarage(
    const nismara::core::TelemetrySnapshot& snap,
    const nismara::core::DriverAccountProfile& account) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);

    if (account.status != nismara::core::AccountSyncStatus::Authenticated) {
        if (ImGui::BeginChild("##GarageUnauthCard", ImVec2(0.0f, 195.0f), true)) {
            ImGui::PushFont(font_semibold_);
            ImGui::TextColored(ImVec4(0.98f, 0.75f, 0.14f, 1.0f), "GARAGE NOT SYNCED");
            ImGui::PopFont();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextWrapped(
                "Link your Nismara Driver Token in Settings to synchronize your personal VTC garage and fleet vehicles with your in-game truck.");
            ImGui::Spacing();
            if (ImGui::Button("Open Settings", ImVec2(-1.0f, 30.0f))) {
                OpenApp(PhoneAppId::Settings);
            }
            if (ImGui::Button("Sync Garage Now", ImVec2(-1.0f, 28.0f))) {
                nismara::core::ApiClient::Instance().SyncAccountAsync();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        return;
    }

    // If a fleet vehicle was clicked, show the Detail Inspector sub-view!
    if (selected_fleet_idx_ >= 0 && selected_fleet_idx_ < static_cast<int>(account.fleets.size())) {
        RenderFleetDetailInspector(
            account.fleets[selected_fleet_idx_],
            snap,
            selected_fleet_idx_ == account.active_fleet_index);
        ImGui::PopStyleVar();
        return;
    }

    // Segmented Control: [Overview] [Armada (X)] [Fasilitas]
    char tab_fleet_title[32];
    std::snprintf(tab_fleet_title, sizeof(tab_fleet_title), "Armada (%d)", static_cast<int>(account.fleets.size()));
    const char* const kGarageTabs[3] = {"Overview", tab_fleet_title, "Fasilitas"};
    RenderSegmentedControl("##GarageTabs", &garage_tab_, kGarageTabs, 3);

    const bool has_matched_fleet =
        (account.active_fleet_index >= 0 &&
         account.active_fleet_index < static_cast<int>(account.fleets.size()));

    if (garage_tab_ == 0) {
        // --- TAB 0: OVERVIEW ---
        // 1. Active Truck Hero Banner
        if (ImGui::BeginChild("##GarageHeroOverview", ImVec2(0.0f, 168.0f), true)) {
            ImGui::PushFont(font_semibold_);
            ImGui::TextColored(ImVec4(0.20f, 0.83f, 0.60f, 1.0f), "ACTIVE IN-GAME VEHICLE");
            ImGui::PopFont();

            if (has_matched_fleet) {
                const auto& fl = account.fleets[account.active_fleet_index];
                std::string title = "[" + fl.fleet_number + "] " + fl.brand_name + " " + fl.model_name;
                if (!fl.custom_name.empty()) {
                    title += " (\"" + fl.custom_name + "\")";
                }
                ImGui::PushFont(font_semibold_);
                ImGui::TextWrapped("%s", title.c_str());
                ImGui::PopFont();

                ImGui::TextDisabled("Live Odo: %.0f km  |  Chassis: %s", snap.odometer_km, fl.wheels.empty() ? "4x2" : fl.wheels.c_str());
                ImGui::TextColored(
                    fl.maintenance_needed ? ImVec4(0.94f, 0.27f, 0.27f, 1.0f) : ImVec4(0.20f, 0.83f, 0.60f, 1.0f),
                    fl.maintenance_needed ? "Status: MEMERLUKAN SERVIS" : "Status: SEHAT (PRIMA)");

                ImGui::Spacing();
                if (ImGui::Button("Lihat Detail Truk Ini >", ImVec2(-1.0f, 28.0f))) {
                    selected_fleet_idx_ = account.active_fleet_index;
                }
            } else {
                const std::string in_game_truck =
                    (!snap.truck_brand.empty() || !snap.truck_name.empty())
                        ? (snap.truck_brand + " " + snap.truck_name)
                        : "No In-Game Truck Detected";
                ImGui::PushFont(font_semibold_);
                ImGui::TextColored(ImVec4(0.98f, 0.75f, 0.14f, 1.0f), "[UNREGISTERED] %s", in_game_truck.c_str());
                ImGui::PopFont();
                ImGui::TextDisabled("In-Game ID: %s", snap.truck_in_game_id.empty() ? "N/A" : snap.truck_in_game_id.c_str());
                ImGui::Spacing();
                ImGui::TextWrapped("Pilih salah satu armada di tab 'Armada' untuk mengaitkan secara manual.");
            }
        }
        ImGui::EndChild();

        // 2. Metrics & Facility Quick Summary
        if (ImGui::BeginChild("##GarageStatsOverview", ImVec2(0.0f, 175.0f), true)) {
            ImGui::PushFont(font_semibold_);
            ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "FASILITAS GARASI & BRANKAS");
            ImGui::PopFont();

            ImGui::Text(
                "Slot Garasi: %d / %d Unit Digunakan",
                account.garage.fleet_slot_used,
                account.garage.fleet_slot);
            ImGui::TextColored(
                ImVec4(0.98f, 0.80f, 0.08f, 1.0f),
                "Tiket Safebox: %d / %d Tiket",
                account.garage.safebox_stock,
                account.garage.max_penalty_tickets);

            ImGui::Spacing();
            const float fuel_frac = (account.garage.fuel_capacity > 0.0)
                                        ? std::clamp(
                                              static_cast<float>(account.garage.fuel_stock / account.garage.fuel_capacity),
                                              0.0f,
                                              1.0f)
                                        : 0.0f;
            char g_fuel_str[48];
            std::snprintf(
                g_fuel_str,
                sizeof(g_fuel_str),
                "%.0f / %.0f L",
                account.garage.fuel_stock,
                account.garage.fuel_capacity);
            RenderCompactProgressBar("Stok Bahan Bakar Garasi", fuel_frac, g_fuel_str, ImVec4(0.20f, 0.83f, 0.60f, 1.0f));

            ImGui::Spacing();
            if (ImGui::Button("Sinkronisasi Data Garasi", ImVec2(-1.0f, 26.0f))) {
                nismara::core::ApiClient::Instance().SyncAccountAsync();
            }
        }
        ImGui::EndChild();
    } else if (garage_tab_ == 1) {
        // --- TAB 1: FLEET ROSTER ---
        ImGui::PushFont(font_semibold_);
        ImGui::TextColored(ImVec4(0.65f, 0.55f, 0.98f, 1.0f), "DAFTAR ARMADA MILIK ANDA (%d)", static_cast<int>(account.fleets.size()));
        ImGui::PopFont();
        ImGui::TextDisabled("Klik salah satu unit untuk membuka rincian lengkap & jadwal servis.");
        ImGui::Spacing();

        for (int i = 0; i < static_cast<int>(account.fleets.size()); ++i) {
            const auto& fl = account.fleets[i];
            const bool is_active = (i == account.active_fleet_index);

            ImGui::PushID(i);
            if (is_active) {
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.83f, 0.60f, 0.90f));
            }

            char card_id[32];
            std::snprintf(card_id, sizeof(card_id), "##FleetCard_%d", i);
            if (ImGui::BeginChild(card_id, ImVec2(0.0f, 114.0f), true)) {
                ImGui::PushFont(font_semibold_);
                ImGui::TextColored(
                    is_active ? ImVec4(0.20f, 0.83f, 0.60f, 1.0f) : ImVec4(0.22f, 0.74f, 0.97f, 1.0f),
                    "[%s] %s %s",
                    fl.fleet_number.c_str(),
                    fl.brand_name.c_str(),
                    fl.model_name.c_str());
                ImGui::PopFont();

                if (!fl.custom_name.empty()) {
                    ImGui::TextDisabled("\"%s\"", fl.custom_name.c_str());
                }

                ImGui::TextDisabled("Odo: %.0f km  |  %s  |  %s", fl.odometer, fl.wheels.empty() ? "4x2" : fl.wheels.c_str(), fl.has_insurance ? "Insured" : "Uninsured");

                if (is_active) {
                    ImGui::PushFont(font_semibold_);
                    ImGui::TextColored(ImVec4(0.20f, 0.83f, 0.60f, 1.0f), "[ACTIVE IN-GAME]");
                    ImGui::PopFont();
                    ImGui::SameLine(ImGui::GetWindowWidth() - 110.0f);
                    if (ImGui::Button("Detail Truk >", ImVec2(95.0f, 22.0f))) {
                        selected_fleet_idx_ = i;
                    }
                } else {
                    if (ImGui::Button("Detail Truk >", ImVec2(100.0f, 24.0f))) {
                        selected_fleet_idx_ = i;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Gunakan Truk Ini", ImVec2(-1.0f, 24.0f))) {
                        nismara::core::TelemetryState::Instance().SetManualFleetOverride(fl.fleet_number);
                    }
                }
            }
            ImGui::EndChild();

            if (is_active) {
                ImGui::PopStyleColor();
            }
            ImGui::PopID();
        }
    } else {
        // --- TAB 2: FASILITAS & SAFEBOX ---
        if (ImGui::BeginChild("##FacilityDetailsCard", ImVec2(0.0f, 175.0f), true)) {
            ImGui::PushFont(font_semibold_);
            ImGui::TextColored(ImVec4(0.20f, 0.83f, 0.60f, 1.0f), "FASILITAS GARASI UTAMA");
            ImGui::PopFont();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Status Garasi: %s", account.garage.has_garage ? "Operasional Aktif" : "Belum Memiliki Garasi");
            ImGui::Text("Slot Armada: Level %d (%d / %d Terisi)", account.garage.fleet_slot_level, account.garage.fleet_slot_used, account.garage.fleet_slot);
            ImGui::Text("Tangki BBM: Level %d (Kapasitas %.0f L)", account.garage.fuel_tank_level, account.garage.fuel_capacity);
            ImGui::Text("Tingkat Brankas (Safebox): Level %d", account.garage.safebox_level);
            ImGui::Spacing();
            ImGui::TextDisabled("Biaya Operasional: %.2f NC / hari", account.garage.operational_cost);
        }
        ImGui::EndChild();

        if (ImGui::BeginChild("##SafeboxDetailsCard", ImVec2(0.0f, 165.0f), true)) {
            ImGui::PushFont(font_semibold_);
            ImGui::TextColored(ImVec4(0.98f, 0.80f, 0.08f, 1.0f), "BRANKAS TIKET HAPUS PENALTI (SAFEBOX)");
            ImGui::PopFont();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Koleksi Tiket: %d / %d Tiket", account.garage.safebox_stock, account.garage.max_penalty_tickets);
            ImGui::Spacing();
            ImGui::TextWrapped("Tiket safebox diperoleh dari event atau upgrade garasi. 1 tiket dapat digunakan untuk menghapus 1 poin penalti tanpa biaya NC.");
            ImGui::Spacing();

            if (ImGui::Button("Buka Aplikasi Penalty Points", ImVec2(-1.0f, 30.0f))) {
                OpenApp(PhoneAppId::PenaltyPoints);
            }
        }
        ImGui::EndChild();

        if (ImGui::BeginChild("##FuelStorageCard", ImVec2(0.0f, 140.0f), true)) {
            ImGui::PushFont(font_semibold_);
            ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "TANGKI BAHAN BAKAR GARASI");
            ImGui::PopFont();
            ImGui::Separator();
            ImGui::Spacing();

            const float fuel_frac = (account.garage.fuel_capacity > 0.0)
                                        ? std::clamp(
                                              static_cast<float>(account.garage.fuel_stock / account.garage.fuel_capacity),
                                              0.0f,
                                              1.0f)
                                        : 0.0f;
            char g_fuel_str[48];
            std::snprintf(
                g_fuel_str,
                sizeof(g_fuel_str),
                "%.0f / %.0f L",
                account.garage.fuel_stock,
                account.garage.fuel_capacity);
            RenderCompactProgressBar("Kapasitas Tangki", fuel_frac, g_fuel_str, ImVec4(0.22f, 0.74f, 0.97f, 1.0f));
            ImGui::Spacing();
            ImGui::TextDisabled("Garasi operasional terdaftar pada Nismara Logistics.");
        }
        ImGui::EndChild();
    }

    ImGui::PopStyleVar();
}

void TabletGui::RenderAppPenaltyPoints(
    const nismara::core::TelemetrySnapshot& snap,
    const nismara::core::DriverAccountProfile& account) {
    (void)snap;
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);

    // 1. Status Utama Poin Penalti Card (Always visible)
    const int32_t current_pts = account.penalty_points;
    const char* pen_status = "Aman";
    ImVec4 pen_col = ImVec4(0.20f, 0.83f, 0.60f, 1.0f); // Green
    if (current_pts >= 50) {
        pen_status = "Penalty 3 (Kritis)";
        pen_col = ImVec4(0.94f, 0.27f, 0.27f, 1.0f); // Red
    } else if (current_pts >= 25) {
        pen_status = "Penalty 2 (Peringatan Keras)";
        pen_col = ImVec4(0.98f, 0.45f, 0.08f, 1.0f); // Orange
    } else if (current_pts >= 10) {
        pen_status = "Penalty 1 (Peringatan)";
        pen_col = ImVec4(0.98f, 0.75f, 0.14f, 1.0f); // Yellow
    }

    if (ImGui::BeginChild("##PenaltyStatusCard", ImVec2(0.0f, 126.0f), true)) {
        ImGui::PushFont(font_semibold_);
        ImGui::TextColored(ImVec4(0.98f, 0.44f, 0.52f, 1.0f), "STATUS POIN PENALTI DRIVER");
        ImGui::PopFont();

        ImGui::PushFont(font_large_num_);
        ImGui::TextColored(pen_col, "%d", current_pts);
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::TextDisabled("/ 50 Maks");

        ImGui::SameLine(ImGui::GetWindowWidth() - 170.0f);
        ImGui::TextColored(pen_col, "%s", pen_status);

        const float progress = std::clamp(static_cast<float>(current_pts) / 50.0f, 0.0f, 1.0f);
        char bar_text[32];
        std::snprintf(bar_text, sizeof(bar_text), "%.0f%% Batas", progress * 100.0f);
        RenderCompactProgressBar("Tingkat Akumulasi Pelanggaran", progress, bar_text, pen_col);
    }
    ImGui::EndChild();

    // 2. Action Toast Notification Card (if any message from last action)
    if (!account.penalty_action_message.empty()) {
        const ImVec4 toast_col = account.penalty_action_is_error
                                     ? ImVec4(0.94f, 0.27f, 0.27f, 1.0f)
                                     : ImVec4(0.20f, 0.83f, 0.60f, 1.0f);
        if (ImGui::BeginChild("##PenaltyActionToast", ImVec2(0.0f, 52.0f), true)) {
            ImGui::TextColored(toast_col, "%s", account.penalty_action_message.c_str());
        }
        ImGui::EndChild();
    }

    // Segmented Control: [Tebus NC] [Tiket Garasi] [Aturan]
    const char* const kPenTabs[3] = {"Tebus NC", "Tiket Garasi", "Aturan"};
    RenderSegmentedControl("##PenTabs", &penalty_tab_, kPenTabs, 3);

    if (penalty_tab_ == 0) {
        // --- TAB 0: TEBUS NC ---
        if (ImGui::BeginChild("##PayViaNcCard", ImVec2(0.0f, 235.0f), true)) {
            ImGui::PushFont(font_semibold_);
            ImGui::TextColored(ImVec4(0.98f, 0.80f, 0.08f, 1.0f), "1. TEBUS MENGGUNAKAN NC");
            ImGui::PopFont();
            ImGui::Separator();

            ImGui::Text("Saldo Dompet: %.2f NC", account.total_nc);

            // Diskon Info (Booster + Nismara+ Akumulatif)
            if (account.discount_booster > 0 && account.discount_nismara_plus > 0) {
                ImGui::TextColored(
                    ImVec4(0.20f, 0.83f, 0.60f, 1.0f),
                    "Diskon: -%d NC (Booster 500 + Nismara+ 500)",
                    account.total_discount);
            } else if (account.discount_booster > 0) {
                ImGui::TextColored(
                    ImVec4(0.20f, 0.83f, 0.60f, 1.0f),
                    "Diskon: -%d NC (Server Booster)",
                    account.discount_booster);
            } else if (account.discount_nismara_plus > 0) {
                ImGui::TextColored(
                    ImVec4(0.20f, 0.83f, 0.60f, 1.0f),
                    "Diskon: -%d NC (Nismara+ Member)",
                    account.discount_nismara_plus);
            } else {
                ImGui::TextDisabled("Harga: %d NC / Poin (Normal)", account.point_price);
            }

            ImGui::Text("Biaya Satuan: %d NC / poin", account.final_point_price);
            ImGui::Spacing();

            // Clamping stepper
            const int max_payable = std::max(1, current_pts);
            if (penalty_pts_to_pay_ < 1) penalty_pts_to_pay_ = 1;
            if (penalty_pts_to_pay_ > max_payable) penalty_pts_to_pay_ = max_payable;

            ImGui::Text("Jumlah Tebus:");
            ImGui::SameLine();
            if (ImGui::Button(" - ##NC", ImVec2(28.0f, 24.0f))) {
                if (penalty_pts_to_pay_ > 1) penalty_pts_to_pay_--;
            }
            ImGui::SameLine();
            ImGui::Text("%d Poin", penalty_pts_to_pay_);
            ImGui::SameLine();
            if (ImGui::Button(" + ##NC", ImVec2(28.0f, 24.0f))) {
                if (penalty_pts_to_pay_ < current_pts) penalty_pts_to_pay_++;
            }
            ImGui::SameLine();
            if (ImGui::Button("Max##NC", ImVec2(44.0f, 24.0f))) {
                penalty_pts_to_pay_ = std::max(1, current_pts);
            }

            const int64_t total_nc_cost = static_cast<int64_t>(penalty_pts_to_pay_) * account.final_point_price;
            ImGui::TextColored(ImVec4(0.98f, 0.82f, 0.15f, 1.0f), "Total Biaya: %lld NC", total_nc_cost);

            ImGui::Spacing();
            const bool can_pay = (current_pts > 0) && (account.total_nc >= total_nc_cost);
            if (!can_pay) {
                ImGui::BeginDisabled();
            }
            char btn_label[64];
            std::snprintf(btn_label, sizeof(btn_label), "Bayar %d Poin (%lld NC)", penalty_pts_to_pay_, total_nc_cost);
            if (ImGui::Button(btn_label, ImVec2(-1.0f, 30.0f))) {
                nismara::core::ApiClient::Instance().PayPenaltyPointsAsync(penalty_pts_to_pay_);
            }
            if (!can_pay) {
                ImGui::EndDisabled();
                if (current_pts == 0) {
                    ImGui::TextDisabled("Poin penalti Anda sudah 0.");
                } else {
                    ImGui::TextColored(ImVec4(0.94f, 0.27f, 0.27f, 1.0f), "Saldo NC Anda tidak mencukupi.");
                }
            }
        }
        ImGui::EndChild();
    } else if (penalty_tab_ == 1) {
        // --- TAB 1: TIKET GARASI ---
        const int32_t safebox_avail = account.garage.safebox_stock > 0 ? account.garage.safebox_stock : account.safebox_tickets;
        const int32_t safebox_cap = account.garage.max_penalty_tickets > 0 ? account.garage.max_penalty_tickets : account.max_penalty_tickets;

        if (ImGui::BeginChild("##UseTicketsCard", ImVec2(0.0f, 205.0f), true)) {
            ImGui::PushFont(font_semibold_);
            ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "2. GUNAKAN TIKET GARASI (SAFEBOX)");
            ImGui::PopFont();
            ImGui::Separator();

            ImGui::Text("Tiket Safebox: %d / %d Tiket", safebox_avail, safebox_cap);
            ImGui::TextDisabled("1 Tiket = Menghapus 1 Poin Penalti");
            ImGui::Spacing();

            const int max_ticket_usable = std::max(1, std::min(safebox_avail, current_pts));
            if (penalty_tickets_to_use_ < 1) penalty_tickets_to_use_ = 1;
            if (penalty_tickets_to_use_ > max_ticket_usable) penalty_tickets_to_use_ = max_ticket_usable;

            ImGui::Text("Jumlah Tiket:");
            ImGui::SameLine();
            if (ImGui::Button(" - ##Tkt", ImVec2(28.0f, 24.0f))) {
                if (penalty_tickets_to_use_ > 1) penalty_tickets_to_use_--;
            }
            ImGui::SameLine();
            ImGui::Text("%d Tiket", penalty_tickets_to_use_);
            ImGui::SameLine();
            if (ImGui::Button(" + ##Tkt", ImVec2(28.0f, 24.0f))) {
                if (penalty_tickets_to_use_ < std::min(safebox_avail, current_pts)) penalty_tickets_to_use_++;
            }
            ImGui::SameLine();
            if (ImGui::Button("Max##Tkt", ImVec2(44.0f, 24.0f))) {
                penalty_tickets_to_use_ = std::max(1, std::min(safebox_avail, current_pts));
            }

            ImGui::Spacing();
            const bool can_use_ticket = (current_pts > 0) && (safebox_avail >= penalty_tickets_to_use_);
            if (!can_use_ticket) {
                ImGui::BeginDisabled();
            }
            char tkt_btn_label[64];
            std::snprintf(tkt_btn_label, sizeof(tkt_btn_label), "Gunakan %d Tiket Hapus Penalti", penalty_tickets_to_use_);
            if (ImGui::Button(tkt_btn_label, ImVec2(-1.0f, 30.0f))) {
                nismara::core::ApiClient::Instance().UsePenaltyTicketAsync(penalty_tickets_to_use_);
            }
            if (!can_use_ticket) {
                ImGui::EndDisabled();
                if (current_pts == 0) {
                    ImGui::TextDisabled("Poin penalti Anda sudah 0.");
                } else {
                    ImGui::TextColored(ImVec4(0.94f, 0.27f, 0.27f, 1.0f), "Tiket di Safebox tidak mencukupi.");
                }
            }
        }
        ImGui::EndChild();
    } else {
        // --- TAB 2: ATURAN & SANKSI PENALTI ---
        if (ImGui::BeginChild("##PenaltyRulesCard", ImVec2(0.0f, 270.0f), true)) {
            ImGui::PushFont(font_semibold_);
            ImGui::TextColored(ImVec4(0.65f, 0.55f, 0.98f, 1.0f), "ATURAN & TINGKAT SANKSI VTC");
            ImGui::PopFont();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.20f, 0.83f, 0.60f, 1.0f), "[0 - 9 Poin] Status Aman");
            ImGui::TextDisabled("Driver aktif normal, bebas klaim seluruh reward event & gaji penuh.");
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.98f, 0.75f, 0.14f, 1.0f), "[10 - 24 Poin] Penalty 1 (Peringatan)");
            ImGui::TextDisabled("Peringatan resmi. Pembatasan klaim reward bonus berkala.");
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.98f, 0.45f, 0.08f, 1.0f), "[25 - 49 Poin] Penalty 2 (Peringatan Keras)");
            ImGui::TextDisabled("Suspensi sementara dari keikutsertaan convoy resmi VTC.");
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.94f, 0.27f, 0.27f, 1.0f), "[>= 50 Poin] Penalty 3 (Kritis)");
            ImGui::TextDisabled("Sanksi nonaktif / pemecatan dari keanggotaan Nismara Logistics.");
        }
        ImGui::EndChild();
    }

    ImGui::PopStyleVar();
}

void TabletGui::RenderAppWallpapers(const nismara::core::DriverAccountProfile& account) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);

    // Segmented Control: [Preset iOS 18] [Foto Disk] [Warna Casing]
    const char* const kWallTabs[3] = {"Preset iOS 18", "Foto Disk", "Warna Casing"};
    RenderSegmentedControl("##WallTabs", &wallpapers_tab_, kWallTabs, 3);

    if (wallpapers_tab_ == 0) {
        // --- TAB 0: PRESET IOS 18 ---
        if (ImGui::BeginChild("##PresetWallpapersCard", ImVec2(0.0f, 220.0f), true)) {
            ImGui::PushFont(font_semibold_);
            ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "BUILT-IN NISMARA OS WALLPAPERS");
            ImGui::PopFont();
            ImGui::Spacing();

            for (int i = 0; i < 5; ++i) {
                ImGui::PushID(100 + i);
                ImGui::Text("%s", kPresetWallpapers[i].title);
                ImGui::SameLine(ImGui::GetWindowWidth() - 122.0f);
                if (ImGui::SmallButton("Lock")) {
                    lockscreen_wallpaper_spec_ = kPresetWallpapers[i].spec;
                    nismara::core::ConfigManager::Instance().SetLockscreenWallpaper(lockscreen_wallpaper_spec_);
                    nismara::core::ConfigManager::Instance().Save();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Home")) {
                    homescreen_wallpaper_spec_ = kPresetWallpapers[i].spec;
                    nismara::core::ConfigManager::Instance().SetHomescreenWallpaper(homescreen_wallpaper_spec_);
                    nismara::core::ConfigManager::Instance().Save();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        // Sync Website Profile Background
        if (ImGui::BeginChild("##WebProfileBgCard", ImVec2(0.0f, 146.0f), true)) {
            ImGui::PushFont(font_semibold_);
            ImGui::TextColored(ImVec4(0.98f, 0.80f, 0.08f, 1.0f), "WEBSITE PROFILE BACKGROUND");
            ImGui::PopFont();

            const bool downloading = nismara::core::ApiClient::Instance().IsDownloadingWallpaper();
            if (!account.background_url.empty()) {
                ImGui::TextDisabled("Synced from Nismara Web Profile");
                if (downloading) {
                    ImGui::BeginDisabled();
                }
                if (ImGui::Button("Set Lock", ImVec2(140.0f, 28.0f))) {
                    nismara::core::ApiClient::Instance().DownloadWallpaperFromUrlAsync(account.background_url, true, false);
                }
                ImGui::SameLine();
                if (ImGui::Button("Set Home", ImVec2(140.0f, 28.0f))) {
                    nismara::core::ApiClient::Instance().DownloadWallpaperFromUrlAsync(account.background_url, false, true);
                }
                if (ImGui::Button("Apply to Both Screens", ImVec2(-1.0f, 28.0f))) {
                    nismara::core::ApiClient::Instance().DownloadWallpaperFromUrlAsync(account.background_url, true, true);
                }
                if (downloading) {
                    ImGui::EndDisabled();
                }
            } else {
                ImGui::TextWrapped("Sync your Nismara account first to use your website profile background.");
            }

            const std::string dl_status = nismara::core::ApiClient::Instance().GetWallpaperDownloadStatus();
            if (!dl_status.empty()) {
                ImGui::TextColored(ImVec4(0.20f, 0.83f, 0.60f, 1.0f), "%s", dl_status.c_str());
            }
        }
        ImGui::EndChild();
    } else if (wallpapers_tab_ == 1) {
        // --- TAB 1: FOTO DISK ---
        if (ImGui::BeginChild("##LocalWallpapersCard", ImVec2(0.0f, 270.0f), true)) {
            ImGui::PushFont(font_semibold_);
            ImGui::TextColored(ImVec4(0.20f, 0.83f, 0.60f, 1.0f), "CUSTOM IMAGES (plugins/wallpapers/)");
            ImGui::PopFont();
            ImGui::SameLine(ImGui::GetWindowWidth() - 78.0f);
            if (ImGui::SmallButton("Rescan")) {
                TextureManager::Instance().RequestReload();
            }

            ImGui::TextDisabled("Place .png, .jpg, .webp, or .bmp in plugins/wallpapers/");
            ImGui::Spacing();

            const auto& files = TextureManager::Instance().GetAvailableFiles();
            if (files.empty()) {
                ImGui::TextDisabled("No image files found in plugins/wallpapers/ yet.");
            } else {
                for (size_t idx = 0; idx < files.size(); ++idx) {
                    ImGui::PushID(500 + static_cast<int>(idx));
                    ImGui::TextWrapped("%s", files[idx].c_str());
                    if (ImGui::SmallButton("Set Lock Screen")) {
                        lockscreen_wallpaper_spec_ = files[idx];
                        nismara::core::ConfigManager::Instance().SetLockscreenWallpaper(files[idx]);
                        nismara::core::ConfigManager::Instance().Save();
                        TextureManager::Instance().RequestReload();
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Set Home Screen")) {
                        homescreen_wallpaper_spec_ = files[idx];
                        nismara::core::ConfigManager::Instance().SetHomescreenWallpaper(files[idx]);
                        nismara::core::ConfigManager::Instance().Save();
                        TextureManager::Instance().RequestReload();
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
            }

            const auto& lock_err = TextureManager::Instance().GetLockscreenTexture().last_error;
            const auto& home_err = TextureManager::Instance().GetHomescreenTexture().last_error;
            if (!lock_err.empty()) {
                ImGui::TextColored(ImVec4(0.97f, 0.44f, 0.44f, 1.0f), "Lock: %s", lock_err.c_str());
            }
            if (!home_err.empty()) {
                ImGui::TextColored(ImVec4(0.97f, 0.44f, 0.44f, 1.0f), "Home: %s", home_err.c_str());
            }
        }
        ImGui::EndChild();
    } else {
        // --- TAB 2: WARNA CASING IPHONE 16 ---
        if (ImGui::BeginChild("##FrameColorCard", ImVec2(0.0f, 220.0f), true)) {
            ImGui::PushFont(font_semibold_);
            ImGui::TextColored(ImVec4(0.65f, 0.55f, 0.98f, 1.0f), "PILIHAN WARNA CASING IPHONE 16");
            ImGui::PopFont();
            ImGui::TextDisabled("Warna Aktif: %s", kFramePalettes[frame_color_idx_].name);
            ImGui::Spacing();

            for (int i = 0; i < 5; ++i) {
                const ImVec4 c = ImGui::ColorConvertU32ToFloat4(kFramePalettes[i].outer_body);
                ImGui::PushID(i);
                ImGui::PushStyleColor(ImGuiCol_Button, c);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(c.x * 1.15f, c.y * 1.15f, c.z * 1.15f, 1.0f));
                if (ImGui::Button(frame_color_idx_ == i ? "ON" : " ", ImVec2(42.0f, 34.0f))) {
                    frame_color_idx_ = i;
                    nismara::core::ConfigManager::Instance().SetPhoneFrameColor(i);
                    nismara::core::ConfigManager::Instance().Save();
                }
                ImGui::PopStyleColor(2);
                ImGui::SameLine();
                ImGui::Text("%s", kFramePalettes[i].name);
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }

    ImGui::PopStyleVar();
}

void TabletGui::RenderAppSettings(const nismara::core::DriverAccountProfile& account) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);

    // Group 1: General & Behavior
    if (ImGui::BeginChild("##LockBehaviorCard", ImVec2(0.0f, 118.0f), true)) {
        ImGui::PushFont(font_semibold_);
        ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "PERILAKU & SISTEM");
        ImGui::PopFont();
        ImGui::Spacing();
        if (ImGui::Checkbox("Selalu buka di Lock Screen (F11)", &auto_lock_on_close_)) {
            nismara::core::ConfigManager::Instance().SetAutoLockOnClose(auto_lock_on_close_);
            nismara::core::ConfigManager::Instance().Save();
        }
        ImGui::TextWrapped("Saat aktif, menekan tombol F11 untuk membuka HP akan selalu menampilkan Lock Screen terlebih dahulu.");
    }
    ImGui::EndChild();

    // Group 2: Cloud Sync & Authentication
    if (ImGui::BeginChild("##SyncSettingsCard", ImVec2(0.0f, 290.0f), true)) {
        ImGui::PushFont(font_semibold_);
        ImGui::TextColored(ImVec4(0.98f, 0.80f, 0.08f, 1.0f), "KONEKSI NISMARA CLOUD TRACKER");
        ImGui::PopFont();

        ImGui::TextDisabled("Alamat Endpoint Server:");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##ApiUrlInput", api_url_buffer_, sizeof(api_url_buffer_));

        if (ImGui::SmallButton("Tracker Server (:3100)")) {
            std::snprintf(api_url_buffer_, sizeof(api_url_buffer_), "http://localhost:3100");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Beta Web")) {
            std::snprintf(api_url_buffer_, sizeof(api_url_buffer_), "https://beta.nismara.web.id");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Localhost:3000")) {
            std::snprintf(api_url_buffer_, sizeof(api_url_buffer_), "http://localhost:3000");
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Driver Token (dari Dashboard -> Settings):");
        ImGui::SetNextItemWidth(-64.0f);
        ImGui::InputText(
            "##TokenInput",
            token_buffer_,
            sizeof(token_buffer_),
            mask_token_ ? ImGuiInputTextFlags_Password : ImGuiInputTextFlags_None);
        ImGui::SameLine();
        if (ImGui::Button(mask_token_ ? "Show" : "Hide", ImVec2(54.0f, 0.0f))) {
            mask_token_ = !mask_token_;
        }

        ImGui::Spacing();
        const bool is_syncing = nismara::core::ApiClient::Instance().IsSyncing();
        if (is_syncing) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button(is_syncing ? "Menyinkronkan..." : "Simpan & Sinkronkan Akun", ImVec2(-1.0f, 32.0f))) {
            nismara::core::ConfigManager::Instance().SetApiBaseUrl(api_url_buffer_);
            nismara::core::ConfigManager::Instance().SetDriverToken(token_buffer_);
            if (nismara::core::ConfigManager::Instance().Save()) {
                save_status_success_ = true;
                save_status_message_ = "Konfigurasi berhasil disimpan.";
            } else {
                save_status_success_ = false;
                save_status_message_ = "Gagal menulis nismara_config.dat.";
            }
            nismara::core::ApiClient::Instance().SyncAccountAsync();
        }
        if (is_syncing) {
            ImGui::EndDisabled();
        }

        ImGui::Spacing();
        ImGui::TextWrapped("Status: %s", account.status_message.c_str());
        if (!save_status_message_.empty()) {
            ImGui::TextColored(
                save_status_success_ ? ImVec4(0.20f, 0.83f, 0.60f, 1.0f) : ImVec4(0.97f, 0.44f, 0.44f, 1.0f),
                "%s",
                save_status_message_.c_str());
        }
    }
    ImGui::EndChild();

    // Group 3: App & Plugin Information
    if (ImGui::BeginChild("##AboutAppCard", ImVec2(0.0f, 120.0f), true)) {
        ImGui::PushFont(font_semibold_);
        ImGui::TextColored(ImVec4(0.65f, 0.55f, 0.98f, 1.0f), "TENTANG NISMARA TRACKER");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("Versi Plugin: v1.2.0 (x64 Release)");
        ImGui::TextDisabled("Engine: SCS Telemetry SDK v1.01");
        ImGui::TextDisabled("Graphics: DirectX 11 Hook (MinHook)");
    }
    ImGui::EndChild();

    ImGui::PopStyleVar();
}

} // namespace nismara::ui
