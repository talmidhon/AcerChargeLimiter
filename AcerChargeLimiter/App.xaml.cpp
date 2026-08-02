#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "LocalizationService.h"
#include "Logger.h"

#if __has_include("App.g.cpp")
#include "App.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::AcerChargeLimiter::implementation
{
    static HANDLE g_hSingleInstanceMutex = NULL;

    struct EnumWindowData {
        HWND hwnd{ nullptr };
    };

    static BOOL CALLBACK EnumInstanceWindowsProc(HWND hwnd, LPARAM lParam) {
        if (GetPropW(hwnd, L"AcerChargeLimiter_Instance") == (HANDLE)1) {
            auto pData = reinterpret_cast<EnumWindowData*>(lParam);
            pData->hwnd = hwnd;
            return FALSE;
        }
        return TRUE;
    }

    static HWND FindExistingInstanceWindow() {
        EnumWindowData data;
        EnumWindows(EnumInstanceWindowsProc, reinterpret_cast<LPARAM>(&data));
        return data.hwnd;
    }

    bool IsProcessRunningAsAdmin()
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

    void SelfElevateAndExit()
    {
        wchar_t szPath[MAX_PATH];
        if (GetModuleFileNameW(NULL, szPath, MAX_PATH))
        {
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.cbSize = sizeof(sei);
            sei.lpVerb = L"runas";
            sei.lpFile = szPath;
            sei.hwnd = NULL;
            sei.nShow = SW_NORMAL;

            if (ShellExecuteExW(&sei))
            {
                ExitProcess(0);
            }
        }
    }

    App::App()
    {
#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
            {
                if (IsDebuggerPresent())
                {
                    auto errorMessage = e.Message();
                    __debugbreak();
                }
            });
#endif
    }

    void App::OnLaunched([[maybe_unused]] LaunchActivatedEventArgs const& e)
    {
        SECURITY_DESCRIPTOR sd;
        InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = FALSE;
        sa.lpSecurityDescriptor = &sd;

        g_hSingleInstanceMutex = CreateMutexW(&sa, FALSE, L"Local\\AcerChargeLimiter_SingleInstance_Mutex");
        DWORD lastErr = GetLastError();

        if (lastErr == ERROR_ALREADY_EXISTS || lastErr == ERROR_ACCESS_DENIED)
        {
            Logger::Instance().Log(LogLevel::Info, L"An instance is already running. Requesting existing instance to restore window...");

            UINT wmShowApp = RegisterWindowMessageW(L"AcerChargeLimiter_ShowAppWindow");

            HWND existingHwnd = FindExistingInstanceWindow();

            if (existingHwnd)
            {
                DWORD processId = 0;
                GetWindowThreadProcessId(existingHwnd, &processId);
                if (processId != 0)
                {
                    AllowSetForegroundWindow(processId);
                }

                SendNotifyMessageW(existingHwnd, wmShowApp, 0, 0);
            }

            Exit();
            return;
        }

        if (!IsProcessRunningAsAdmin())
        {
            Logger::Instance().Log(LogLevel::Info, L"Not running as admin. Triggering Self-Elevation...");
            SelfElevateAndExit();
            return;
        }

        window = make<MainWindow>();

        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        bool startMinimized = false;

        if (argv) {
            for (int i = 1; i < argc; ++i) {
                if (wcscmp(argv[i], L"--minimized") == 0) {
                    startMinimized = true;
                    break;
                }
            }
            LocalFree(argv);
        }

        if (!startMinimized) {
            window.Activate();
        }
    }
}