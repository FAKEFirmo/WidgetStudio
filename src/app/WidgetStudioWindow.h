#pragma once

#include "layout/GridLayout.h"
#include "persistence/AssetLibrary.h"
#include "rendering/Renderer.h"
#include "scene/WidgetScene.h"
#include "windows/MonitorTopology.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>

namespace ws {

class WallpaperCache;
class RenderingResources;

class WidgetStudioWindow {
public:
    WidgetStudioWindow() = default;
    ~WidgetStudioWindow();

    bool Open(HWND owner, HINSTANCE instance, WidgetScene& scene, GridLayout& grid,
        GridMetrics layoutMetrics, RectF layoutBounds, std::filesystem::path assetDirectory,
        std::wstring monitorId, std::vector<MonitorDescriptor> monitors,
        std::shared_ptr<WallpaperCache> wallpaperCache,
        std::shared_ptr<RenderingResources> renderingResources,
        std::function<void()> sceneChanged,
        std::function<void()> selectionChanged, std::function<void()> openLibrary);
    void Close() noexcept;
    void Refresh();
    void InvalidatePreview(bool reloadWallpaper = false);
    void UpdateLayoutContext(GridMetrics layoutMetrics, RectF layoutBounds, std::wstring monitorId);
    void UpdateMonitors(std::vector<MonitorDescriptor> monitors);
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
    void EndPreviewDrag();
    bool HandleEditKey(WPARAM key);
    void ResetControlHandles() noexcept;

    [[nodiscard]] WidgetInstance* PrimaryWidget() noexcept;

    HINSTANCE instance_{};
    HWND hwnd_{};
    HWND preview_{};
    HWND monitorChoice_{};
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
    HWND duplicate_{};
    HWND alignment_{};
    HWND widgetSetting_{};
    HWND widgetValue_{};
    HWND widgetChoice_{};
    HWND widgetCheck_{};
    HWND browse_{};
    HWND applyWidget_{};
    WidgetScene* scene_{};
    GridLayout* grid_{};
    GridMetrics layoutMetrics_{};
    GridMetrics previewMetrics_{};
    float previewScale_{1.0f};
    PointF previewOffset_{};
    RectF layoutBounds_{};
    std::wstring monitorId_;
    std::vector<MonitorDescriptor> monitors_;
    std::unique_ptr<Renderer> previewRenderer_;
    std::unique_ptr<AssetLibrary> assetLibrary_;
    std::function<void()> sceneChanged_;
    std::function<void()> selectionChanged_;
    std::function<void()> openLibrary_;
    struct PreviewDragState {
        std::string widgetId;
        PointF offset{};
        bool moved{false};
    };
    std::optional<PreviewDragState> previewDrag_;
};

} // namespace ws
