#pragma once
#include <windows.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <microsoft.ui.xaml.window.h>

class RtlHelper
{
public:
    static bool IsSystemRtl()
    {
        WORD langId = PRIMARYLANGID(GetUserDefaultUILanguage());
        return (langId == LANG_HEBREW || langId == LANG_ARABIC);
    }

    static void SetWindowLayoutDirection(winrt::Microsoft::UI::Xaml::Window const& window, bool isRtl)
    {
        auto windowNative = window.as<IWindowNative>();
        HWND hwnd = nullptr;

        if (SUCCEEDED(windowNative->get_WindowHandle(&hwnd)) && hwnd != nullptr)
        {
            LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
            LONG_PTR newStyle = isRtl ? (exStyle | WS_EX_LAYOUTRTL) : (exStyle & ~WS_EX_LAYOUTRTL);

            if (exStyle != newStyle)
            {
                SetWindowLongPtr(hwnd, GWL_EXSTYLE, newStyle);
            }
        }
    }
};