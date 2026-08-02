#include "pch.h"
#include "MainWindow.xaml.h"
#include "RtlHelper.h"
#include "LocalizationService.h"
#include "Logger.h"
#include "AutoStartManager.h"
#include "resource.h"
#include <microsoft.ui.xaml.window.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Networking.Connectivity.h>
#include <commctrl.h>

#pragma comment(lib, "version.lib")
#pragma comment(lib, "comctl32.lib")

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::AcerChargeLimiter::implementation
{
    static UINT g_wmTaskbarCreated = 0;
    static UINT g_wmShowApp = 0;

    bool IsRunningAsAdmin()
    {
        BOOL isAdmin = FALSE;
        PSID adminGroup = nullptr;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup))
        {
            CheckTokenMembership(nullptr, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }
        return isAdmin != FALSE;
    }

    LRESULT CALLBACK MainWindow::SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
    {
        if (g_wmShowApp != 0 && msg == g_wmShowApp)
        {
            auto self = reinterpret_cast<MainWindow*>(dwRefData);
            if (self)
            {
                Logger::Instance().Log(LogLevel::Info, L"IPC show window request received. Restoring window.");
                self->RestoreFromTray();
            }
            return 0;
        }

        if (g_wmTaskbarCreated != 0 && msg == g_wmTaskbarCreated)
        {
            auto self = reinterpret_cast<MainWindow*>(dwRefData);
            if (self)
            {
                Logger::Instance().Log(LogLevel::Info, L"TaskbarCreated message received. Recreating tray icon.");
                self->RecreateTrayIcon();
            }
            return 0;
        }

        if (msg == WM_NCDESTROY)
        {
            RemoveWindowSubclass(hwnd, MainWindow::SubclassProc, uIdSubclass);
        }

        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

    void MainWindow::RecreateTrayIcon()
    {
        m_trayManager.RecreateIcon();
    }

    std::wstring MainWindow::GetSystemModelName()
    {
        HKEY hKey;
        wchar_t modelBuffer[256] = { 0 };
        DWORD bufferSize = sizeof(modelBuffer);

        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            RegQueryValueExW(hKey, L"SystemProductName", NULL, NULL, (LPBYTE)modelBuffer, &bufferSize);
            RegCloseKey(hKey);
        }

        std::wstring model(modelBuffer);
        return model.empty() ? L"Unknown Acer Model" : model;
    }

    bool MainWindow::IsInternetAvailable()
    {
        try
        {
            auto profile = winrt::Windows::Networking::Connectivity::NetworkInformation::GetInternetConnectionProfile();
            return profile != nullptr &&
                profile.GetNetworkConnectivityLevel() == winrt::Windows::Networking::Connectivity::NetworkConnectivityLevel::InternetAccess;
        }
        catch (...)
        {
            return false;
        }
    }

    bool MainWindow::HasDismissedReportPrompt()
    {
        HKEY hKey;
        DWORD value = 0;
        DWORD size = sizeof(value);
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\AcerChargeLimiter", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            RegQueryValueExW(hKey, L"DismissedReportPrompt", NULL, NULL, (LPBYTE)&value, &size);
            RegCloseKey(hKey);
        }
        return value == 1;
    }

    void MainWindow::MarkReportPromptDismissed()
    {
        HKEY hKey;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\AcerChargeLimiter", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
        {
            DWORD value = 1;
            RegSetValueExW(hKey, L"DismissedReportPrompt", 0, REG_DWORD, (const BYTE*)&value, sizeof(value));
            RegCloseKey(hKey);
        }
    }

    MainWindow::MainWindow()
    {
        InitializeComponent();
        Logger::Instance().Log(LogLevel::Info, L"=== Application Started ===");

        g_wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
        g_wmShowApp = RegisterWindowMessageW(L"AcerChargeLimiter_ShowAppWindow");

        auto windowNative = try_as<::IWindowNative>();

        if (windowNative && SUCCEEDED(windowNative->get_WindowHandle(&m_hwnd)) && m_hwnd != nullptr)
        {
            SetPropW(m_hwnd, L"AcerChargeLimiter_Instance", (HANDLE)1);

            ChangeWindowMessageFilterEx(m_hwnd, g_wmShowApp, MSGFLT_ALLOW, NULL);

            SetWindowSubclass(m_hwnd, MainWindow::SubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));

            HINSTANCE hInstance = GetModuleHandle(NULL);

            m_hIconBig = (HICON)LoadImageW(
                hInstance, MAKEINTRESOURCEW(IDI_ICON1), IMAGE_ICON,
                GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);

            m_hIconSmall = (HICON)LoadImageW(
                hInstance, MAKEINTRESOURCEW(IDI_ICON1), IMAGE_ICON,
                GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

            if (m_hIconBig) SendMessage(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)m_hIconBig);
            if (m_hIconSmall) SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)m_hIconSmall);

            m_trayManager.Initialize(
                m_hwnd,
                m_hIconSmall ? m_hIconSmall : m_hIconBig,
                [this]() {
                    OnBatteryLimitToggled(nullptr, nullptr);
                },
                [this]() {
                    RestoreFromTray();
                },
                [this]() {
                    m_trayManager.RemoveTrayIcon();
                    PostQuitMessage(0);
                }
            );
        }

        this->Closed([this](IInspectable const&, WindowEventArgs const& args)
            {
                if (!AutoStartManager::IsAutoStartEnabled())
                {
                    m_trayManager.RemoveTrayIcon();
                    PostQuitMessage(0);
                    return;
                }

                args.Handled(true);
                MinimizeToTray();
            });

        bool forceEnglish = false;
        bool isRtl = forceEnglish ? false : RtlHelper::IsSystemRtl();

        RtlHelper::SetWindowLayoutDirection(*this, isRtl);
        RootGrid().FlowDirection(isRtl ? FlowDirection::RightToLeft : FlowDirection::LeftToRight);

        AppStrings strings = LocalizationService::GetStrings(!forceEnglish && isRtl);

        this->Title(strings.appTitle);

        TitleTextBlock().Text(strings.appTitle);
        SubtitleTextBlock().Text(strings.appSubtitle);
        ToggleTitleTextBlock().Text(strings.limitToggleTitle);
        ToggleDescTextBlock().Text(strings.limitToggleDesc);

        AutoStartTitleTextBlock().Text(strings.autoStartTitle);
        AutoStartDescTextBlock().Text(strings.autoStartDesc);

        BatteryLimitToggle().OnContent(box_value(strings.toggleOn));
        BatteryLimitToggle().OffContent(box_value(strings.toggleOff));

        AutoStartToggle().OnContent(box_value(strings.toggleOn));
        AutoStartToggle().OffContent(box_value(strings.toggleOff));

        DevelopedByTextBlock().Text(strings.developedBy);
        ReportButtonTextBlock().Text(strings.reportDeviceTitle);
        ReportMenuItem().Text(strings.reportDeviceTitle);
        DontShowAgainMenuItem().Text(strings.dontShowAgainTitle);

        std::wstring versionStr = strings.versionPrefix;
        wchar_t exePath[MAX_PATH];
        bool versionFound = false;

        if (GetModuleFileNameW(NULL, exePath, MAX_PATH))
        {
            DWORD dummy = 0;
            DWORD size = GetFileVersionInfoSizeW(exePath, &dummy);
            if (size > 0)
            {
                std::vector<BYTE> data(size);
                if (GetFileVersionInfoW(exePath, 0, size, data.data()))
                {
                    wchar_t* prodVer = nullptr;
                    UINT len = 0;
                    if (VerQueryValueW(data.data(), L"\\StringFileInfo\\040904b0\\ProductVersion", (LPVOID*)&prodVer, &len) && len > 0 && prodVer)
                    {
                        m_currentRawVersion = prodVer;
                        versionStr += prodVer;
                        versionFound = true;
                    }
                }
            }
        }

        if (!versionFound) versionStr += L"0.0.0";
        AppVersionTextBlock().Text(versionStr);

        ToolTipService::SetToolTip(GithubButton(), box_value(strings.githubTooltip));
        ToolTipService::SetToolTip(MitmachimButton(), box_value(strings.mitmachimTooltip));
        ToolTipService::SetToolTip(ReportDropDownButton(), box_value(strings.reportDeviceTooltip));

        m_isUpdatingUi = true;
        AutoStartToggle().IsOn(AutoStartManager::IsAutoStartEnabled());
        m_isUpdatingUi = false;

        UpdateSystemStatus();
        CheckForUpdatesAsync();
    }

    MainWindow::~MainWindow()
    {
        if (m_hIconBig) DestroyIcon(m_hIconBig);
        if (m_hIconSmall) DestroyIcon(m_hIconSmall);
    }

    winrt::fire_and_forget MainWindow::CheckForUpdatesAsync()
    {
        auto lifetime = get_strong();
        co_await UpdateManager::CheckForUpdatesAsync(m_currentRawVersion, [this, lifetime](UpdateInfo update)
            {
                if (update.hasUpdate)
                {
                    m_pendingUpdate = update;
                    bool isRtl = (RootGrid().FlowDirection() == FlowDirection::RightToLeft);
                    AppStrings strings = LocalizationService::GetStrings(isRtl);

                    UpdateInfoBar().Title(strings.updateAvailableTitle);

                    if (update.isCritical) {
                        UpdateInfoBar().Severity(InfoBarSeverity::Error);
                    }
                    else {
                        UpdateInfoBar().Severity(InfoBarSeverity::Informational);
                    }

                    std::wstring msg = update.displayMessage.empty()
                        ? (strings.updateAvailableMsg + L" (v" + update.newVersion + L")")
                        : update.displayMessage;

                    UpdateInfoBar().Message(msg);
                    UpdateActionButton().Content(box_value(strings.btnUpdateNow));
                    UpdateInfoBar().IsOpen(true);
                }
            });
    }

    void MainWindow::OnUpdateNowClicked(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_pendingUpdate.downloadUrl.empty()) return;

        Logger::Instance().Log(LogLevel::Info, L"User clicked 'Update Now'. Target URL: " + m_pendingUpdate.downloadUrl);

        bool isRtl = (RootGrid().FlowDirection() == FlowDirection::RightToLeft);
        AppStrings strings = LocalizationService::GetStrings(isRtl);

        UpdateActionButton().IsEnabled(false);
        UpdateActionButton().Content(box_value(strings.btnDownloading));

        [this, downloadUrl = m_pendingUpdate.downloadUrl, strings]() -> winrt::fire_and_forget {
            auto lifetime = get_strong();
            bool success = co_await UpdateManager::DownloadAndInstallUpdateAsync(downloadUrl);

            if (!success) {
                UpdateInfoBar().Severity(InfoBarSeverity::Error);
                UpdateInfoBar().Message(strings.updateFailedMsg);
                UpdateActionButton().IsEnabled(true);
                UpdateActionButton().Content(box_value(strings.btnUpdateNow));
            }
            else {
                UpdateInfoBar().Severity(InfoBarSeverity::Success);
                UpdateInfoBar().Message(strings.updateRunningMsg);
                UpdateActionButton().Visibility(Visibility::Collapsed);

                m_trayManager.RemoveTrayIcon();
                Application::Current().Exit();
            }
            }();
    }

    void MainWindow::OnReportCompatibilityClicked(IInspectable const&, RoutedEventArgs const&)
    {
        bool isRtl = (RootGrid().FlowDirection() == FlowDirection::RightToLeft);
        AppStrings strings = LocalizationService::GetStrings(isRtl);

        std::wstring model = GetSystemModelName();
        bool isSupported = m_batteryService.IsSupported();

        std::wstring statusStr = isSupported ?
            L"Working (Battery charge cap works properly)" :
            L"Unsupported (WMI interface error / does not change charge limit)";

        winrt::hstring escapedModel = winrt::Windows::Foundation::Uri::EscapeComponent(model);
        winrt::hstring escapedStatus = winrt::Windows::Foundation::Uri::EscapeComponent(statusStr);

        std::wstring fullUrl = strings.googleFormBaseUrl;
        fullUrl += L"&entry.559081186=" + std::wstring(escapedModel.c_str());
        fullUrl += L"&entry.888550639=" + std::wstring(escapedStatus.c_str());

        Logger::Instance().Log(LogLevel::Info, L"Opening browser form for model compatibility: " + model);

        winrt::Windows::System::Launcher::LaunchUriAsync(winrt::Windows::Foundation::Uri(fullUrl));
    }

    void MainWindow::OnDontShowAgainClicked(IInspectable const&, RoutedEventArgs const&)
    {
        Logger::Instance().Log(LogLevel::Info, L"User selected 'Don't show again' for compatibility report.");
        MarkReportPromptDismissed();
        SupportInfoBar().IsOpen(false);
    }

    void MainWindow::OnSupportInfoBarClosed(InfoBar const&, IInspectable const&)
    {
        Logger::Instance().Log(LogLevel::Info, L"SupportInfoBar closed for current session.");
    }

    void MainWindow::RestoreFromTray()
    {
        if (m_hwnd)
        {
            ShowWindow(m_hwnd, SW_SHOW);
            ShowWindow(m_hwnd, SW_RESTORE);
            SetForegroundWindow(m_hwnd);
        }
        this->Activate();
    }

    void MainWindow::MinimizeToTray()
    {
        if (m_hwnd)
        {
            ShowWindow(m_hwnd, SW_HIDE);
        }
    }

    void MainWindow::UpdateSystemStatus()
    {
        m_isUpdatingUi = true;

        bool isRtl = (RootGrid().FlowDirection() == FlowDirection::RightToLeft);
        AppStrings strings = LocalizationService::GetStrings(isRtl);

        bool canShowReportPrompt = IsInternetAvailable() && !HasDismissedReportPrompt();
        ReportDropDownButton().Visibility(canShowReportPrompt ? Visibility::Visible : Visibility::Collapsed);

        if (!IsRunningAsAdmin())
        {
            Logger::Instance().Log(LogLevel::Warning, L"Application running without Administrator privileges.");
            SupportInfoBar().Title(strings.noAdminTitle);
            SupportInfoBar().Message(strings.noAdminMsg);
            SupportInfoBar().Severity(InfoBarSeverity::Warning);
            SupportInfoBar().IsOpen(true);

            BatteryLimitToggle().IsEnabled(false);
            AutoStartToggle().IsEnabled(false);

            StatusCard().Visibility(Visibility::Visible);
            StatusIcon().Glyph(L"\xE7BA");
            StatusTextBlock().Text(strings.noAdminTitle);

            m_trayManager.SetState(false, false, strings);
            m_trayManager.UpdateTooltip(strings.trayTooltipAdmin);
            m_isUpdatingUi = false;
            return;
        }
        else
        {
            AutoStartToggle().IsEnabled(true);
        }

        if (!m_batteryService.IsSupported())
        {
            Logger::Instance().Log(LogLevel::Error, L"Hardware or WMI driver unsupported on this machine.");
            SupportInfoBar().Title(strings.deviceUnsupportedTitle);
            SupportInfoBar().Message(strings.deviceUnsupportedMsg);
            SupportInfoBar().Severity(InfoBarSeverity::Error);
            SupportInfoBar().IsOpen(true);

            BatteryLimitToggle().IsEnabled(false);
            StatusCard().Visibility(Visibility::Visible);

            StatusIcon().Glyph(L"\xEC02");
            StatusTextBlock().Text(strings.deviceUnsupportedTitle);

            m_trayManager.SetState(false, false, strings);
            m_trayManager.UpdateTooltip(strings.trayTooltipUnsupported);
        }
        else
        {
            SupportInfoBar().Title(strings.deviceSupportedTitle);
            SupportInfoBar().Message(strings.deviceSupportedMsg);
            SupportInfoBar().Severity(InfoBarSeverity::Success);
            SupportInfoBar().IsOpen(true);

            BatteryLimitState status = m_batteryService.GetCurrentStatus();
            BatteryLimitToggle().IsEnabled(true);
            StatusCard().Visibility(Visibility::Visible);

            bool is80 = (status == BatteryLimitState::Limit80);
            m_trayManager.SetState(is80, true, strings);

            if (is80)
            {
                BatteryLimitToggle().IsOn(true);
                StatusTextBlock().Text(strings.statusOptimized);

                StatusIcon().Glyph(L"\xEBBE");
                m_trayManager.UpdateTooltip(strings.trayTooltipOptimized);
            }
            else if (status == BatteryLimitState::Limit100)
            {
                BatteryLimitToggle().IsOn(false);
                StatusTextBlock().Text(strings.statusFullCharge);

                StatusIcon().Glyph(L"\xEBAA");
                m_trayManager.UpdateTooltip(strings.trayTooltipFull);
            }
            else
            {
                StatusIcon().Glyph(L"\xEC02");
            }
        }

        m_isUpdatingUi = false;
    }

    void MainWindow::OnBatteryLimitToggled(IInspectable const& sender, RoutedEventArgs const&)
    {
        if (m_isUpdatingUi) return;

        bool enable80 = !BatteryLimitToggle().IsOn();
        if (sender != nullptr) {
            enable80 = BatteryLimitToggle().IsOn();
        }

        Logger::Instance().Log(LogLevel::Info, L"Toggling optimization switch to: " + std::wstring(enable80 ? L"ON" : L"OFF"));

        bool success = m_batteryService.SetBatteryLimit(enable80);

        if (success)
        {
            UpdateSystemStatus();
        }
        else
        {
            Logger::Instance().Log(LogLevel::Error, L"Failed to update battery limit state.");
            m_isUpdatingUi = true;
            BatteryLimitToggle().IsOn(!enable80);
            m_isUpdatingUi = false;
        }
    }

    void MainWindow::OnAutoStartToggled(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_isUpdatingUi) return;

        bool enableAutoStart = AutoStartToggle().IsOn();
        Logger::Instance().Log(LogLevel::Info, L"AutoStart toggle changed to: " + std::wstring(enableAutoStart ? L"ENABLED" : L"DISABLED"));

        bool success = AutoStartManager::SetAutoStart(enableAutoStart);

        if (!success)
        {
            Logger::Instance().Log(LogLevel::Error, L"Failed to update Registry for AutoStart.");
            m_isUpdatingUi = true;
            AutoStartToggle().IsOn(!enableAutoStart);
            m_isUpdatingUi = false;
        }
    }

    void MainWindow::OnGithubClicked(IInspectable const&, RoutedEventArgs const&)
    {
        Logger::Instance().Log(LogLevel::Info, L"GitHub link clicked.");
        winrt::Windows::System::Launcher::LaunchUriAsync(winrt::Windows::Foundation::Uri(L"https://github.com/talmidhon"));
    }

    void MainWindow::OnMitmachimClicked([[maybe_unused]] IInspectable const& sender, [[maybe_unused]] RoutedEventArgs const& e)
    {
        Logger::Instance().Log(LogLevel::Info, L"Mitmachim Top link clicked.");
        winrt::Windows::System::Launcher::LaunchUriAsync(winrt::Windows::Foundation::Uri(L"https://mitmachim.top/user/%D7%AA%D7%9C%D7%9E%D7%99%D7%93%D7%94%D7%95%D7%9F"));
    }
}