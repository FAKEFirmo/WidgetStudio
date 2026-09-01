#pragma once

#include "rendering/WallpaperPlacement.h"

#include <cstdint>
#include <shobjidl.h>
#include <string>
#include <unordered_map>
#include <wincodec.h>
#include <wrl/client.h>

namespace ws {

class WallpaperCache {
public:
    HRESULT Initialize();
    HRESULT Reload();

    HRESULT MonitorBitmap(
        const std::wstring& monitorId,
        const WallpaperMonitorGeometry& geometry,
        IWICBitmapSource** bitmap);

    [[nodiscard]] WallpaperPosition Position() const noexcept { return position_; }
    [[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }

private:
    struct MonitorEntry {
        WallpaperMonitorGeometry geometry{};
        Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    };

    HRESULT ResolveWallpaperPath(
        const WallpaperMonitorGeometry& geometry, std::wstring& path) const;
    HRESULT Decode(const std::wstring& path, IWICBitmapSource** source) const;
    HRESULT ComposeMonitor(
        IWICBitmapSource* source,
        const WallpaperMonitorGeometry& geometry,
        IWICBitmap** bitmap) const;

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory_;
    Microsoft::WRL::ComPtr<IDesktopWallpaper> desktopWallpaper_;
    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<IWICBitmapSource>> sources_;
    std::unordered_map<std::wstring, MonitorEntry> monitors_;
    WallpaperPosition position_{WallpaperPosition::Fill};
    std::uint32_t backgroundBgra_{0xFF000000u};
    std::uint64_t revision_{};
};

} // namespace ws
