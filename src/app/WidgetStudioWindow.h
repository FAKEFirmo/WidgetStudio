#pragma once

#include "layout/GridLayout.h"
#include "persistence/AssetLibrary.h"
#include "rendering/Renderer.h"
#include "scene/WidgetScene.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <windows.h>

namespace ws {

class WidgetStudioWindow {
public:
    WidgetStudioWindow() = default;
    ~WidgetStudioWindow();

    bool Open(HWND owner, HINSTANCE instance, WidgetScene& scene, GridLayout& grid,
        GridMetrics layoutMetrics, RectF layoutBounds, std::filesystem::path assetDirectory,
        std::wstring monitorId, std::function<void()> sceneChanged,
        std::function<void()> selectionChanged, std::function<void()> openLibrary);
    void Close() noexcept;
    void Refresh();
    void InvalidatePreview(bool reloadWallpaper = false);
    void UpdateLayoutContext(GridMetrics layoutMetrics, RectF layoutBounds, std::wstring monitorId);
    [[nodiscard]] HWND Window() const noexcept { return hwnd_; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK PreviewProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandlePreviewMessage(UINT message, WPARAM wParam, LPARAM lParam);

    bool CreateControls();
    void LayoutControls(int width, int height);
    void UpdatePreviewMetrics();
    void PaintPreview();
    void UpdateControlsFromSelection();
    void UpdateWidgetSettingValue();
    void ApplyUniversalSettings();
    void ApplyAlignment();
    void ApplyWidgetSetting();
    void ChooseWidgetFile();
    void NotifySceneChanged();

    [[nodiscard]] WidgetInstance* PrimaryWidget() noexcept;

    HINSTANCE instance_{};
    HWND hwnd_{};
    HWND preview_{};
    HWND layoutMode_{};
    HWND locked_{};
    HWND contentScale_{};
    HWND appearanceMode_{};
    HWND glass_{};
    HWND opacity_{};
    HWND blur_{};
    HWND radius_{};
    HWND positionA_{};
    HWND positionB_{};
    HWND sizeA_{};
    HWND sizeB_{};
    HWND alignment_{};
    HWND widgetSetting_{};
    HWND widgetValue_{};
    HWND widgetChoice_{};
    HWND widgetCheck_{};
    HWND browse_{};
    WidgetScene* scene_{};
    GridLayout* grid_{};
    GridMetrics layoutMetrics_{};
    GridMetrics previewMetrics_{};
    float previewScale_{1.0f};
    PointF previewOffset_{};
    RectF layoutBounds_{};
    std::wstring monitorId_;
    std::unique_ptr<Renderer> previewRenderer_;
    std::unique_ptr<AssetLibrary> assetLibrary_;
    std::function<void()> sceneChanged_;
    std::function<void()> selectionChanged_;
    std::function<void()> openLibrary_;
};

} // namespace ws
