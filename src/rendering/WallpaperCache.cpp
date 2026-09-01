#include "rendering/WallpaperCache.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <shobjidl.h>
#include <vector>
#include <windows.h>

namespace ws {
namespace {

bool SameGeometry(const WallpaperMonitorGeometry& a, const WallpaperMonitorGeometry& b) noexcept {
    return a.monitorX == b.monitorX && a.monitorY == b.monitorY &&
        a.monitorWidth == b.monitorWidth && a.monitorHeight == b.monitorHeight &&
        a.virtualX == b.virtualX && a.virtualY == b.virtualY &&
        a.virtualWidth == b.virtualWidth && a.virtualHeight == b.virtualHeight;
}

WallpaperPosition ConvertPosition(DESKTOP_WALLPAPER_POSITION value) noexcept {
    switch (value) {
    case DWPOS_CENTER: return WallpaperPosition::Center;
    case DWPOS_TILE: return WallpaperPosition::Tile;
    case DWPOS_STRETCH: return WallpaperPosition::Stretch;
    case DWPOS_FIT: return WallpaperPosition::Fit;
    case DWPOS_SPAN: return WallpaperPosition::Span;
    default: return WallpaperPosition::Fill;
    }
}

std::uint32_t SampleBilinear(
    const std::vector<std::uint8_t>& pixels, UINT width, UINT height, float x, float y) noexcept {
    x = std::clamp(x, 0.0f, static_cast<float>(width - 1));
    y = std::clamp(y, 0.0f, static_cast<float>(height - 1));
    const UINT x0 = static_cast<UINT>(std::floor(x));
    const UINT y0 = static_cast<UINT>(std::floor(y));
    const UINT x1 = std::min(width - 1, x0 + 1);
    const UINT y1 = std::min(height - 1, y0 + 1);
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    const auto channelAt = [&](UINT px, UINT py, int channel) {
        return static_cast<float>(pixels[(static_cast<std::size_t>(py) * width + px) * 4 + channel]);
    };
    std::uint32_t packed{};
    for (int channel = 0; channel < 4; ++channel) {
        const float top = channelAt(x0, y0, channel) * (1.0f - fx) + channelAt(x1, y0, channel) * fx;
        const float bottom = channelAt(x0, y1, channel) * (1.0f - fx) + channelAt(x1, y1, channel) * fx;
        const auto value = static_cast<std::uint32_t>(std::lround(top * (1.0f - fy) + bottom * fy));
        packed |= std::min(255u, value) << (channel * 8);
    }
    return packed;
}

} // namespace

HRESULT WallpaperCache::Initialize() {
    if (factory_) return S_OK;
    HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory_.GetAddressOf()));
    if (FAILED(result)) return result;
    static_cast<void>(CoCreateInstance(CLSID_DesktopWallpaper, nullptr,
        CLSCTX_ALL, IID_PPV_ARGS(desktopWallpaper_.GetAddressOf())));
    return Reload();
}

HRESULT WallpaperCache::Reload() {
    if (!factory_) return Initialize();
    sources_.clear();
    monitors_.clear();
    ++revision_;
    position_ = WallpaperPosition::Fill;
    backgroundBgra_ = 0xFF000000u;
    if (desktopWallpaper_) {
        DESKTOP_WALLPAPER_POSITION position{};
        if (SUCCEEDED(desktopWallpaper_->GetPosition(&position))) position_ = ConvertPosition(position);
        COLORREF color{};
        if (SUCCEEDED(desktopWallpaper_->GetBackgroundColor(&color))) {
            backgroundBgra_ = 0xFF000000u |
                (static_cast<std::uint32_t>(GetRValue(color)) << 16) |
                (static_cast<std::uint32_t>(GetGValue(color)) << 8) |
                static_cast<std::uint32_t>(GetBValue(color));
        }
    }
    return S_OK;
}

HRESULT WallpaperCache::ResolveWallpaperPath(
    const WallpaperMonitorGeometry& geometry, std::wstring& path) const {
    if (desktopWallpaper_) {
        UINT count{};
        if (SUCCEEDED(desktopWallpaper_->GetMonitorDevicePathCount(&count))) {
            for (UINT index = 0; index < count; ++index) {
                PWSTR monitorPath{};
                if (FAILED(desktopWallpaper_->GetMonitorDevicePathAt(index, &monitorPath))) continue;
                RECT bounds{};
                const bool match = SUCCEEDED(desktopWallpaper_->GetMonitorRECT(monitorPath, &bounds)) &&
                    bounds.left == geometry.monitorX && bounds.top == geometry.monitorY &&
                    bounds.right - bounds.left == geometry.monitorWidth &&
                    bounds.bottom - bounds.top == geometry.monitorHeight;
                if (match) {
                    PWSTR wallpaperPath{};
                    const HRESULT result = desktopWallpaper_->GetWallpaper(monitorPath, &wallpaperPath);
                    CoTaskMemFree(monitorPath);
                    if (SUCCEEDED(result) && wallpaperPath) path = wallpaperPath;
                    CoTaskMemFree(wallpaperPath);
                    return SUCCEEDED(result) && path.empty() ? S_FALSE : result;
                }
                CoTaskMemFree(monitorPath);
            }
        }
    }
    std::array<wchar_t, 32768> fallback{};
    if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER,
            static_cast<UINT>(fallback.size()), fallback.data(), 0)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    path = fallback.data();
    return path.empty() ? S_FALSE : S_OK;
}

HRESULT WallpaperCache::Decode(const std::wstring& path, IWICBitmapSource** source) const {
    if (!source) return E_POINTER;
    *source = nullptr;
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT result = factory_->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
    if (FAILED(result)) return result;
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(result = decoder->GetFrame(0, frame.GetAddressOf()))) return result;
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(result = factory_->CreateFormatConverter(converter.GetAddressOf()))) return result;
    if (FAILED(result = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeMedianCut))) return result;
    return converter.CopyTo(source);
}

HRESULT WallpaperCache::ComposeMonitor(
    IWICBitmapSource* source, const WallpaperMonitorGeometry& geometry, IWICBitmap** bitmap) const {
    if (!source || !bitmap || geometry.monitorWidth <= 0 || geometry.monitorHeight <= 0) return E_INVALIDARG;
    *bitmap = nullptr;
    UINT sourceWidth{};
    UINT sourceHeight{};
    HRESULT result = source->GetSize(&sourceWidth, &sourceHeight);
    if (FAILED(result) || sourceWidth == 0 || sourceHeight == 0) return FAILED(result) ? result : E_FAIL;
    const UINT sourceStride = sourceWidth * 4;
    std::vector<std::uint8_t> sourcePixels(static_cast<std::size_t>(sourceStride) * sourceHeight);
    if (FAILED(result = source->CopyPixels(nullptr, sourceStride,
            static_cast<UINT>(sourcePixels.size()), sourcePixels.data()))) return result;

    Microsoft::WRL::ComPtr<IWICBitmap> destination;
    if (FAILED(result = factory_->CreateBitmap(static_cast<UINT>(geometry.monitorWidth),
            static_cast<UINT>(geometry.monitorHeight), GUID_WICPixelFormat32bppPBGRA,
            WICBitmapCacheOnLoad, destination.GetAddressOf()))) return result;
    WICRect lockRect{0, 0, geometry.monitorWidth, geometry.monitorHeight};
    Microsoft::WRL::ComPtr<IWICBitmapLock> lock;
    if (FAILED(result = destination->Lock(&lockRect, WICBitmapLockWrite, lock.GetAddressOf()))) return result;
    UINT stride{};
    UINT bufferSize{};
    BYTE* output{};
    if (FAILED(result = lock->GetStride(&stride)) ||
        FAILED(result = lock->GetDataPointer(&bufferSize, &output))) return result;

    const WallpaperTransform transform = WallpaperPlacement::Calculate(
        {static_cast<float>(sourceWidth), static_cast<float>(sourceHeight)}, geometry, position_);
    for (int y = 0; y < geometry.monitorHeight; ++y) {
        auto* row = reinterpret_cast<std::uint32_t*>(output + static_cast<std::size_t>(y) * stride);
        for (int x = 0; x < geometry.monitorWidth; ++x) {
            float sourceX{};
            float sourceY{};
            bool inside = true;
            if (transform.tiled) {
                const int globalX = geometry.monitorX - geometry.virtualX + x;
                const int globalY = geometry.monitorY - geometry.virtualY + y;
                sourceX = static_cast<float>((globalX % static_cast<int>(sourceWidth) +
                    static_cast<int>(sourceWidth)) % static_cast<int>(sourceWidth));
                sourceY = static_cast<float>((globalY % static_cast<int>(sourceHeight) +
                    static_cast<int>(sourceHeight)) % static_cast<int>(sourceHeight));
            } else {
                sourceX = (static_cast<float>(x) + 0.5f - transform.destinationX) /
                    transform.scaleX - 0.5f;
                sourceY = (static_cast<float>(y) + 0.5f - transform.destinationY) /
                    transform.scaleY - 0.5f;
                inside = sourceX >= -0.5f && sourceY >= -0.5f &&
                    sourceX < static_cast<float>(sourceWidth) - 0.5f &&
                    sourceY < static_cast<float>(sourceHeight) - 0.5f;
            }
            row[x] = inside ? SampleBilinear(sourcePixels, sourceWidth, sourceHeight, sourceX, sourceY)
                            : backgroundBgra_;
        }
    }
    return destination.CopyTo(bitmap);
}

HRESULT WallpaperCache::MonitorBitmap(
    const std::wstring& monitorId, const WallpaperMonitorGeometry& geometry,
    IWICBitmapSource** bitmap) {
    if (!bitmap) return E_POINTER;
    *bitmap = nullptr;
    if (!factory_) {
        const HRESULT result = Initialize();
        if (FAILED(result)) return result;
    }
    const auto cached = monitors_.find(monitorId);
    if (cached != monitors_.end() && SameGeometry(cached->second.geometry, geometry) &&
        cached->second.bitmap) return cached->second.bitmap.CopyTo(bitmap);

    std::wstring path;
    HRESULT result = ResolveWallpaperPath(geometry, path);
    if (result != S_OK) return result;
    Microsoft::WRL::ComPtr<IWICBitmapSource> source;
    const auto decoded = sources_.find(path);
    if (decoded != sources_.end()) {
        source = decoded->second;
    } else {
        if (FAILED(result = Decode(path, source.GetAddressOf()))) return result;
        sources_.emplace(path, source);
    }
    MonitorEntry entry{.geometry = geometry};
    if (FAILED(result = ComposeMonitor(source.Get(), geometry, entry.bitmap.GetAddressOf()))) return result;
    auto [iterator, inserted] = monitors_.insert_or_assign(monitorId, std::move(entry));
    static_cast<void>(inserted);
    return iterator->second.bitmap.CopyTo(bitmap);
}

} // namespace ws
