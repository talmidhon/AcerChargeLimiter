#pragma once
#include "pch.h"
#include "MainWindow.g.h"
#include "AcerBatteryService.h"
#include "TrayManager.h"
#include "UpdateManager.h"

namespace winrt::AcerChargeLimiter::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        ~MainWindow();

        void OnBatteryLimitToggled(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnAutoStartToggled(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnGithubClicked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnMitmachimClicked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnUpdateNowClicked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnReportCompatibilityClicked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnDontShowAgainClicked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnSupportInfoBarClosed(winrt::Microsoft::UI::Xaml::Controls::InfoBar const& sender, winrt::Windows::Foundation::IInspectable const& args);

        void RecreateTrayIcon();
        void RestoreFromTray();

    private:
        void UpdateSystemStatus();
        void MinimizeToTray();
        winrt::fire_and_forget CheckForUpdatesAsync();

        std::wstring GetSystemModelName();
        bool IsInternetAvailable();
        bool HasDismissedReportPrompt();
        void MarkReportPromptDismissed();

        static LRESULT CALLBACK SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

        AcerBatteryService m_batteryService;
        TrayManager m_trayManager;
        HWND m_hwnd{ nullptr };
        HICON m_hIconBig{ nullptr };
        HICON m_hIconSmall{ nullptr };
        bool m_isUpdatingUi{ false };

        std::wstring m_currentRawVersion{ L"0.0.0" };
        UpdateInfo m_pendingUpdate;
    };
}

namespace winrt::AcerChargeLimiter::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}