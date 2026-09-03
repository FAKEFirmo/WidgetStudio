#pragma once

#include "layout/GridLayout.h"
#include "persistence/AssetLibrary.h"
#include "rendering/Renderer.h"
#include "scene/WidgetScene.h"
#include "windows/MonitorTopology.h"

#include <filesystem>
#include <array>
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
    void CancelInteraction();
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
    void UpdateLayoutSettingValues();
    void RebuildWidgetSettings(const WidgetInstance* widget);
    void UpdateWidgetSettingValues();
    void UpdateSettingsPageVisibility();
    void UpdateGeneralAppearanceControls();
    void ApplyGeneralAppearance();
    void ApplyGeneralAppearanceOptOut();
    void ApplyUniversalSettings();
    void ApplyRequestedLayoutMode();
    void ApplyAlignment();
    void ApplyWidgetSetting(std::size_t index);
    void ChooseWidgetFile(std::size_t index);
    void NotifySceneChanged();
    void EndPreviewDrag();
    bool HandleEditKey(WPARAM key);
    void ResetControlHandles() noexcept;

    [[nodiscard]] WidgetInstance* PrimaryWidget() noexcept;

    HINSTANCE instance_{};
    HWND hwnd_{};
    HWND preview_{};
    std::array<HWND, 5> settingsPageButtons_{};
    HWND generalAppearanceMode_{};
    HWND generalSurface_{};
    HWND generalFont_{};
    HWND generalTint_{};
    HWND generalOpacity_{};
    HWND generalBlur_{};
    HWND generalRadius_{};
    HWND generalPadding_{};
    HWND generalBorder_{};
    HWND generalShadow_{};
    HWND monitorChoice_{};
    HWND layoutMode_{};
    HWND locked_{};
    HWND contentScale_{};
    HWND useGeneralAppearance_{};
    HWND fontFamily_{};
    HWND tint_{};
    HWND appearanceMode_{};
    HWND glass_{};
    HWND padding_{};
    HWND border_{};
    HWND shadow_{};
    HWND showGrid_{};
    HWND opacity_{};
    HWND blur_{};
    HWND radius_{};
    HWND positionA_{};
    HWND positionB_{};
    HWND sizeA_{};
    HWND sizeB_{};
    HWND duplicate_{};
    HWND alignment_{};
    HWND widgetEmpty_{};
    HWND generalSection_{};
    HWND layoutSection_{};
    HWND appearanceSection_{};
    HWND widgetSection_{};
    HWND actionsSection_{};
    HWND applyGeneral_{};
    HWND applyUniversal_{};
    HWND openLibraryButton_{};
    HWND delete_{};
    HWND applyAlignment_{};
    struct FieldControls {
        HWND label{};
        HWND control{};
    };
    struct WidgetSettingControls {
        WidgetSettingDefinition definition;
        HWND label{};
        HWND editor{};
        HWND browse{};
    };
    std::vector<FieldControls> layoutFields_;
    std::vector<FieldControls> generalFields_;
    std::vector<FieldControls> appearanceFields_;
    std::vector<FieldControls> widgetFields_;
    std::vector<WidgetSettingControls> widgetSettingControls_;
    std::string widgetSettingsTypeId_;
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
    int activeSettingsPage_{};
    bool updatingControls_{false};
    HBRUSH backgroundBrush_{};
    HBRUSH fieldBrush_{};
    struct PreviewDragState {
        std::string widgetId;
        PointF offset{};
        bool moved{false};
    };
    std::optional<PreviewDragState> previewDrag_;
};

} // namespace ws
