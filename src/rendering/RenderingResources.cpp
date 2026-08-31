#include "rendering/RenderingResources.h"

#include <objbase.h>

namespace ws {

HRESULT RenderingResources::Initialize() {
    if (d2dFactory_ && dwriteFactory_ && wicFactory_ && labelFormat_ && smallFormat_) return S_OK;

    d2dFactory_.Reset();
    dwriteFactory_.Reset();
    wicFactory_.Reset();
    labelFormat_.Reset();
    smallFormat_.Reset();

    HRESULT result = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf());
    if (FAILED(result)) return result;
    result = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf()));
    if (FAILED(result)) return result;
    result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(wicFactory_.GetAddressOf()));
    if (FAILED(result)) return result;

    result = dwriteFactory_->CreateTextFormat(L"Segoe UI Variable", nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"en-us", labelFormat_.GetAddressOf());
    if (FAILED(result)) {
        result = dwriteFactory_->CreateTextFormat(L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"en-us", labelFormat_.GetAddressOf());
    }
    if (FAILED(result)) return result;
    result = dwriteFactory_->CreateTextFormat(L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"en-us", smallFormat_.GetAddressOf());
    if (FAILED(result)) return result;
    result = labelFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    if (FAILED(result)) return result;
    return smallFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
}

} // namespace ws
