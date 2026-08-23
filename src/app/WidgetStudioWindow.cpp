#include "app/WidgetStudioWindow.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <shobjidl.h>
#include <string>
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
constexpr int kApplyWidget = 202;
constexpr int kBrowse = 203;
constexpr int kSettingCombo = 204;
constexpr int kOpenLibrary = 205;
constexpr int kDuplicateWidget = 206;
constexpr int kDeleteWidget = 207;

HWND AddControl(HWND parent, HINSTANCE instance, const wchar_t* type, const wchar_t* text,
    DWORD style, int id = 0, DWORD extendedStyle = 0) {
    HWND control = CreateWindowExW(extendedStyle, type, text, WS_CHILD | WS_VISIBLE | style,
        0, 0, 100, 24, parent, id ? reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)) : nullptr,
        instance, nullptr);
    if (control) SendMessageW(control, WM_SETFONT,
        reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    return control;
}

void SetNumber(HWND control, double value) {
    wchar_t text[48]{};
    _snwprintf_s(text, _countof(text), _TRUNCATE, L"%.4g", value);
    SetWindowTextW(control, text);
}

double ReadNumber(HWND control, double fallback) {
    wchar_t text[64]{};
    GetWindowTextW(control, text, static_cast<int>(std::size(text)));
    wchar_t* end = nullptr;
    const double value = std::wcstod(text, &end);
    return end != text && *end == L'\0' && std::isfinite(value) ? value : fallback;
}

std::wstring ReadText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) return {};
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

} // namespace

WidgetStudioWindow::~WidgetStudioWindow() { Close(); }

bool WidgetStudioWindow::Open(HWND owner, HINSTANCE instance, WidgetScene& scene, GridLayout& grid,
    GridMetrics layoutMetrics, RectF layoutBounds, std::filesystem::path assetDirectory,
    std::wstring monitorId, std::function<void()> sceneChanged, std::function<void()> openLibrary) {
    if (hwnd_) { ShowWindow(hwnd_, SW_SHOWNORMAL); SetForegroundWindow(hwnd_); Refresh(); return true; }
    instance_ = instance;
    owner_ = owner;
    scene_ = &scene;
    grid_ = &grid;
    layoutMetrics_ = layoutMetrics;
    layoutBounds_ = layoutBounds;
    monitorId_ = std::move(monitorId);
    assetLibrary_ = std::make_unique<AssetLibrary>(std::move(assetDirectory));
    sceneChanged_ = std::move(sceneChanged);
    openLibrary_ = std::move(openLibrary);

    WNDCLASSEXW previewClass{};
    previewClass.cbSize = sizeof(previewClass);
    previewClass.lpfnWndProc = PreviewProc;
    previewClass.hInstance = instance_;
    previewClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    previewClass.lpszClassName = kPreviewClass;
    if (!RegisterClassExW(&previewClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    WNDCLASSEXW studioClass{};
    studioClass.cbSize = sizeof(studioClass);
    studioClass.lpfnWndProc = WindowProc;
    studioClass.hInstance = instance_;
    studioClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    studioClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    studioClass.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_BTNFACE + 1));
    studioClass.lpszClassName = kStudioClass;
    if (!RegisterClassExW(&studioClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    hwnd_ = CreateWindowExW(WS_EX_APPWINDOW, kStudioClass, L"Widget Studio",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1120, 820,
        owner, nullptr, instance_, this);
    if (!hwnd_ || !preview_) { Close(); return false; }
    previewRenderer_ = std::make_unique<Renderer>();
    if (FAILED(previewRenderer_->Initialize(preview_))) { Close(); return false; }
    UpdatePreviewMetrics();
    UpdateControlsFromSelection();
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    UpdateWindow(hwnd_);
    return true;
}

void WidgetStudioWindow::Close() noexcept {
    if (hwnd_ && IsWindow(hwnd_)) DestroyWindow(hwnd_);
    previewRenderer_.reset();
    assetLibrary_.reset();
    hwnd_ = nullptr;
    preview_ = nullptr;
    owner_ = nullptr;
}

void WidgetStudioWindow::Refresh() {
    if (!hwnd_) return;
    UpdateControlsFromSelection();
    InvalidateRect(preview_, nullptr, FALSE);
}

void WidgetStudioWindow::UpdateLayoutContext(
    GridMetrics layoutMetrics, RectF layoutBounds, std::wstring monitorId) {
    layoutMetrics_ = layoutMetrics;
    layoutBounds_ = layoutBounds;
    monitorId_ = std::move(monitorId);
    UpdatePreviewMetrics();
    if (preview_) InvalidateRect(preview_, nullptr, FALSE);
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
    preview_ = CreateWindowExW(WS_EX_CLIENTEDGE, kPreviewClass, nullptr, WS_CHILD | WS_VISIBLE,
        0, 0, 100, 100, hwnd_, nullptr, instance_, this);
    AddControl(hwnd_, instance_, L"STATIC", L"Layout", 0);
    layoutMode_ = AddControl(hwnd_, instance_, L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP);
    SendMessageW(layoutMode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Grid"));
    SendMessageW(layoutMode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Free"));
    locked_ = AddControl(hwnd_, instance_, L"BUTTON", L"Locked", BS_AUTOCHECKBOX | WS_TABSTOP);
    AddControl(hwnd_, instance_, L"STATIC", L"Scale", 0);
    contentScale_ = AddControl(hwnd_, instance_, L"EDIT", nullptr, ES_AUTOHSCROLL | WS_TABSTOP, 0, WS_EX_CLIENTEDGE);
    AddControl(hwnd_, instance_, L"STATIC", L"Appearance", 0);
    appearanceMode_ = AddControl(hwnd_, instance_, L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP);
    SendMessageW(appearanceMode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Dark"));
    SendMessageW(appearanceMode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Light"));
    glass_ = AddControl(hwnd_, instance_, L"BUTTON", L"Glass", BS_AUTOCHECKBOX | WS_TABSTOP);
    AddControl(hwnd_, instance_, L"STATIC", L"Opacity", 0);
    opacity_ = AddControl(hwnd_, instance_, L"EDIT", nullptr, ES_AUTOHSCROLL | WS_TABSTOP, 0, WS_EX_CLIENTEDGE);
    AddControl(hwnd_, instance_, L"STATIC", L"Blur", 0);
    blur_ = AddControl(hwnd_, instance_, L"EDIT", nullptr, ES_AUTOHSCROLL | WS_TABSTOP, 0, WS_EX_CLIENTEDGE);
    AddControl(hwnd_, instance_, L"STATIC", L"Radius", 0);
    radius_ = AddControl(hwnd_, instance_, L"EDIT", nullptr, ES_AUTOHSCROLL | WS_TABSTOP, 0, WS_EX_CLIENTEDGE);
    AddControl(hwnd_, instance_, L"STATIC", L"Column / X", 0);
    positionA_ = AddControl(hwnd_, instance_, L"EDIT", nullptr, ES_AUTOHSCROLL | WS_TABSTOP, 0, WS_EX_CLIENTEDGE);
    AddControl(hwnd_, instance_, L"STATIC", L"Row / Y", 0);
    positionB_ = AddControl(hwnd_, instance_, L"EDIT", nullptr, ES_AUTOHSCROLL | WS_TABSTOP, 0, WS_EX_CLIENTEDGE);
    AddControl(hwnd_, instance_, L"STATIC", L"Col span / Width", 0);
    sizeA_ = AddControl(hwnd_, instance_, L"EDIT", nullptr, ES_AUTOHSCROLL | WS_TABSTOP, 0, WS_EX_CLIENTEDGE);
    AddControl(hwnd_, instance_, L"STATIC", L"Row span / Height", 0);
    sizeB_ = AddControl(hwnd_, instance_, L"EDIT", nullptr, ES_AUTOHSCROLL | WS_TABSTOP, 0, WS_EX_CLIENTEDGE);
    AddControl(hwnd_, instance_, L"BUTTON", L"Apply universal settings", BS_PUSHBUTTON | WS_TABSTOP, kApplyUniversal);
    AddControl(hwnd_, instance_, L"BUTTON", L"Add widget...", BS_PUSHBUTTON | WS_TABSTOP, kOpenLibrary);
    AddControl(hwnd_, instance_, L"BUTTON", L"Duplicate", BS_PUSHBUTTON | WS_TABSTOP, kDuplicateWidget);
    AddControl(hwnd_, instance_, L"BUTTON", L"Remove", BS_PUSHBUTTON | WS_TABSTOP, kDeleteWidget);
    alignment_ = AddControl(hwnd_, instance_, L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP);
    for (const wchar_t* item : {L"Align left", L"Horizontal center", L"Align right", L"Align top",
            L"Vertical center", L"Align bottom", L"Match width", L"Match height", L"Match both",
            L"Distribute horizontally", L"Distribute vertically"})
        SendMessageW(alignment_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
    SendMessageW(alignment_, CB_SETCURSEL, 0, 0);
    AddControl(hwnd_, instance_, L"BUTTON", L"Apply alignment", BS_PUSHBUTTON | WS_TABSTOP, kApplyAlignment);
    AddControl(hwnd_, instance_, L"STATIC", L"Widget setting", 0);
    widgetSetting_ = AddControl(hwnd_, instance_, L"COMBOBOX", nullptr,
        CBS_DROPDOWNLIST | WS_TABSTOP, kSettingCombo);
    widgetValue_ = AddControl(hwnd_, instance_, L"EDIT", nullptr, ES_AUTOHSCROLL | WS_TABSTOP, 0, WS_EX_CLIENTEDGE);
    widgetChoice_ = AddControl(hwnd_, instance_, L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP);
    widgetCheck_ = AddControl(hwnd_, instance_, L"BUTTON", L"Enabled", BS_AUTOCHECKBOX | WS_TABSTOP);
    ShowWindow(widgetChoice_, SW_HIDE);
    ShowWindow(widgetCheck_, SW_HIDE);
    browse_ = AddControl(hwnd_, instance_, L"BUTTON", L"Browse...", BS_PUSHBUTTON | WS_TABSTOP, kBrowse);
    ShowWindow(browse_, SW_HIDE);
    AddControl(hwnd_, instance_, L"BUTTON", L"Apply widget setting", BS_PUSHBUTTON | WS_TABSTOP, kApplyWidget);
    return preview_ && layoutMode_ && locked_ && contentScale_ && widgetSetting_ && widgetValue_;
}

void WidgetStudioWindow::LayoutControls(int width, int height) {
    const int margin = 16;
    const int previewHeight = std::clamp(height * 58 / 100, 280, 470);
    MoveWindow(preview_, margin, margin, std::max(1, width - margin * 2), previewHeight, TRUE);
    std::vector<HWND> children;
    for (HWND child = GetWindow(hwnd_, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        if (child != preview_) children.push_back(child);
    }
    std::reverse(children.begin(), children.end());
    int x = margin;
    int y = previewHeight + margin * 2;
    int valueX = 0;
    int valueY = 0;
    constexpr int labelWidth = 105;
    constexpr int editWidth = 72;
    for (HWND child : children) {
        if (child == widgetValue_) {
            valueX = x;
            valueY = y;
            MoveWindow(child, valueX, valueY, 155, 26, TRUE);
            continue;
        }
        if (child == widgetChoice_) {
            MoveWindow(child, valueX, valueY, 155, 200, TRUE);
            continue;
        }
        if (child == widgetCheck_) {
            MoveWindow(child, valueX, valueY, 155, 26, TRUE);
            x += 162;
            continue;
        }
        wchar_t className[32]{};
        GetClassNameW(child, className, static_cast<int>(std::size(className)));
        int controlWidth = wcscmp(className, L"STATIC") == 0 ? labelWidth : editWidth;
        if (wcscmp(className, L"BUTTON") == 0) controlWidth = 145;
        if (wcscmp(className, L"COMBOBOX") == 0) controlWidth = 155;
        if (x + controlWidth > width - margin) { x = margin; y += 34; }
        MoveWindow(child, x, y, controlWidth, 26, TRUE);
        x += controlWidth + 7;
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
        const HRESULT result = previewRenderer_->Render(
            *scene_, *grid_, previewMetrics_, true, previewScale_, previewOffset_, monitorId_);
        if (result == D2DERR_RECREATE_TARGET) InvalidateRect(preview_, nullptr, FALSE);
    }
    EndPaint(preview_, &paint);
}

WidgetInstance* WidgetStudioWindow::PrimaryWidget() noexcept {
    if (!scene_) return nullptr;
    const auto id = scene_->PrimarySelection();
    return id ? scene_->Find(*id) : nullptr;
}

void WidgetStudioWindow::UpdateControlsFromSelection() {
    WidgetInstance* widget = PrimaryWidget();
    EnableWindow(layoutMode_, widget != nullptr);
    if (!widget) {
        SendMessageW(widgetSetting_, CB_RESETCONTENT, 0, 0);
        ShowWindow(widgetValue_, SW_HIDE); ShowWindow(widgetChoice_, SW_HIDE);
        ShowWindow(widgetCheck_, SW_HIDE); ShowWindow(browse_, SW_HIDE);
        return;
    }
    SendMessageW(layoutMode_, CB_SETCURSEL, widget->layoutMode == LayoutMode::Grid ? 0 : 1, 0);
    SendMessageW(locked_, BM_SETCHECK, widget->locked ? BST_CHECKED : BST_UNCHECKED, 0);
    SetNumber(contentScale_, widget->contentScale);
    SendMessageW(appearanceMode_, CB_SETCURSEL, widget->appearance.mode == AppearanceMode::Dark ? 0 : 1, 0);
    SendMessageW(glass_, BM_SETCHECK, widget->appearance.glassEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SetNumber(opacity_, widget->appearance.opacity);
    SetNumber(blur_, widget->appearance.blurRadius);
    SetNumber(radius_, widget->appearance.cornerRadius);
    if (widget->layoutMode == LayoutMode::Grid) {
        SetNumber(positionA_, widget->grid.column); SetNumber(positionB_, widget->grid.row);
        SetNumber(sizeA_, widget->grid.columnSpan); SetNumber(sizeB_, widget->grid.rowSpan);
    } else {
        SetNumber(positionA_, widget->free.x); SetNumber(positionB_, widget->free.y);
        SetNumber(sizeA_, widget->free.width); SetNumber(sizeB_, widget->free.height);
    }
    SendMessageW(widgetSetting_, CB_RESETCONTENT, 0, 0);
    for (const auto& definition : widget->content->Settings())
        SendMessageW(widgetSetting_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(definition.displayName.c_str()));
    if (!widget->content->Settings().empty()) SendMessageW(widgetSetting_, CB_SETCURSEL, 0, 0);
    UpdateWidgetSettingValue();
}

void WidgetStudioWindow::UpdateWidgetSettingValue() {
    WidgetInstance* widget = PrimaryWidget();
    if (!widget) return;
    const LRESULT index = SendMessageW(widgetSetting_, CB_GETCURSEL, 0, 0);
    const auto settings = widget->content->Settings();
    if (index == CB_ERR || static_cast<std::size_t>(index) >= settings.size()) {
        SetWindowTextW(widgetValue_, L"");
        ShowWindow(widgetValue_, SW_HIDE); ShowWindow(widgetChoice_, SW_HIDE);
        ShowWindow(widgetCheck_, SW_HIDE); ShowWindow(browse_, SW_HIDE);
        return;
    }
    const WidgetSettingDefinition& definition = settings[static_cast<std::size_t>(index)];
    const auto state = widget->content->SaveState();
    const auto value = state.find(definition.key);
    const std::wstring current = value == state.end() ? L"" : value->second;
    const bool isChoice = definition.kind == WidgetSettingKind::Choice;
    const bool isBoolean = definition.kind == WidgetSettingKind::Boolean;
    ShowWindow(widgetValue_, !isChoice && !isBoolean ? SW_SHOW : SW_HIDE);
    ShowWindow(widgetChoice_, isChoice ? SW_SHOW : SW_HIDE);
    ShowWindow(widgetCheck_, isBoolean ? SW_SHOW : SW_HIDE);
    ShowWindow(browse_, definition.kind == WidgetSettingKind::File ? SW_SHOW : SW_HIDE);
    if (isChoice) {
        SendMessageW(widgetChoice_, CB_RESETCONTENT, 0, 0);
        int selected = 0;
        for (std::size_t choice = 0; choice < definition.choices.size(); ++choice) {
            SendMessageW(widgetChoice_, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(definition.choices[choice].c_str()));
            if (definition.choices[choice] == current) selected = static_cast<int>(choice);
        }
        SendMessageW(widgetChoice_, CB_SETCURSEL, selected, 0);
    } else if (isBoolean) {
        const bool checked = current == L"true" || current == L"1";
        SendMessageW(widgetCheck_, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    } else {
        SetWindowTextW(widgetValue_, current.c_str());
    }
}

void WidgetStudioWindow::ApplyUniversalSettings() {
    WidgetInstance* primary = PrimaryWidget();
    if (!primary) return;
    const LayoutMode mode = SendMessageW(layoutMode_, CB_GETCURSEL, 0, 0) == 1 ? LayoutMode::Free : LayoutMode::Grid;
    const bool single = scene_->SelectionCount() == 1;
    for (auto& widget : scene_->Widgets()) {
        if (!widget.selected) continue;
        scene_->SetWidgetLayoutMode(widget.instanceId, mode, *grid_, layoutMetrics_);
        widget.locked = SendMessageW(locked_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        widget.contentScale = std::clamp(static_cast<float>(ReadNumber(contentScale_, widget.contentScale)), 0.25f, 4.0f);
        widget.appearance.mode = SendMessageW(appearanceMode_, CB_GETCURSEL, 0, 0) == 1
            ? AppearanceMode::Light : AppearanceMode::Dark;
        widget.appearance.glassEnabled = SendMessageW(glass_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        widget.appearance.opacity = std::clamp(static_cast<float>(ReadNumber(opacity_, widget.appearance.opacity)), 0.0f, 1.0f);
        widget.appearance.blurRadius = std::clamp(static_cast<float>(ReadNumber(blur_, widget.appearance.blurRadius)), 0.0f, 128.0f);
        widget.appearance.cornerRadius = std::clamp(static_cast<float>(ReadNumber(radius_, widget.appearance.cornerRadius)), 0.0f, 128.0f);
        if (single && widget.layoutMode == LayoutMode::Grid) {
            widget.grid.columnSpan = std::clamp(static_cast<int>(ReadNumber(sizeA_, widget.grid.columnSpan)),
                1, grid_->Columns());
            widget.grid.rowSpan = std::clamp(static_cast<int>(ReadNumber(sizeB_, widget.grid.rowSpan)),
                1, grid_->Rows());
            widget.grid.column = std::clamp(static_cast<int>(ReadNumber(positionA_, widget.grid.column)),
                0, grid_->Columns() - widget.grid.columnSpan);
            widget.grid.row = std::clamp(static_cast<int>(ReadNumber(positionB_, widget.grid.row)),
                0, grid_->Rows() - widget.grid.rowSpan);
        } else if (single) {
            widget.free.width = std::clamp(static_cast<float>(ReadNumber(sizeA_, widget.free.width)),
                1.0f, std::max(1.0f, layoutBounds_.width));
            widget.free.height = std::clamp(static_cast<float>(ReadNumber(sizeB_, widget.free.height)),
                1.0f, std::max(1.0f, layoutBounds_.height));
            widget.free.x = std::clamp(static_cast<float>(ReadNumber(positionA_, widget.free.x)),
                layoutBounds_.x, layoutBounds_.x + layoutBounds_.width - widget.free.width);
            widget.free.y = std::clamp(static_cast<float>(ReadNumber(positionB_, widget.free.y)),
                layoutBounds_.y, layoutBounds_.y + layoutBounds_.height - widget.free.height);
        }
    }
    NotifySceneChanged();
}

void WidgetStudioWindow::ApplyAlignment() {
    const LRESULT selection = SendMessageW(alignment_, CB_GETCURSEL, 0, 0);
    if (selection != CB_ERR && scene_->AlignSelected(static_cast<AlignmentOperation>(selection), layoutBounds_))
        NotifySceneChanged();
}

void WidgetStudioWindow::ApplyWidgetSetting() {
    WidgetInstance* widget = PrimaryWidget();
    if (!widget) return;
    const LRESULT index = SendMessageW(widgetSetting_, CB_GETCURSEL, 0, 0);
    const auto settings = widget->content->Settings();
    if (index == CB_ERR || static_cast<std::size_t>(index) >= settings.size()) return;
    const WidgetSettingDefinition& definition = settings[static_cast<std::size_t>(index)];
    WidgetState state = widget->content->SaveState();
    std::wstring value;
    if (definition.kind == WidgetSettingKind::Boolean) {
        value = SendMessageW(widgetCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"true" : L"false";
    } else if (definition.kind == WidgetSettingKind::Choice) {
        const LRESULT choice = SendMessageW(widgetChoice_, CB_GETCURSEL, 0, 0);
        if (choice == CB_ERR || static_cast<std::size_t>(choice) >= definition.choices.size()) return;
        value = definition.choices[static_cast<std::size_t>(choice)];
    } else {
        value = ReadText(widgetValue_);
    }
    state[definition.key] = std::move(value);
    widget->content->RestoreState(state);
    NotifySceneChanged();
}

void WidgetStudioWindow::ChooseWidgetFile() {
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
    SetWindowTextW(widgetValue_, imported->c_str());
    ApplyWidgetSetting();
}

void WidgetStudioWindow::NotifySceneChanged() {
    if (sceneChanged_) sceneChanged_();
    UpdateControlsFromSelection();
    InvalidateRect(preview_, nullptr, FALSE);
}

LRESULT WidgetStudioWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: return CreateControls() ? 0 : -1;
    case WM_SIZE: LayoutControls(LOWORD(lParam), HIWORD(lParam)); UpdatePreviewMetrics(); return 0;
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
        switch (LOWORD(wParam)) {
        case kApplyUniversal: ApplyUniversalSettings(); return 0;
        case kApplyAlignment: ApplyAlignment(); return 0;
        case kApplyWidget: ApplyWidgetSetting(); return 0;
        case kBrowse: ChooseWidgetFile(); return 0;
        case kOpenLibrary: if (openLibrary_) openLibrary_(); return 0;
        case kDuplicateWidget: {
            const auto primary = scene_->PrimarySelection();
            if (primary && scene_->DuplicateWidget(*primary)) NotifySceneChanged();
            return 0;
        }
        case kDeleteWidget:
            if (scene_->RemoveSelectedWidgets() > 0) NotifySceneChanged();
            return 0;
        case kSettingCombo:
            if (HIWORD(wParam) == CBN_SELCHANGE) { UpdateWidgetSettingValue(); return 0; }
            break;
        default: break;
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_DELETE) {
            if (scene_->RemoveSelectedWidgets() > 0) NotifySceneChanged();
            return 0;
        }
        if (wParam == 'D' && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            const auto primary = scene_->PrimarySelection();
            if (primary && scene_->DuplicateWidget(*primary)) NotifySceneChanged();
            return 0;
        }
        break;
    case WM_CLOSE: DestroyWindow(hwnd_); return 0;
    case WM_NCDESTROY:
        previewRenderer_.reset();
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        hwnd_ = nullptr; preview_ = nullptr;
        return 0;
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
        UpdateControlsFromSelection();
        InvalidateRect(preview_, nullptr, FALSE);
        if (owner_) InvalidateRect(owner_, nullptr, FALSE);
        return 0;
    }
    default: break;
    }
    return DefWindowProcW(preview_, message, wParam, lParam);
}

} // namespace ws
