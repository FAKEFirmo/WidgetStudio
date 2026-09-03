#include "app/WidgetStudioWindow.h"
#include "Version.h"

#include "layout/OuterLayout.h"
#include "desktop/WidgetWindowPlacement.h"
#include "rendering/RenderingResources.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <shobjidl.h>
#include <string>
#include <string_view>
#include <uxtheme.h>
#include <utility>
#include <vector>
#include <windowsx.h>
#include <wrl/client.h>

namespace ws {
namespace {

constexpr wchar_t kStudioClass[] = L"WidgetStudioManagementWindow";
constexpr wchar_t kPreviewClass[] = L"WidgetStudioPreviewWindow";
constexpr int kApplyUniversal = 200;
constexpr int kApplyAlignment = 201;
constexpr int kOpenLibrary = 205;
constexpr int kDuplicateWidget = 206;
constexpr int kDeleteWidget = 207;
constexpr int kMonitorChoice = 208;
constexpr int kLayoutMode = 209;
constexpr int kLocked = 210;
constexpr int kContentScale = 211;
constexpr int kAppearanceMode = 212;
constexpr int kGlass = 213;
constexpr int kOpacity = 214;
constexpr int kBlur = 215;
constexpr int kRadius = 216;
constexpr int kPositionA = 217;
constexpr int kPositionB = 218;
constexpr int kSizeA = 219;
constexpr int kSizeB = 220;
constexpr int kAlignment = 221;
constexpr int kPadding = 225;
constexpr int kBorder = 226;
constexpr int kShadow = 227;
constexpr int kShowGrid = 228;
constexpr int kSettingsPageBase = 229;
constexpr int kWidgetSettingBase = 400;
constexpr int kWidgetBrowseBase = 500;

HWND AddControl(HWND parent, HINSTANCE instance, const wchar_t* type, const wchar_t* text,
    DWORD style, int id = 0, DWORD extendedStyle = 0) {
    if (wcscmp(type, L"BUTTON") == 0 && (style & BS_TYPEMASK) == BS_PUSHBUTTON) {
        style = (style & ~BS_TYPEMASK) | BS_OWNERDRAW;
    } else if (wcscmp(type, L"COMBOBOX") == 0 && (style & CBS_DROPDOWNLIST) != 0) {
        style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
    }
    HWND control = CreateWindowExW(extendedStyle, type, text, WS_CHILD | WS_VISIBLE | style,
        0, 0, 100, 24, parent, id ? reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)) : nullptr,
        instance, nullptr);
    if (control) SendMessageW(control, WM_SETFONT,
        reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    if (control && wcscmp(type, L"BUTTON") == 0) {
        const DWORD buttonType = style & BS_TYPEMASK;
        if (buttonType == BS_AUTOCHECKBOX || buttonType == BS_GROUPBOX) {
            // The themed Windows 10 checkbox/group-box renderer assumes a
            // light parent and ignores our dark control colors.
            SetWindowTheme(control, L"", L"");
        }
    }
    return control;
}

void SetNumber(HWND control, double value) {
    wchar_t text[48]{};
    _snwprintf_s(text, _countof(text), _TRUNCATE, L"%.10g", value);
    SetWindowTextW(control, text);
}

double ParseNumber(std::wstring_view text, double fallback) {
    if (text.empty()) return fallback;
    std::wstring terminated(text);
    wchar_t* end = nullptr;
    const double value = std::wcstod(terminated.c_str(), &end);
    return end != terminated.c_str() && *end == L'\0' && std::isfinite(value) ? value : fallback;
}

double ReadNumber(HWND control, double fallback) {
    wchar_t text[64]{};
    GetWindowTextW(control, text, static_cast<int>(std::size(text)));
    return ParseNumber(text, fallback);
}

std::wstring ReadText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) return {};
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

void EnableNativeDarkFrame(HWND window) {
    using SetWindowAttribute = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    const HMODULE module = LoadLibraryW(L"dwmapi.dll");
    if (!module) return;
    const auto setAttribute = reinterpret_cast<SetWindowAttribute>(
        GetProcAddress(module, "DwmSetWindowAttribute"));
    if (setAttribute) {
        const BOOL enabled = TRUE;
        // Attribute 20 is the supported immersive dark-frame flag on current
        // Windows 10/11 builds. Unsupported builds simply reject it.
        static_cast<void>(setAttribute(window, 20, &enabled, sizeof(enabled)));
    }
    FreeLibrary(module);
}

void DrawSettingsNavigationButton(const DRAWITEMSTRUCT& item, bool active) {
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const COLORREF background = active
        ? (pressed ? RGB(69, 91, 158) : RGB(55, 72, 120))
        : (pressed ? RGB(38, 44, 54) : RGB(24, 28, 34));
    HBRUSH brush = CreateSolidBrush(background);
    FillRect(item.hDC, &item.rcItem, brush);
    DeleteObject(brush);

    HPEN pen = CreatePen(PS_SOLID, active ? 2 : 1,
        active ? RGB(142, 167, 255) : RGB(72, 79, 91));
    HGDIOBJ previousPen = SelectObject(item.hDC, pen);
    HGDIOBJ previousBrush = SelectObject(item.hDC, GetStockObject(NULL_BRUSH));
    Rectangle(item.hDC, item.rcItem.left, item.rcItem.top,
        item.rcItem.right, item.rcItem.bottom);
    SelectObject(item.hDC, previousBrush);
    SelectObject(item.hDC, previousPen);
    DeleteObject(pen);

    wchar_t text[64]{};
    GetWindowTextW(item.hwndItem, text, static_cast<int>(std::size(text)));
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, (item.itemState & ODS_DISABLED) != 0
        ? RGB(128, 135, 145) : RGB(248, 249, 252));
    RECT textRect = item.rcItem;
    DrawTextW(item.hDC, text, -1, &textRect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if ((item.itemState & ODS_FOCUS) != 0) {
        InflateRect(&textRect, -4, -4);
        DrawFocusRect(item.hDC, &textRect);
    }
}

void DrawDarkComboItem(const DRAWITEMSTRUCT& item) {
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool selectedListItem = (item.itemState & ODS_SELECTED) != 0 &&
        (item.itemState & ODS_COMBOBOXEDIT) == 0;
    HBRUSH brush = CreateSolidBrush(selectedListItem ? RGB(55, 72, 120) : RGB(22, 25, 30));
    FillRect(item.hDC, &item.rcItem, brush);
    DeleteObject(brush);

    wchar_t text[192]{};
    if (item.itemID != static_cast<UINT>(-1)) {
        SendMessageW(item.hwndItem, CB_GETLBTEXT, item.itemID, reinterpret_cast<LPARAM>(text));
    }
    RECT textRect = item.rcItem;
    textRect.left += 8;
    textRect.right -= 4;
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, disabled ? RGB(130, 137, 147) : RGB(248, 249, 252));
    DrawTextW(item.hDC, text, -1, &textRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if ((item.itemState & ODS_FOCUS) != 0) DrawFocusRect(item.hDC, &item.rcItem);
}

} // namespace

WidgetStudioWindow::~WidgetStudioWindow() { Close(); }

bool WidgetStudioWindow::Open(HWND owner, HINSTANCE instance, WidgetScene& scene, GridLayout& grid,
    GridMetrics layoutMetrics, RectF layoutBounds, std::filesystem::path assetDirectory,
    std::wstring monitorId, std::vector<MonitorDescriptor> monitors,
    std::shared_ptr<WallpaperCache> wallpaperCache,
    std::shared_ptr<RenderingResources> renderingResources,
    std::function<void()> sceneChanged,
    std::function<void()> selectionChanged, std::function<void()> openLibrary) {
    if (hwnd_) {
        UpdateLayoutContext(layoutMetrics, layoutBounds, std::move(monitorId));
        UpdateMonitors(std::move(monitors));
        ShowWindow(hwnd_, SW_SHOWNORMAL);
        SetForegroundWindow(hwnd_);
        Refresh();
        return true;
    }
    instance_ = instance;
    scene_ = &scene;
    grid_ = &grid;
    layoutMetrics_ = layoutMetrics;
    layoutBounds_ = layoutBounds;
    monitorId_ = std::move(monitorId);
    monitors_ = std::move(monitors);
    assetLibrary_ = std::make_unique<AssetLibrary>(std::move(assetDirectory));
    sceneChanged_ = std::move(sceneChanged);
    selectionChanged_ = std::move(selectionChanged);
    openLibrary_ = std::move(openLibrary);
    backgroundBrush_ = CreateSolidBrush(RGB(13, 15, 18));
    fieldBrush_ = CreateSolidBrush(RGB(22, 25, 30));

    WNDCLASSEXW previewClass{};
    previewClass.cbSize = sizeof(previewClass);
    previewClass.lpfnWndProc = PreviewProc;
    previewClass.hInstance = instance_;
    previewClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    previewClass.lpszClassName = kPreviewClass;
    if (!RegisterClassExW(&previewClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Close();
        return false;
    }
    WNDCLASSEXW studioClass{};
    studioClass.cbSize = sizeof(studioClass);
    studioClass.lpfnWndProc = WindowProc;
    studioClass.hInstance = instance_;
    studioClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    studioClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    studioClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    studioClass.lpszClassName = kStudioClass;
    if (!RegisterClassExW(&studioClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Close();
        return false;
    }

    const float initialScale = static_cast<float>(std::max(96u, GetDpiForSystem())) / 96.0f;
    const std::wstring title = std::wstring(L"Widget Studio ") + kDisplayVersion;
    hwnd_ = CreateWindowExW(WS_EX_APPWINDOW, kStudioClass, title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
        static_cast<int>(1120.0f * initialScale), static_cast<int>(820.0f * initialScale),
        owner, nullptr, instance_, this);
    if (!hwnd_ || !preview_) { Close(); return false; }
    EnableNativeDarkFrame(hwnd_);
    previewRenderer_ = std::make_unique<Renderer>(
        std::move(wallpaperCache), std::move(renderingResources));
    if (FAILED(previewRenderer_->Initialize(preview_))) { Close(); return false; }
    UpdatePreviewMetrics();
    UpdateControlsFromSelection();
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    UpdateWindow(hwnd_);
    return true;
}

void WidgetStudioWindow::Close() noexcept {
    CancelInteraction();
    if (hwnd_ && IsWindow(hwnd_)) DestroyWindow(hwnd_);
    previewRenderer_.reset();
    assetLibrary_.reset();
    if (backgroundBrush_) DeleteObject(backgroundBrush_);
    if (fieldBrush_) DeleteObject(fieldBrush_);
    backgroundBrush_ = nullptr;
    fieldBrush_ = nullptr;
    hwnd_ = nullptr;
    ResetControlHandles();
}

void WidgetStudioWindow::CancelInteraction() {
    EndPreviewDrag();
}

void WidgetStudioWindow::ResetControlHandles() noexcept {
    preview_ = nullptr;
    settingsPageButtons_.fill(nullptr);
    monitorChoice_ = nullptr;
    layoutMode_ = nullptr;
    locked_ = nullptr;
    contentScale_ = nullptr;
    appearanceMode_ = nullptr;
    glass_ = nullptr;
    padding_ = nullptr;
    border_ = nullptr;
    shadow_ = nullptr;
    showGrid_ = nullptr;
    opacity_ = nullptr;
    blur_ = nullptr;
    radius_ = nullptr;
    positionA_ = nullptr;
    positionB_ = nullptr;
    sizeA_ = nullptr;
    sizeB_ = nullptr;
    duplicate_ = nullptr;
    alignment_ = nullptr;
    widgetEmpty_ = nullptr;
    layoutSection_ = nullptr;
    appearanceSection_ = nullptr;
    widgetSection_ = nullptr;
    actionsSection_ = nullptr;
    applyUniversal_ = nullptr;
    openLibraryButton_ = nullptr;
    delete_ = nullptr;
    applyAlignment_ = nullptr;
    layoutFields_.clear();
    appearanceFields_.clear();
    widgetFields_.clear();
    widgetSettingControls_.clear();
    widgetSettingsTypeId_.clear();
    activeSettingsPage_ = 0;
    updatingControls_ = false;
    previewDrag_.reset();
}

void WidgetStudioWindow::Refresh() {
    if (!hwnd_) return;
    UpdateControlsFromSelection();
    InvalidateRect(preview_, nullptr, FALSE);
}

void WidgetStudioWindow::InvalidatePreview(bool reloadWallpaper) {
    if (!preview_) return;
    if (reloadWallpaper && previewRenderer_) static_cast<void>(previewRenderer_->ReloadWallpaper());
    InvalidateRect(preview_, nullptr, FALSE);
}

void WidgetStudioWindow::UpdateLayoutContext(
    GridMetrics layoutMetrics, RectF layoutBounds, std::wstring monitorId) {
    layoutMetrics_ = layoutMetrics;
    layoutBounds_ = layoutBounds;
    monitorId_ = std::move(monitorId);
    if (hwnd_) {
        RECT client{};
        GetClientRect(hwnd_, &client);
        LayoutControls(client.right - client.left, client.bottom - client.top);
    }
    UpdatePreviewMetrics();
    if (preview_) InvalidateRect(preview_, nullptr, FALSE);
}

void WidgetStudioWindow::UpdateMonitors(std::vector<MonitorDescriptor> monitors) {
    monitors_ = std::move(monitors);
    if (!monitorChoice_) return;
    SendMessageW(monitorChoice_, CB_RESETCONTENT, 0, 0);
    for (const MonitorDescriptor& monitor : monitors_) {
        const std::wstring label = monitor.id + (monitor.primary ? L" (Primary)" : L"");
        SendMessageW(monitorChoice_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
    UpdateControlsFromSelection();
}

LRESULT CALLBACK WidgetStudioWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    WidgetStudioWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        self = static_cast<WidgetStudioWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else self = reinterpret_cast<WidgetStudioWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->HandleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK WidgetStudioWindow::PreviewProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    WidgetStudioWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        self = static_cast<WidgetStudioWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        self->preview_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else self = reinterpret_cast<WidgetStudioWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->HandlePreviewMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

bool WidgetStudioWindow::CreateControls() {
    layoutFields_.clear();
    appearanceFields_.clear();
    widgetFields_.clear();
    preview_ = CreateWindowExW(WS_EX_CLIENTEDGE, kPreviewClass, nullptr, WS_CHILD | WS_VISIBLE,
        0, 0, 100, 100, hwnd_, nullptr, instance_, this);
    const std::array<const wchar_t*, 4> pageLabels{
        L"Layout", L"Appearance", L"Widget content", L"Actions"};
    for (std::size_t index = 0; index < settingsPageButtons_.size(); ++index) {
        settingsPageButtons_[index] = AddControl(hwnd_, instance_, L"BUTTON", pageLabels[index],
            BS_OWNERDRAW | WS_TABSTOP, kSettingsPageBase + static_cast<int>(index));
    }
    layoutSection_ = AddControl(hwnd_, instance_, L"BUTTON", L"Layout && placement", BS_GROUPBOX);
    appearanceSection_ = AddControl(hwnd_, instance_, L"BUTTON", L"Appearance", BS_GROUPBOX);
    widgetSection_ = AddControl(hwnd_, instance_, L"BUTTON", L"Widget content", BS_GROUPBOX);
    actionsSection_ = AddControl(hwnd_, instance_, L"BUTTON", L"Actions && alignment", BS_GROUPBOX);
    HWND monitorLabel = AddControl(hwnd_, instance_, L"STATIC", L"Monitor", 0);
    monitorChoice_ = AddControl(hwnd_, instance_, L"COMBOBOX", nullptr,
        CBS_DROPDOWNLIST | WS_TABSTOP, kMonitorChoice);
    for (const MonitorDescriptor& monitor : monitors_) {
        const std::wstring label = monitor.id + (monitor.primary ? L" (Primary)" : L"");
        SendMessageW(monitorChoice_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
    HWND layoutLabel = AddControl(hwnd_, instance_, L"STATIC", L"Layout mode", 0);
    layoutMode_ = AddControl(hwnd_, instance_, L"COMBOBOX", nullptr,
        CBS_DROPDOWNLIST | WS_TABSTOP, kLayoutMode);
    SendMessageW(layoutMode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Grid"));
    SendMessageW(layoutMode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Free / Align"));
    layoutFields_.push_back({layoutLabel, layoutMode_});
    layoutFields_.push_back({monitorLabel, monitorChoice_});
    locked_ = AddControl(hwnd_, instance_, L"BUTTON", L"Locked",
        BS_AUTOCHECKBOX | WS_TABSTOP, kLocked);
    HWND scaleLabel = AddControl(hwnd_, instance_, L"STATIC", L"Content scale", 0);
    contentScale_ = AddControl(hwnd_, instance_, L"EDIT", nullptr,
        ES_AUTOHSCROLL | WS_TABSTOP, kContentScale, WS_EX_CLIENTEDGE);
    layoutFields_.push_back({scaleLabel, contentScale_});
    HWND lockLabel = AddControl(hwnd_, instance_, L"STATIC", L"Lock widget", 0);
    layoutFields_.push_back({lockLabel, locked_});
    HWND appearanceLabel = AddControl(hwnd_, instance_, L"STATIC", L"Theme", 0);
    appearanceMode_ = AddControl(hwnd_, instance_, L"COMBOBOX", nullptr,
        CBS_DROPDOWNLIST | WS_TABSTOP, kAppearanceMode);
    SendMessageW(appearanceMode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Dark"));
    SendMessageW(appearanceMode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Light"));
    appearanceFields_.push_back({appearanceLabel, appearanceMode_});
    glass_ = AddControl(hwnd_, instance_, L"COMBOBOX", nullptr,
        CBS_DROPDOWNLIST | WS_TABSTOP, kGlass);
    SendMessageW(glass_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Frosted glass"));
    SendMessageW(glass_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Transparent / text-only"));
    SendMessageW(glass_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Solid translucent"));
    HWND glassLabel = AddControl(hwnd_, instance_, L"STATIC", L"Surface mode", 0);
    appearanceFields_.push_back({glassLabel, glass_});
    HWND opacityLabel = AddControl(hwnd_, instance_, L"STATIC", L"Opacity (0-1)", 0);
    opacity_ = AddControl(hwnd_, instance_, L"EDIT", nullptr,
        ES_AUTOHSCROLL | WS_TABSTOP, kOpacity, WS_EX_CLIENTEDGE);
    appearanceFields_.push_back({opacityLabel, opacity_});
    HWND blurLabel = AddControl(hwnd_, instance_, L"STATIC", L"Blur radius (DIP)", 0);
    blur_ = AddControl(hwnd_, instance_, L"EDIT", nullptr,
        ES_AUTOHSCROLL | WS_TABSTOP, kBlur, WS_EX_CLIENTEDGE);
    appearanceFields_.push_back({blurLabel, blur_});
    HWND radiusLabel = AddControl(hwnd_, instance_, L"STATIC", L"Corner radius (DIP)", 0);
    radius_ = AddControl(hwnd_, instance_, L"EDIT", nullptr,
        ES_AUTOHSCROLL | WS_TABSTOP, kRadius, WS_EX_CLIENTEDGE);
    appearanceFields_.push_back({radiusLabel, radius_});
    HWND paddingLabel = AddControl(hwnd_, instance_, L"STATIC", L"Internal padding (DIP)", 0);
    padding_ = AddControl(hwnd_, instance_, L"EDIT", nullptr,
        ES_AUTOHSCROLL | WS_TABSTOP, kPadding, WS_EX_CLIENTEDGE);
    appearanceFields_.push_back({paddingLabel, padding_});
    HWND borderLabel = AddControl(hwnd_, instance_, L"STATIC", L"Border", 0);
    border_ = AddControl(hwnd_, instance_, L"BUTTON", L"1 DIP subtle border",
        BS_AUTOCHECKBOX | WS_TABSTOP, kBorder);
    appearanceFields_.push_back({borderLabel, border_});
    HWND shadowLabel = AddControl(hwnd_, instance_, L"STATIC", L"Shadow", 0);
    shadow_ = AddControl(hwnd_, instance_, L"BUTTON", L"Subtle shadow",
        BS_AUTOCHECKBOX | WS_TABSTOP, kShadow);
    appearanceFields_.push_back({shadowLabel, shadow_});
    HWND positionALabel = AddControl(hwnd_, instance_, L"STATIC", L"Grid column / Free X", 0);
    positionA_ = AddControl(hwnd_, instance_, L"EDIT", nullptr,
        ES_AUTOHSCROLL | WS_TABSTOP, kPositionA, WS_EX_CLIENTEDGE);
    HWND positionBLabel = AddControl(hwnd_, instance_, L"STATIC", L"Grid row / Free Y", 0);
    positionB_ = AddControl(hwnd_, instance_, L"EDIT", nullptr,
        ES_AUTOHSCROLL | WS_TABSTOP, kPositionB, WS_EX_CLIENTEDGE);
    HWND sizeALabel = AddControl(hwnd_, instance_, L"STATIC", L"Column span / Free width", 0);
    sizeA_ = AddControl(hwnd_, instance_, L"EDIT", nullptr,
        ES_AUTOHSCROLL | WS_TABSTOP, kSizeA, WS_EX_CLIENTEDGE);
    HWND sizeBLabel = AddControl(hwnd_, instance_, L"STATIC", L"Row span / Free height", 0);
    sizeB_ = AddControl(hwnd_, instance_, L"EDIT", nullptr,
        ES_AUTOHSCROLL | WS_TABSTOP, kSizeB, WS_EX_CLIENTEDGE);
    layoutFields_.insert(layoutFields_.begin() + 2, {
        {positionALabel, positionA_}, {positionBLabel, positionB_},
        {sizeALabel, sizeA_}, {sizeBLabel, sizeB_}});
    HWND showGridLabel = AddControl(hwnd_, instance_, L"STATIC", L"Preview grid", 0);
    showGrid_ = AddControl(hwnd_, instance_, L"BUTTON", L"Show while editing",
        BS_AUTOCHECKBOX | WS_TABSTOP, kShowGrid);
    SendMessageW(showGrid_, BM_SETCHECK, BST_CHECKED, 0);
    layoutFields_.push_back({showGridLabel, showGrid_});
    applyUniversal_ = AddControl(hwnd_, instance_, L"BUTTON", L"Apply all changes",
        BS_PUSHBUTTON | WS_TABSTOP, kApplyUniversal);
    openLibraryButton_ = AddControl(hwnd_, instance_, L"BUTTON", L"Add widget...",
        BS_PUSHBUTTON | WS_TABSTOP, kOpenLibrary);
    duplicate_ = AddControl(hwnd_, instance_, L"BUTTON", L"Duplicate", BS_PUSHBUTTON | WS_TABSTOP, kDuplicateWidget);
    delete_ = AddControl(hwnd_, instance_, L"BUTTON", L"Remove", BS_PUSHBUTTON | WS_TABSTOP, kDeleteWidget);
    alignment_ = AddControl(hwnd_, instance_, L"COMBOBOX", nullptr,
        CBS_DROPDOWNLIST | WS_TABSTOP, kAlignment);
    for (const wchar_t* item : {L"Align left", L"Horizontal center", L"Align right", L"Align top",
            L"Vertical center", L"Align bottom", L"Match width", L"Match height", L"Match both",
            L"Distribute horizontally", L"Distribute vertically"})
        SendMessageW(alignment_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
    SendMessageW(alignment_, CB_SETCURSEL, 0, 0);
    applyAlignment_ = AddControl(hwnd_, instance_, L"BUTTON", L"Apply alignment",
        BS_PUSHBUTTON | WS_TABSTOP, kApplyAlignment);
    widgetEmpty_ = AddControl(hwnd_, instance_, L"STATIC",
        L"Select a configurable widget to edit its content.", SS_LEFT);
    UpdateSettingsPageVisibility();
    return preview_ && std::ranges::all_of(settingsPageButtons_, [](HWND button) { return button != nullptr; }) &&
        monitorChoice_ && layoutMode_ && locked_ && contentScale_ &&
        appearanceMode_ && glass_ && opacity_ && blur_ && radius_ && padding_ && border_ && shadow_ && showGrid_ &&
        positionA_ && positionB_ && sizeA_ && sizeB_ && duplicate_ && alignment_ && widgetEmpty_ &&
        layoutSection_ && appearanceSection_ && widgetSection_ && actionsSection_ &&
        applyUniversal_ && openLibraryButton_ && delete_ && applyAlignment_;
}

void WidgetStudioWindow::UpdateSettingsPageVisibility() {
    const auto showFields = [](const std::vector<FieldControls>& fields, bool visible) {
        for (const FieldControls& field : fields) {
            ShowWindow(field.label, visible ? SW_SHOW : SW_HIDE);
            ShowWindow(field.control, visible ? SW_SHOW : SW_HIDE);
        }
    };
    const bool layoutVisible = activeSettingsPage_ == 0;
    const bool appearanceVisible = activeSettingsPage_ == 1;
    const bool widgetVisible = activeSettingsPage_ == 2;
    const bool actionsVisible = activeSettingsPage_ == 3;
    ShowWindow(layoutSection_, layoutVisible ? SW_SHOW : SW_HIDE);
    showFields(layoutFields_, layoutVisible);
    ShowWindow(applyUniversal_, layoutVisible ? SW_SHOW : SW_HIDE);
    ShowWindow(appearanceSection_, appearanceVisible ? SW_SHOW : SW_HIDE);
    showFields(appearanceFields_, appearanceVisible);
    ShowWindow(widgetSection_, widgetVisible ? SW_SHOW : SW_HIDE);
    showFields(widgetFields_, widgetVisible);
    for (const WidgetSettingControls& controls : widgetSettingControls_) {
        if (controls.browse) ShowWindow(controls.browse, widgetVisible ? SW_SHOW : SW_HIDE);
    }
    ShowWindow(widgetEmpty_, widgetVisible && widgetSettingControls_.empty() ? SW_SHOW : SW_HIDE);
    ShowWindow(actionsSection_, actionsVisible ? SW_SHOW : SW_HIDE);
    for (HWND control : {openLibraryButton_, duplicate_, delete_, alignment_, applyAlignment_}) {
        ShowWindow(control, actionsVisible ? SW_SHOW : SW_HIDE);
    }
    for (HWND button : settingsPageButtons_) InvalidateRect(button, nullptr, TRUE);
}

void WidgetStudioWindow::LayoutControls(int width, int height) {
    const float dpiScale = static_cast<float>(std::max(96u, GetDpiForWindow(hwnd_))) / 96.0f;
    const int margin = static_cast<int>(18.0f * dpiScale);
    const int gap = static_cast<int>(14.0f * dpiScale);
    const int rowHeight = static_cast<int>(38.0f * dpiScale);
    const int controlHeight = static_cast<int>(26.0f * dpiScale);
    const int contentWidth = std::max(1, width - margin * 2);
    const float monitorAspect = layoutBounds_.height > 0.0f
        ? layoutBounds_.width / layoutBounds_.height : 16.0f / 9.0f;
    const auto sectionHeight = [rowHeight, dpiScale](std::size_t fields, int extraRows) {
        return static_cast<int>(34.0f * dpiScale) +
            (static_cast<int>((fields + 1) / 2) + extraRows) * rowHeight +
            static_cast<int>(12.0f * dpiScale);
    };
    const int layoutHeight = sectionHeight(layoutFields_.size(), 0);
    const int appearanceHeight = sectionHeight(appearanceFields_.size(), 0);
    const int widgetHeight = sectionHeight(std::max<std::size_t>(1, widgetFields_.size()), 0);
    const int actionsHeight = static_cast<int>(112.0f * dpiScale);
    const int pageHeight = std::max({layoutHeight, appearanceHeight, widgetHeight, actionsHeight});
    const int navigationHeight = static_cast<int>(34.0f * dpiScale);
    const int naturalPreviewHeight = std::max(1, static_cast<int>(std::lround(
        static_cast<float>(contentWidth) / std::max(0.5f, monitorAspect))));
    const int availablePreviewHeight = std::max(1,
        height - margin * 2 - gap * 2 - navigationHeight - pageHeight);
    const int previewHeight = std::min(naturalPreviewHeight, availablePreviewHeight);
    const int previewWidth = std::min(contentWidth, std::max(1,
        static_cast<int>(std::lround(static_cast<float>(previewHeight) * monitorAspect))));

    struct PendingPlacement {
        HWND control{};
        int x{};
        int y{};
        int width{};
        int height{};
    };
    std::vector<PendingPlacement> placements;
    placements.reserve(48 + widgetSettingControls_.size() * 2);

    int y = margin;
    const auto place = [&placements](HWND control, int x, int top,
        int controlWidth, int controlHeightValue) {
        if (!control) return;
        placements.push_back(PendingPlacement{
            control,
            x,
            top,
            std::max(1, controlWidth),
            std::max(1, controlHeightValue),
        });
    };

    place(preview_, margin + (contentWidth - previewWidth) / 2, y, previewWidth, previewHeight);
    y += previewHeight + gap;
    const int navigationGap = static_cast<int>(6.0f * dpiScale);
    const int navigationButtonWidth =
        (contentWidth - navigationGap * static_cast<int>(settingsPageButtons_.size() - 1)) /
        static_cast<int>(settingsPageButtons_.size());
    for (std::size_t index = 0; index < settingsPageButtons_.size(); ++index) {
        place(settingsPageButtons_[index],
            margin + static_cast<int>(index) * (navigationButtonWidth + navigationGap), y,
            navigationButtonWidth, navigationHeight);
    }
    y += navigationHeight + gap;

    const auto placeSection = [&](HWND group, const std::vector<FieldControls>& fields,
        int top, int sectionSize, HWND action) {
        place(group, margin, top, contentWidth, sectionSize);
        const int innerX = margin + static_cast<int>(14.0f * dpiScale);
        const int innerWidth = contentWidth - static_cast<int>(28.0f * dpiScale);
        const int columnGap = static_cast<int>(22.0f * dpiScale);
        const int columnWidth = (innerWidth - columnGap) / 2;
        const int labelWidth = std::max(static_cast<int>(125.0f * dpiScale), columnWidth * 46 / 100);
        const int firstY = top + static_cast<int>(28.0f * dpiScale);
        for (std::size_t index = 0; index < fields.size(); ++index) {
            const int column = static_cast<int>(index % 2);
            const int row = static_cast<int>(index / 2);
            const int x = innerX + column * (columnWidth + columnGap);
            const int fieldY = firstY + row * rowHeight;
            place(fields[index].label, x, fieldY + static_cast<int>(5.0f * dpiScale),
                labelWidth, controlHeight);
            const int valueX = x + labelWidth;
            const int valueWidth = std::max(1, columnWidth - labelWidth);
            wchar_t className[24]{};
            GetClassNameW(fields[index].control, className, static_cast<int>(std::size(className)));
            const int dropHeight = wcscmp(className, L"ComboBox") == 0
                ? static_cast<int>(220.0f * dpiScale) : controlHeight;
            const auto widgetSetting = std::find_if(widgetSettingControls_.begin(),
                widgetSettingControls_.end(), [&](const WidgetSettingControls& item) {
                    return item.editor == fields[index].control;
                });
            if (widgetSetting != widgetSettingControls_.end() && widgetSetting->browse) {
                const int browseGap = static_cast<int>(8.0f * dpiScale);
                const int browseWidth = static_cast<int>(88.0f * dpiScale);
                place(fields[index].control, valueX, fieldY,
                    valueWidth - browseWidth - browseGap, controlHeight);
                place(widgetSetting->browse, valueX + valueWidth - browseWidth,
                    fieldY, browseWidth, controlHeight);
            } else {
                place(fields[index].control, valueX, fieldY, valueWidth, dropHeight);
            }
        }
        if (action) {
            const int actionColumn = fields.size() % 2 == 1 ? 1 : 0;
            const int actionRow = fields.size() % 2 == 1
                ? static_cast<int>(fields.size() / 2)
                : static_cast<int>((fields.size() + 1) / 2);
            place(action,
                innerX + actionColumn * (columnWidth + columnGap),
                firstY + actionRow * rowHeight,
                columnWidth, controlHeight);
        }
    };

    placeSection(layoutSection_, layoutFields_, y, pageHeight, applyUniversal_);
    placeSection(appearanceSection_, appearanceFields_, y, pageHeight, nullptr);
    placeSection(widgetSection_, widgetFields_, y, pageHeight, nullptr);
    place(widgetEmpty_, margin + static_cast<int>(14.0f * dpiScale),
        y + static_cast<int>(30.0f * dpiScale),
        contentWidth - static_cast<int>(28.0f * dpiScale), controlHeight);

    place(actionsSection_, margin, y, contentWidth, pageHeight);
    const int actionX = margin + gap;
    const int actionY = y + static_cast<int>(28.0f * dpiScale);
    const int buttonWidth = static_cast<int>(120.0f * dpiScale);
    place(openLibraryButton_, actionX, actionY, buttonWidth, controlHeight);
    place(duplicate_, actionX + buttonWidth + gap, actionY, buttonWidth, controlHeight);
    place(delete_, actionX + (buttonWidth + gap) * 2, actionY, buttonWidth, controlHeight);
    const int alignY = actionY + rowHeight;
    place(alignment_, actionX, alignY, static_cast<int>(250.0f * dpiScale),
        static_cast<int>(220.0f * dpiScale));
    place(applyAlignment_, actionX + static_cast<int>(264.0f * dpiScale), alignY,
        static_cast<int>(140.0f * dpiScale), controlHeight);

    // Reposition the complete child-window set as one operation. Copying old
    // child pixels while controls cross the viewport boundary leaves trails on
    // some Windows 10/DWM combinations, so every moved child repaints instead.
    constexpr UINT placementFlags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS;
    bool placed = false;
    if (HDWP deferred = BeginDeferWindowPos(static_cast<int>(placements.size()))) {
        for (const PendingPlacement& placement : placements) {
            deferred = DeferWindowPos(deferred, placement.control, nullptr,
                placement.x, placement.y, placement.width, placement.height, placementFlags);
            if (!deferred) break;
        }
        if (deferred) placed = EndDeferWindowPos(deferred) != FALSE;
    }
    if (!placed) {
        for (const PendingPlacement& placement : placements) {
            SetWindowPos(placement.control, nullptr,
                placement.x, placement.y, placement.width, placement.height, placementFlags);
        }
    }
    // Paint in two phases. WS_CLIPCHILDREN deliberately excludes the controls
    // at their new positions while the parent erases every uncovered pixel,
    // including their former positions. Only then repaint the visible controls.
    RedrawWindow(hwnd_, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_NOCHILDREN | RDW_UPDATENOW);
    for (const PendingPlacement& placement : placements) {
        if (!IsWindowVisible(placement.control)) continue;
        RedrawWindow(placement.control, nullptr, nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
}

void WidgetStudioWindow::UpdatePreviewMetrics() {
    if (!preview_ || !grid_) return;
    RECT rect{};
    GetClientRect(preview_, &rect);
    const float pixelsToDips = 96.0f / static_cast<float>(std::max(96u, GetDpiForWindow(preview_)));
    const float width = static_cast<float>(rect.right) * pixelsToDips;
    const float height = static_cast<float>(rect.bottom) * pixelsToDips;
    previewMetrics_ = layoutMetrics_;
    previewScale_ = std::min(width / std::max(1.0f, layoutBounds_.width),
        height / std::max(1.0f, layoutBounds_.height));
    previewScale_ = std::max(0.01f, previewScale_);
    previewOffset_ = PointF{
        (width - layoutBounds_.width * previewScale_) * 0.5f,
        (height - layoutBounds_.height * previewScale_) * 0.5f,
    };
}

void WidgetStudioWindow::PaintPreview() {
    PAINTSTRUCT paint{};
    BeginPaint(preview_, &paint);
    if (previewRenderer_ && scene_ && grid_) {
        RectF wallpaperBounds{};
        WallpaperMonitorGeometry wallpaperMonitor{};
        const auto monitor = std::find_if(monitors_.begin(), monitors_.end(), [this](const MonitorDescriptor& item) {
            return item.id == monitorId_;
        });
        if (monitor != monitors_.end()) {
            wallpaperBounds = {0.0f, 0.0f,
                static_cast<float>(monitor->monitorPixelWidth),
                static_cast<float>(monitor->monitorPixelHeight)};
            wallpaperMonitor = WidgetWindowPlacementCalculator::WallpaperMonitor(*monitor);
        }
        const HRESULT result = previewRenderer_->Render(
            *scene_, *grid_, previewMetrics_, true,
            SendMessageW(showGrid_, BM_GETCHECK, 0, 0) == BST_CHECKED,
            previewScale_, previewOffset_,
            {layoutBounds_.width, layoutBounds_.height}, wallpaperBounds, wallpaperMonitor, monitorId_);
        if (result == D2DERR_RECREATE_TARGET) InvalidateRect(preview_, nullptr, FALSE);
    }
    EndPaint(preview_, &paint);
}

WidgetInstance* WidgetStudioWindow::PrimaryWidget() noexcept {
    if (!scene_) return nullptr;
    const auto id = scene_->PrimarySelection();
    WidgetInstance* widget = id ? scene_->Find(*id) : nullptr;
    return widget && widget->monitorId == monitorId_ ? widget : nullptr;
}

void WidgetStudioWindow::UpdateControlsFromSelection() {
    updatingControls_ = true;
    WidgetInstance* widget = PrimaryWidget();
    const std::size_t activeSelectionCount = scene_ ? static_cast<std::size_t>(std::count_if(
        scene_->Widgets().begin(), scene_->Widgets().end(), [this](const WidgetInstance& item) {
            return item.selected && item.monitorId == monitorId_;
        })) : 0;
    const bool single = activeSelectionCount == 1;
    const WidgetDescriptor* descriptor = widget ? scene_->DescriptorFor(widget->instanceId) : nullptr;
    const bool scalable = descriptor && HasCapability(descriptor->capabilities, WidgetCapability::Scalable);
    const bool resizable = descriptor && HasCapability(descriptor->capabilities, WidgetCapability::Resizable);
    const bool duplicatable = descriptor && HasCapability(descriptor->capabilities, WidgetCapability::Duplicatable);
    const bool configurable = descriptor && HasCapability(descriptor->capabilities, WidgetCapability::Configurable);
    const bool hasWidget = widget != nullptr;
    const std::size_t freeSelectionCount = scene_ ? static_cast<std::size_t>(std::count_if(
        scene_->Widgets().begin(), scene_->Widgets().end(), [this](const WidgetInstance& item) {
            return item.selected && item.monitorId == monitorId_ && item.layoutMode == LayoutMode::Free;
        })) : 0;
    EnableWindow(monitorChoice_, widget != nullptr && !monitors_.empty());
    EnableWindow(layoutMode_, hasWidget);
    EnableWindow(locked_, hasWidget);
    EnableWindow(contentScale_, scalable);
    EnableWindow(appearanceMode_, hasWidget);
    EnableWindow(glass_, hasWidget);
    EnableWindow(opacity_, hasWidget);
    EnableWindow(blur_, hasWidget);
    EnableWindow(radius_, hasWidget);
    EnableWindow(padding_, hasWidget);
    EnableWindow(border_, hasWidget);
    EnableWindow(shadow_, hasWidget);
    EnableWindow(positionA_, single);
    EnableWindow(positionB_, single);
    EnableWindow(sizeA_, single && resizable);
    EnableWindow(sizeB_, single && resizable);
    EnableWindow(duplicate_, duplicatable);
    EnableWindow(delete_, hasWidget);
    EnableWindow(applyUniversal_, hasWidget);
    EnableWindow(alignment_, freeSelectionCount >= 2);
    EnableWindow(applyAlignment_, freeSelectionCount >= 2);
    RebuildWidgetSettings(widget);
    for (const WidgetSettingControls& controls : widgetSettingControls_) {
        EnableWindow(controls.editor, configurable);
        if (controls.browse) EnableWindow(controls.browse, configurable);
    }
    if (!widget) {
        updatingControls_ = false;
        return;
    }
    const auto monitor = std::find_if(monitors_.begin(), monitors_.end(), [widget](const MonitorDescriptor& item) {
        return item.id == widget->monitorId;
    });
    SendMessageW(monitorChoice_, CB_SETCURSEL,
        monitor == monitors_.end() ? CB_ERR : static_cast<WPARAM>(std::distance(monitors_.begin(), monitor)), 0);
    SendMessageW(layoutMode_, CB_SETCURSEL, widget->layoutMode == LayoutMode::Grid ? 0 : 1, 0);
    SendMessageW(locked_, BM_SETCHECK, widget->locked ? BST_CHECKED : BST_UNCHECKED, 0);
    SetNumber(contentScale_, widget->contentScale);
    SendMessageW(appearanceMode_, CB_SETCURSEL, widget->appearance.mode == AppearanceMode::Dark ? 0 : 1, 0);
    SendMessageW(glass_, CB_SETCURSEL, static_cast<WPARAM>(widget->appearance.surface), 0);
    SetNumber(opacity_, widget->appearance.opacity);
    SetNumber(blur_, widget->appearance.blurRadius);
    SetNumber(radius_, widget->appearance.cornerRadius);
    SetNumber(padding_, widget->appearance.innerPadding);
    SendMessageW(border_, BM_SETCHECK, widget->appearance.borderEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(shadow_, BM_SETCHECK, widget->appearance.shadowEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    UpdateLayoutSettingValues();
    UpdateWidgetSettingValues();
    updatingControls_ = false;
}

void WidgetStudioWindow::UpdateLayoutSettingValues() {
    WidgetInstance* widget = PrimaryWidget();
    if (!widget || !grid_) return;
    const LayoutMode requested = SendMessageW(layoutMode_, CB_GETCURSEL, 0, 0) == 1
        ? LayoutMode::Free : LayoutMode::Grid;
    if (layoutFields_.size() >= 6) {
        SetWindowTextW(layoutFields_[2].label,
            requested == LayoutMode::Free ? L"X position (DIP)" : L"Grid column");
        SetWindowTextW(layoutFields_[3].label,
            requested == LayoutMode::Free ? L"Y position (DIP)" : L"Grid row");
        SetWindowTextW(layoutFields_[4].label,
            requested == LayoutMode::Free ? L"Width (DIP)" : L"Column span");
        SetWindowTextW(layoutFields_[5].label,
            requested == LayoutMode::Free ? L"Height (DIP)" : L"Row span");
    }
    if (requested == LayoutMode::Free) {
        const RectF rect = OuterLayout::RectFor(*widget, *grid_, layoutMetrics_);
        SetNumber(positionA_, rect.x);
        SetNumber(positionB_, rect.y);
        SetNumber(sizeA_, rect.width);
        SetNumber(sizeB_, rect.height);
        return;
    }

    GridPlacement placement = widget->grid;
    if (widget->layoutMode == LayoutMode::Free) {
        const WidgetDescriptor* descriptor = scene_->DescriptorFor(widget->instanceId);
        const int minimumColumns = std::min(
            grid_->Columns(), descriptor ? descriptor->minimumGridSize.columns : 1);
        const int minimumRows = std::min(
            grid_->Rows(), descriptor ? descriptor->minimumGridSize.rows : 1);
        const int maximumColumns = std::max(minimumColumns,
            std::min(grid_->Columns(), descriptor
                ? descriptor->maximumGridSize.columns : grid_->Columns()));
        const int maximumRows = std::max(minimumRows,
            std::min(grid_->Rows(), descriptor
                ? descriptor->maximumGridSize.rows : grid_->Rows()));
        placement = OuterLayout::GridForRect(
            RectF{widget->free.x, widget->free.y, widget->free.width, widget->free.height},
            placement, *grid_, layoutMetrics_,
            minimumColumns, minimumRows, maximumColumns, maximumRows);
    }
    SetNumber(positionA_, placement.column);
    SetNumber(positionB_, placement.row);
    SetNumber(sizeA_, placement.columnSpan);
    SetNumber(sizeB_, placement.rowSpan);
}

void WidgetStudioWindow::RebuildWidgetSettings(const WidgetInstance* widget) {
    const std::string typeId = widget ? widget->typeId : std::string{};
    const auto settings = widget && widget->content
        ? widget->content->Settings() : std::span<const WidgetSettingDefinition>{};
    if (typeId == widgetSettingsTypeId_ && settings.size() == widgetSettingControls_.size()) return;

    for (const WidgetSettingControls& controls : widgetSettingControls_) {
        if (controls.label) DestroyWindow(controls.label);
        if (controls.editor) DestroyWindow(controls.editor);
        if (controls.browse) DestroyWindow(controls.browse);
    }
    widgetSettingControls_.clear();
    widgetFields_.clear();
    widgetSettingsTypeId_ = typeId;

    for (std::size_t index = 0; index < settings.size(); ++index) {
        const WidgetSettingDefinition& definition = settings[index];
        HWND label = AddControl(hwnd_, instance_, L"STATIC", definition.displayName.c_str(), SS_LEFT);
        HWND editor = nullptr;
        const int controlIndex = static_cast<int>(widgetSettingControls_.size());
        const int id = kWidgetSettingBase + controlIndex;
        if (definition.kind == WidgetSettingKind::Boolean) {
            editor = AddControl(hwnd_, instance_, L"BUTTON", L"Enabled",
                BS_AUTOCHECKBOX | WS_TABSTOP, id);
        } else if (definition.kind == WidgetSettingKind::Choice) {
            editor = AddControl(hwnd_, instance_, L"COMBOBOX", nullptr,
                CBS_DROPDOWNLIST | WS_TABSTOP, id);
            for (std::size_t choice = 0; choice < definition.choices.size(); ++choice) {
                const std::wstring& display = choice < definition.choiceDisplayNames.size()
                    ? definition.choiceDisplayNames[choice] : definition.choices[choice];
                SendMessageW(editor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(display.c_str()));
            }
        } else {
            editor = AddControl(hwnd_, instance_, L"EDIT", nullptr,
                ES_AUTOHSCROLL | WS_TABSTOP, id, WS_EX_CLIENTEDGE);
        }
        HWND browse = definition.kind == WidgetSettingKind::File
            ? AddControl(hwnd_, instance_, L"BUTTON", L"Browse...", BS_PUSHBUTTON | WS_TABSTOP,
                kWidgetBrowseBase + controlIndex)
            : nullptr;
        if (!label || !editor || (definition.kind == WidgetSettingKind::File && !browse)) {
            if (label) DestroyWindow(label);
            if (editor) DestroyWindow(editor);
            if (browse) DestroyWindow(browse);
            continue;
        }
        widgetSettingControls_.push_back({definition, label, editor, browse});
        widgetFields_.push_back({label, editor});
    }

    SetWindowTextW(widgetEmpty_, widget
        ? L"This widget has no content settings."
        : L"Select a widget in the preview to edit its content.");
    std::wstring heading = L"Widget content";
    if (widget && scene_) {
        if (const WidgetDescriptor* descriptor = scene_->DescriptorFor(widget->instanceId)) {
            heading = descriptor->displayName + L" content";
        }
    }
    SetWindowTextW(widgetSection_, heading.c_str());
    UpdateSettingsPageVisibility();
    if (hwnd_) {
        RECT client{};
        GetClientRect(hwnd_, &client);
        LayoutControls(client.right - client.left, client.bottom - client.top);
    }
}

void WidgetStudioWindow::UpdateWidgetSettingValues() {
    WidgetInstance* widget = PrimaryWidget();
    if (!widget || !widget->content) return;
    const auto state = widget->content->SaveState();
    for (const WidgetSettingControls& controls : widgetSettingControls_) {
        const WidgetSettingDefinition& definition = controls.definition;
        const auto value = state.find(definition.key);
        const std::wstring current = value == state.end() ? L"" : value->second;
        if (definition.kind == WidgetSettingKind::Choice) {
            int selected = 0;
            for (std::size_t choice = 0; choice < definition.choices.size(); ++choice) {
                if (definition.choices[choice] == current) selected = static_cast<int>(choice);
            }
            SendMessageW(controls.editor, CB_SETCURSEL, selected, 0);
        } else if (definition.kind == WidgetSettingKind::Boolean) {
            const bool checked = current == L"true" || current == L"1";
            SendMessageW(controls.editor, BM_SETCHECK,
                checked ? BST_CHECKED : BST_UNCHECKED, 0);
        } else {
            SetWindowTextW(controls.editor, current.c_str());
        }
    }
}

void WidgetStudioWindow::ApplyUniversalSettings() {
    if (updatingControls_) return;
    WidgetInstance* primary = PrimaryWidget();
    if (!primary) return;
    const LayoutMode mode = SendMessageW(layoutMode_, CB_GETCURSEL, 0, 0) == 1 ? LayoutMode::Free : LayoutMode::Grid;
    const std::size_t activeSelectionCount = static_cast<std::size_t>(std::count_if(
        scene_->Widgets().begin(), scene_->Widgets().end(), [this](const WidgetInstance& widget) {
            return widget.selected && widget.monitorId == monitorId_;
        }));
    const bool single = activeSelectionCount == 1;
    const LRESULT monitorSelection = SendMessageW(monitorChoice_, CB_GETCURSEL, 0, 0);
    const MonitorDescriptor* destination = monitorSelection != CB_ERR &&
        static_cast<std::size_t>(monitorSelection) < monitors_.size()
        ? &monitors_[static_cast<std::size_t>(monitorSelection)] : nullptr;
    const RectF destinationBounds = destination ? destination->monitorBoundsDips : layoutBounds_;
    for (auto& widget : scene_->Widgets()) {
        if (!widget.selected || widget.monitorId != monitorId_) continue;
        const WidgetDescriptor* descriptor = scene_->DescriptorFor(widget.instanceId);
        const WidgetCapability capabilities = descriptor
            ? descriptor->capabilities : WidgetCapability::None;
        scene_->SetWidgetLayoutMode(widget.instanceId, mode, *grid_, layoutMetrics_);
        if (destination) widget.monitorId = destination->id;
        widget.locked = SendMessageW(locked_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (HasCapability(capabilities, WidgetCapability::Scalable)) {
            widget.contentScale = std::clamp(
                static_cast<float>(ReadNumber(contentScale_, widget.contentScale)), 0.25f, 4.0f);
        }
        widget.appearance.mode = SendMessageW(appearanceMode_, CB_GETCURSEL, 0, 0) == 1
            ? AppearanceMode::Light : AppearanceMode::Dark;
        const LRESULT surface = SendMessageW(glass_, CB_GETCURSEL, 0, 0);
        widget.appearance.surface = surface == 1 ? SurfaceMode::Transparent :
            surface == 2 ? SurfaceMode::Solid : SurfaceMode::Frosted;
        widget.appearance.glassEnabled = widget.appearance.surface == SurfaceMode::Frosted;
        widget.appearance.opacity = std::clamp(static_cast<float>(ReadNumber(opacity_, widget.appearance.opacity)), 0.0f, 1.0f);
        widget.appearance.blurRadius = std::clamp(static_cast<float>(ReadNumber(blur_, widget.appearance.blurRadius)), 0.0f, 128.0f);
        widget.appearance.cornerRadius = std::clamp(static_cast<float>(ReadNumber(radius_, widget.appearance.cornerRadius)), 0.0f, 128.0f);
        widget.appearance.innerPadding = std::clamp(
            static_cast<float>(ReadNumber(padding_, widget.appearance.innerPadding)), 0.0f, 64.0f);
        widget.appearance.borderEnabled = SendMessageW(border_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        widget.appearance.shadowEnabled = SendMessageW(shadow_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (widget.layoutMode == LayoutMode::Grid && single) {
            const int minimumColumns = descriptor
                ? std::min(grid_->Columns(), descriptor->minimumGridSize.columns) : 1;
            const int minimumRows = descriptor
                ? std::min(grid_->Rows(), descriptor->minimumGridSize.rows) : 1;
            const int maximumColumns = descriptor
                ? std::max(minimumColumns,
                    std::min(grid_->Columns(), descriptor->maximumGridSize.columns))
                : grid_->Columns();
            const int maximumRows = descriptor
                ? std::max(minimumRows,
                    std::min(grid_->Rows(), descriptor->maximumGridSize.rows))
                : grid_->Rows();
            if (HasCapability(capabilities, WidgetCapability::Resizable)) {
                widget.grid.columnSpan = std::clamp(
                    static_cast<int>(ReadNumber(sizeA_, widget.grid.columnSpan)),
                    minimumColumns, maximumColumns);
                widget.grid.rowSpan = std::clamp(
                    static_cast<int>(ReadNumber(sizeB_, widget.grid.rowSpan)),
                    minimumRows, maximumRows);
            }
            widget.grid.column = std::clamp(static_cast<int>(ReadNumber(positionA_, widget.grid.column)),
                0, grid_->Columns() - widget.grid.columnSpan);
            widget.grid.row = std::clamp(static_cast<int>(ReadNumber(positionB_, widget.grid.row)),
                0, grid_->Rows() - widget.grid.rowSpan);
        } else if (widget.layoutMode == LayoutMode::Free) {
            if (single && HasCapability(capabilities, WidgetCapability::Resizable)) {
                widget.free.width = std::clamp(
                    static_cast<float>(ReadNumber(sizeA_, widget.free.width)),
                    1.0f, std::max(1.0f, destinationBounds.width));
                widget.free.height = std::clamp(
                    static_cast<float>(ReadNumber(sizeB_, widget.free.height)),
                    1.0f, std::max(1.0f, destinationBounds.height));
            }
            widget.free.width = std::clamp(
                widget.free.width, 1.0f, std::max(1.0f, destinationBounds.width));
            widget.free.height = std::clamp(
                widget.free.height, 1.0f, std::max(1.0f, destinationBounds.height));
            const float requestedX = single
                ? static_cast<float>(ReadNumber(positionA_, widget.free.x)) : widget.free.x;
            const float requestedY = single
                ? static_cast<float>(ReadNumber(positionB_, widget.free.y)) : widget.free.y;
            widget.free.x = std::clamp(requestedX,
                destinationBounds.x, destinationBounds.x + destinationBounds.width - widget.free.width);
            widget.free.y = std::clamp(requestedY,
                destinationBounds.y, destinationBounds.y + destinationBounds.height - widget.free.height);
        }
    }
    if (destination) {
        monitorId_ = destination->id;
        layoutBounds_ = destination->monitorBoundsDips;
        layoutMetrics_ = grid_->Calculate({layoutBounds_.width, layoutBounds_.height});
        UpdatePreviewMetrics();
    }
    NotifySceneChanged();
}

void WidgetStudioWindow::ApplyRequestedLayoutMode() {
    if (updatingControls_ || !scene_ || !grid_) return;
    const LayoutMode mode = SendMessageW(layoutMode_, CB_GETCURSEL, 0, 0) == 1
        ? LayoutMode::Free : LayoutMode::Grid;
    bool changed = false;
    for (WidgetInstance& widget : scene_->Widgets()) {
        if (!widget.selected || widget.monitorId != monitorId_) continue;
        if (widget.layoutMode != mode) {
            scene_->SetWidgetLayoutMode(widget.instanceId, mode, *grid_, layoutMetrics_);
            changed = true;
        }
    }
    if (changed) NotifySceneChanged();
    else UpdateLayoutSettingValues();
}

void WidgetStudioWindow::ApplyAlignment() {
    const LRESULT selection = SendMessageW(alignment_, CB_GETCURSEL, 0, 0);
    if (selection != CB_ERR && scene_->AlignSelected(
            static_cast<AlignmentOperation>(selection), layoutBounds_, monitorId_))
        NotifySceneChanged();
}

void WidgetStudioWindow::ApplyWidgetSetting(std::size_t index) {
    if (updatingControls_) return;
    WidgetInstance* widget = PrimaryWidget();
    if (!widget || index >= widgetSettingControls_.size()) return;
    const WidgetSettingControls& controls = widgetSettingControls_[index];
    const WidgetSettingDefinition& definition = controls.definition;
    WidgetState state = widget->content->SaveState();
    std::wstring value;
    if (definition.kind == WidgetSettingKind::Boolean) {
        value = SendMessageW(controls.editor, BM_GETCHECK, 0, 0) == BST_CHECKED
            ? L"true" : L"false";
    } else if (definition.kind == WidgetSettingKind::Choice) {
        const LRESULT choice = SendMessageW(controls.editor, CB_GETCURSEL, 0, 0);
        if (choice == CB_ERR || static_cast<std::size_t>(choice) >= definition.choices.size()) return;
        value = definition.choices[static_cast<std::size_t>(choice)];
    } else if (definition.kind == WidgetSettingKind::Number) {
        const auto current = state.find(definition.key);
        const double fallback = current == state.end()
            ? definition.minimum : ParseNumber(current->second, definition.minimum);
        double number = ReadNumber(controls.editor, fallback);
        if (definition.maximum >= definition.minimum) {
            number = std::clamp(number, definition.minimum, definition.maximum);
            if (definition.step > 0.0 && std::isfinite(definition.step)) {
                number = definition.minimum + std::round(
                    (number - definition.minimum) / definition.step) * definition.step;
                number = std::clamp(number, definition.minimum, definition.maximum);
            }
        }
        SetNumber(controls.editor, number);
        value = ReadText(controls.editor);
    } else {
        value = ReadText(controls.editor);
    }
    state[definition.key] = std::move(value);
    widget->content->RestoreState(state);
    NotifySceneChanged();
}

void WidgetStudioWindow::ChooseWidgetFile(std::size_t index) {
    if (index >= widgetSettingControls_.size() ||
        widgetSettingControls_[index].definition.kind != WidgetSettingKind::File) return;
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(dialog.GetAddressOf())))) return;
    const COMDLG_FILTERSPEC filters[]{{L"Images", L"*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tif;*.tiff;*.webp"},
                                      {L"All files", L"*.*"}};
    dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
    if (FAILED(dialog->Show(hwnd_))) return;
    Microsoft::WRL::ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(item.GetAddressOf()))) return;
    PWSTR path = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) return;
    std::wstring error;
    const auto imported = assetLibrary_->Import(path, error);
    CoTaskMemFree(path);
    if (!imported) { MessageBoxW(hwnd_, error.c_str(), L"Widget Studio", MB_OK | MB_ICONWARNING); return; }
    const std::wstring reference = assetLibrary_->ReferenceFor(*imported);
    SetWindowTextW(widgetSettingControls_[index].editor, reference.c_str());
    ApplyWidgetSetting(index);
}

void WidgetStudioWindow::NotifySceneChanged() {
    if (sceneChanged_) sceneChanged_();
    UpdateControlsFromSelection();
    InvalidateRect(preview_, nullptr, FALSE);
}

void WidgetStudioWindow::EndPreviewDrag() {
    if (!previewDrag_) return;
    const bool moved = previewDrag_->moved;
    previewDrag_.reset();
    if (GetCapture() == preview_) ReleaseCapture();
    if (moved) NotifySceneChanged();
}

bool WidgetStudioWindow::HandleEditKey(WPARAM key) {
    if (key == VK_DELETE) {
        EndPreviewDrag();
        if (scene_->RemoveSelectedWidgets() > 0) NotifySceneChanged();
        return true;
    }
    if ((GetKeyState(VK_CONTROL) & 0x8000) == 0) return false;
    const auto primary = scene_->PrimarySelection();
    if (key == 'D') {
        EndPreviewDrag();
        if (primary && scene_->DuplicateWidget(*primary, layoutBounds_)) NotifySceneChanged();
        return true;
    }
    if (key == 'L') {
        EndPreviewDrag();
        WidgetInstance* widget = primary ? scene_->Find(*primary) : nullptr;
        if (widget && scene_->SetWidgetLocked(widget->instanceId, !widget->locked)) {
            NotifySceneChanged();
        }
        return true;
    }
    return false;
}

LRESULT WidgetStudioWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: return CreateControls() ? 0 : -1;
    case WM_GETMINMAXINFO: {
        const float scale = static_cast<float>(std::max(96u, GetDpiForWindow(hwnd_))) / 96.0f;
        auto* bounds = reinterpret_cast<MINMAXINFO*>(lParam);
        bounds->ptMinTrackSize.x = static_cast<LONG>(760.0f * scale);
        bounds->ptMinTrackSize.y = static_cast<LONG>(650.0f * scale);
        return 0;
    }
    case WM_SIZE: LayoutControls(LOWORD(lParam), HIWORD(lParam)); UpdatePreviewMetrics(); return 0;
    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(hwnd_, &client);
        FillRect(reinterpret_cast<HDC>(wParam), &client,
            backgroundBrush_ ? backgroundBrush_ : static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, RGB(232, 235, 239));
        SetBkColor(dc, RGB(13, 15, 18));
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(backgroundBrush_);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, RGB(245, 247, 250));
        SetBkColor(dc, RGB(22, 25, 30));
        return reinterpret_cast<LRESULT>(fieldBrush_);
    }
    case WM_DRAWITEM: {
        const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (item && item->CtlType == ODT_BUTTON) {
            const bool navigation = item->CtlID >= kSettingsPageBase &&
                item->CtlID < kSettingsPageBase + settingsPageButtons_.size();
            DrawSettingsNavigationButton(*item, navigation &&
                static_cast<int>(item->CtlID - kSettingsPageBase) == activeSettingsPage_);
            return TRUE;
        }
        if (item && item->CtlType == ODT_COMBOBOX) {
            DrawDarkComboItem(*item);
            return TRUE;
        }
        break;
    }
    case WM_MEASUREITEM: {
        auto* item = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (item && item->CtlType == ODT_COMBOBOX) {
            const UINT dpi = std::max(96u, GetDpiForWindow(hwnd_));
            item->itemHeight = static_cast<UINT>(MulDiv(24, static_cast<int>(dpi), 96));
            return TRUE;
        }
        break;
    }
    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        if (previewRenderer_) previewRenderer_->SetDpi(static_cast<float>(GetDpiForWindow(preview_)));
        UpdatePreviewMetrics();
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) >= kSettingsPageBase &&
            LOWORD(wParam) < kSettingsPageBase + static_cast<int>(settingsPageButtons_.size()) &&
            HIWORD(wParam) == BN_CLICKED) {
            activeSettingsPage_ = static_cast<int>(LOWORD(wParam) - kSettingsPageBase);
            UpdateSettingsPageVisibility();
            RECT client{};
            GetClientRect(hwnd_, &client);
            LayoutControls(client.right - client.left, client.bottom - client.top);
            return 0;
        }
        if (LOWORD(wParam) >= kWidgetSettingBase &&
            LOWORD(wParam) < kWidgetSettingBase + static_cast<int>(widgetSettingControls_.size())) {
            const std::size_t index = static_cast<std::size_t>(LOWORD(wParam) - kWidgetSettingBase);
            const WidgetSettingKind kind = widgetSettingControls_[index].definition.kind;
            if ((kind == WidgetSettingKind::Boolean && HIWORD(wParam) == BN_CLICKED) ||
                (kind == WidgetSettingKind::Choice && HIWORD(wParam) == CBN_SELCHANGE) ||
                (kind != WidgetSettingKind::Boolean && kind != WidgetSettingKind::Choice &&
                    HIWORD(wParam) == EN_KILLFOCUS)) {
                ApplyWidgetSetting(index);
                return 0;
            }
        }
        if (LOWORD(wParam) >= kWidgetBrowseBase &&
            LOWORD(wParam) < kWidgetBrowseBase + static_cast<int>(widgetSettingControls_.size()) &&
            HIWORD(wParam) == BN_CLICKED) {
            ChooseWidgetFile(static_cast<std::size_t>(LOWORD(wParam) - kWidgetBrowseBase));
            return 0;
        }
        switch (LOWORD(wParam)) {
        case kApplyUniversal: ApplyUniversalSettings(); return 0;
        case kApplyAlignment: ApplyAlignment(); return 0;
        case kOpenLibrary: if (openLibrary_) openLibrary_(); return 0;
        case kDuplicateWidget: {
            const auto primary = scene_->PrimarySelection();
            if (primary && scene_->DuplicateWidget(*primary, layoutBounds_)) NotifySceneChanged();
            return 0;
        }
        case kDeleteWidget:
            if (scene_->RemoveSelectedWidgets() > 0) NotifySceneChanged();
            return 0;
        case kLayoutMode:
            if (HIWORD(wParam) == CBN_SELCHANGE) { ApplyRequestedLayoutMode(); return 0; }
            break;
        case kMonitorChoice:
        case kAppearanceMode:
        case kGlass:
            if (HIWORD(wParam) == CBN_SELCHANGE) { ApplyUniversalSettings(); return 0; }
            break;
        case kLocked:
        case kBorder:
        case kShadow:
            if (HIWORD(wParam) == BN_CLICKED) { ApplyUniversalSettings(); return 0; }
            break;
        case kShowGrid:
            if (HIWORD(wParam) == BN_CLICKED) { InvalidateRect(preview_, nullptr, FALSE); return 0; }
            break;
        case kContentScale:
        case kOpacity:
        case kBlur:
        case kRadius:
        case kPadding:
        case kPositionA:
        case kPositionB:
        case kSizeA:
        case kSizeB:
            if (HIWORD(wParam) == EN_KILLFOCUS) { ApplyUniversalSettings(); return 0; }
            break;
        default: break;
        }
        break;
    case WM_KEYDOWN:
        if (HandleEditKey(wParam)) return 0;
        break;
    case WM_CLOSE: DestroyWindow(hwnd_); return 0;
    case WM_NCDESTROY: {
        const HWND destroyedWindow = hwnd_;
        previewRenderer_.reset();
        assetLibrary_.reset();
        if (backgroundBrush_) DeleteObject(backgroundBrush_);
        if (fieldBrush_) DeleteObject(fieldBrush_);
        backgroundBrush_ = nullptr;
        fieldBrush_ = nullptr;
        SetWindowLongPtrW(destroyedWindow, GWLP_USERDATA, 0);
        hwnd_ = nullptr;
        ResetControlHandles();
        return DefWindowProcW(destroyedWindow, message, wParam, lParam);
    }
    default: break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

LRESULT WidgetStudioWindow::HandlePreviewMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_SIZE:
        UpdatePreviewMetrics();
        if (previewRenderer_) previewRenderer_->Resize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_PAINT: PaintPreview(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN: {
        const float scale = 96.0f / static_cast<float>(std::max(96u, GetDpiForWindow(preview_)));
        const PointF point{
            (static_cast<float>(GET_X_LPARAM(lParam)) * scale - previewOffset_.x) / previewScale_,
            (static_cast<float>(GET_Y_LPARAM(lParam)) * scale - previewOffset_.y) / previewScale_,
        };
        const auto hit = scene_->HitTest(point, *grid_, previewMetrics_, monitorId_);
        if (hit) scene_->Select(*hit, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
        else scene_->ClearSelection();
        const WidgetInstance* selected = hit ? scene_->Find(*hit) : nullptr;
        if (selected && selected->selected && !selected->locked) {
            const RectF rect = OuterLayout::RectFor(*selected, *grid_, previewMetrics_);
            previewDrag_ = PreviewDragState{
                .widgetId = selected->instanceId,
                .offset = {point.x - rect.x, point.y - rect.y},
            };
            SetCapture(preview_);
            SetFocus(preview_);
        }
        UpdateControlsFromSelection();
        InvalidateRect(preview_, nullptr, FALSE);
        if (selectionChanged_) selectionChanged_();
        return 0;
    }
    case WM_MOUSEMOVE:
        if (previewDrag_ && (wParam & MK_LBUTTON) != 0) {
            const float scale = 96.0f / static_cast<float>(std::max(96u, GetDpiForWindow(preview_)));
            const PointF point{
                (static_cast<float>(GET_X_LPARAM(lParam)) * scale - previewOffset_.x) / previewScale_,
                (static_cast<float>(GET_Y_LPARAM(lParam)) * scale - previewOffset_.y) / previewScale_,
            };
            WidgetInstance* widget = scene_->Find(previewDrag_->widgetId);
            if (!widget || widget->locked) {
                EndPreviewDrag();
                return 0;
            }
            if (widget->layoutMode == LayoutMode::Grid) {
                const GridPlacement moved = grid_->MoveToPoint(
                    widget->grid, point, previewDrag_->offset, previewMetrics_);
                if (moved.column != widget->grid.column || moved.row != widget->grid.row) {
                    widget->grid = moved;
                    previewDrag_->moved = true;
                }
            } else {
                const FreePlacement moved = OuterLayout::MoveFreeToPoint(
                    widget->free, point, previewDrag_->offset, layoutBounds_);
                if (moved.x != widget->free.x || moved.y != widget->free.y) {
                    widget->free = moved;
                    previewDrag_->moved = true;
                }
            }
            if (previewDrag_->moved) {
                InvalidateRect(preview_, nullptr, FALSE);
                if (selectionChanged_) selectionChanged_();
            }
            return 0;
        }
        if (previewDrag_) EndPreviewDrag();
        return 0;
    case WM_LBUTTONUP:
        EndPreviewDrag();
        return 0;
    case WM_KEYDOWN:
        if (HandleEditKey(wParam)) return 0;
        break;
    case WM_CAPTURECHANGED:
        if (previewDrag_) {
            const bool moved = previewDrag_->moved;
            previewDrag_.reset();
            if (moved) NotifySceneChanged();
        }
        return 0;
    default: break;
    }
    return DefWindowProcW(preview_, message, wParam, lParam);
}

} // namespace ws
