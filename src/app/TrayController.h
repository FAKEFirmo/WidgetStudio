#pragma once

#include <windows.h>
#include <shellapi.h>

namespace ws {

class TrayController {
public:
    static constexpr UINT kTrayCallbackMessage = WM_APP + 10;
    static constexpr UINT kCommandToggleEdit = 40001;
    static constexpr UINT kCommandAddWidget = 40002;
    static constexpr UINT kCommandOpenStudio = 40003;
    static constexpr UINT kCommandExit = 40004;

    TrayController() = default;
    ~TrayController();

    bool Initialize(HWND owner);
    void RestoreAfterExplorerRestart();
    void Shutdown() noexcept;
    void ShowContextMenu(bool editMode);

private:
    HWND owner_{};
    NOTIFYICONDATAW data_{};
    bool initialized_{false};
};

} // namespace ws
