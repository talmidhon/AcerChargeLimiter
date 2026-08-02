#pragma once
#include <windows.h>
#include <shellapi.h>
#include <functional>
#include <string>
#include "LocalizationService.h"

#define WM_TRAYICON (WM_USER + 1)
#define IDM_TRAY_STATUS 1000
#define IDM_TRAY_TOGGLE 1001
#define IDM_TRAY_OPEN   1002
#define IDM_TRAY_EXIT   1003

class TrayManager
{
public:
    using ToggleCallback = std::function<void()>;
    using OpenCallback = std::function<void()>;
    using ExitCallback = std::function<void()>;

    TrayManager() = default;
    ~TrayManager() { RemoveTrayIcon(); }

    void Initialize(HWND hwnd, HICON hIcon, ToggleCallback onToggle, OpenCallback onOpen, ExitCallback onExit)
    {
        m_hwnd = hwnd;
        m_hIcon = hIcon;
        m_onToggle = onToggle;
        m_onOpen = onOpen;
        m_onExit = onExit;

        WNDCLASSW wc = { 0 };
        wc.lpfnWndProc = TrayWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = L"AcerTrayMsgWindow";
        RegisterClassW(&wc);

        if (!m_msgHwnd)
        {
            m_msgHwnd = CreateWindowExW(0, L"AcerTrayMsgWindow", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
            SetWindowLongPtrW(m_msgHwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        }

        AddTrayIcon();
    }

    void SetState(bool isOptimized, bool isEnabled, const AppStrings& strings)
    {
        m_isOptimized = isOptimized;
        m_isEnabled = isEnabled;
        m_strings = strings;
    }

    void UpdateTooltip(const std::wstring& text)
    {
        m_currentTooltip = text;

        if (!m_iconAdded)
        {
            AddTrayIcon();
            return;
        }

        NOTIFYICONDATAW nid = { sizeof(nid) };
        nid.hWnd = m_msgHwnd;
        nid.uID = 1;
        nid.uFlags = NIF_TIP;
        wcsncpy_s(nid.szTip, text.c_str(), _TRUNCATE);

        if (!Shell_NotifyIconW(NIM_MODIFY, &nid))
        {
            AddTrayIcon();
        }
    }

    bool AddTrayIcon(int maxRetries = 10, DWORD retryDelayMs = 500)
    {
        if (!m_msgHwnd) return false;

        RemoveTrayIconOnly();

        NOTIFYICONDATAW nid = { sizeof(nid) };
        nid.hWnd = m_msgHwnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = m_hIcon;

        std::wstring tip = m_currentTooltip.empty() ? L"Acer Battery Optimizer" : m_currentTooltip;
        wcsncpy_s(nid.szTip, tip.c_str(), _TRUNCATE);

        for (int i = 0; i < maxRetries; ++i)
        {
            if (Shell_NotifyIconW(NIM_ADD, &nid))
            {
                m_iconAdded = true;
                return true;
            }
            if (i < maxRetries - 1)
            {
                Sleep(retryDelayMs);
            }
        }
        return false;
    }

    bool RecreateIcon()
    {
        return AddTrayIcon(10, 500);
    }

    void RemoveTrayIconOnly()
    {
        if (m_iconAdded && m_msgHwnd)
        {
            NOTIFYICONDATAW nid = { sizeof(nid) };
            nid.hWnd = m_msgHwnd;
            nid.uID = 1;
            Shell_NotifyIconW(NIM_DELETE, &nid);
            m_iconAdded = false;
        }
    }

    void RemoveTrayIcon()
    {
        RemoveTrayIconOnly();
        if (m_msgHwnd)
        {
            DestroyWindow(m_msgHwnd);
            m_msgHwnd = nullptr;
        }
    }

private:
    void ShowContextMenu()
    {
        POINT pt;
        GetCursorPos(&pt);

        HMENU hMenu = CreatePopupMenu();

        if (m_isEnabled)
        {
            std::wstring statusText = m_isOptimized ? m_strings.trayCurrentOptimized : m_strings.trayCurrentFull;
            AppendMenuW(hMenu, MF_STRING | MF_GRAYED, IDM_TRAY_STATUS, statusText.c_str());

            std::wstring actionText = m_isOptimized ? m_strings.trayDisableOptimization : m_strings.trayEnableOptimization;
            AppendMenuW(hMenu, MF_STRING, IDM_TRAY_TOGGLE, actionText.c_str());

            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        }

        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, m_strings.trayOpenWindow.c_str());
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, m_strings.trayExit.c_str());

        SetForegroundWindow(m_msgHwnd);
        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_msgHwnd, NULL);
        PostMessageW(m_msgHwnd, WM_NULL, 0, 0);
        DestroyMenu(hMenu);

        if (cmd == IDM_TRAY_TOGGLE && m_onToggle) m_onToggle();
        else if (cmd == IDM_TRAY_OPEN && m_onOpen) m_onOpen();
        else if (cmd == IDM_TRAY_EXIT && m_onExit) m_onExit();
    }

    static LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        auto tray = reinterpret_cast<TrayManager*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        if (msg == WM_TRAYICON)
        {
            if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU)
            {
                if (tray) tray->ShowContextMenu();
            }
            else if (lParam == WM_LBUTTONDBLCLK)
            {
                if (tray && tray->m_onOpen) tray->m_onOpen();
            }
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    HWND m_hwnd{ nullptr };
    HWND m_msgHwnd{ nullptr };
    HICON m_hIcon{ nullptr };
    bool m_iconAdded{ false };
    bool m_isOptimized{ false };
    bool m_isEnabled{ false };
    std::wstring m_currentTooltip;

    AppStrings m_strings;

    ToggleCallback m_onToggle;
    OpenCallback m_onOpen;
    ExitCallback m_onExit;
};