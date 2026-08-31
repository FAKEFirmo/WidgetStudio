#include "rendering/WallpaperCache.h"

#include <array>
#include <windows.h>

namespace ws {

HRESULT WallpaperCache::Initialize() {
    if (factory_) return S_OK;
    const HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory_.GetAddressOf()));
    if (FAILED(result)) return result;
    return Reload();
}

HRESULT WallpaperCache::Reload() {
    if (!factory_) {
        const HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
            CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory_.GetAddressOf()));
        if (FAILED(result)) return result;
    }

    bitmap_.Reset();
    ++revision_;
    std::array<wchar_t, 32768> path{};
    if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER,
            static_cast<UINT>(path.size()), path.data(), 0)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    if (path[0] == L'\0') return S_FALSE;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT result = factory_->CreateDecoderFromFilename(path.data(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
    if (FAILED(result)) return result;
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    result = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(result)) return result;
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    result = factory_->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(result)) return result;
    result = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeMedianCut);
    if (FAILED(result)) return result;
    return factory_->CreateBitmapFromSource(
        converter.Get(), WICBitmapCacheOnLoad, bitmap_.GetAddressOf());
}

} // namespace ws
