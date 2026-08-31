#pragma once

#include <cstdint>
#include <wincodec.h>
#include <wrl/client.h>

namespace ws {

class WallpaperCache {
public:
    HRESULT Initialize();
    HRESULT Reload();

    [[nodiscard]] IWICBitmapSource* Bitmap() const noexcept { return bitmap_.Get(); }
    [[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }

private:
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory_;
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap_;
    std::uint64_t revision_{};
};

} // namespace ws
