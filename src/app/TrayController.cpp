#include "app/TrayController.h"

#include <iterator>
#include <shellapi.h>

namespace ws {

TrayController::~TrayController() {
    Shutdown();
}

bool TrayController::Initialize(HWND owner) {
    owner_ = owner;
    data_ = {};
    data_.cbSize = sizeof(data_);
    data_.hWnd = owner_;
    data_.uID = 1;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data_.uCallbackMessage = kTrayCallbackMessage;
    data_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpynW(data_.szTip, L"Widget Studio", static_cast<int>(std::size(data_.szTip)));

    initialized_ = Shell_NotifyIconW(NIM_ADD, &data_) == TRUE;
    if (initialized_) {
        data_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data_);
    }
    return initialized_;
}

void TrayController::RestoreAfterExplorerRestart() {
    if (!initialized_) return;
    if (Shell_NotifyIconW(NIM_ADD, &data_) == TRUE) {
        data_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data_);
    }
}

void TrayController::Shutdown() noexcept {
    if (initialized_) {
        Shell_NotifyIconW(NIM_DELETE, &data_);
        initialized_ = false;
    }
}

void TrayController::ShowContextMenu(bool editMode, bool launchAtLogin) {
    if (!owner_) return;

    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenuW(menu, MF_STRING, kCommandToggleEdit, editMode ? L"Finish editing" : L"Edit desktop");
    AppendMenuW(menu, MF_STRING, kCommandAddWidget, L"Add Widget...");
    AppendMenuW(menu, MF_STRING, kCommandOpenStudio, L"Open Widget Studio...");
    AppendMenuW(menu, MF_STRING, kCommandLockAll, L"Lock All");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (launchAtLogin ? MF_CHECKED : MF_UNCHECKED),
        kCommandToggleLaunchAtLogin, L"Launch at login");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCommandExit, L"Exit");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(owner_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, cursor.x, cursor.y, 0, owner_, nullptr);
    DestroyMenu(menu);
}

} // namespace ws
