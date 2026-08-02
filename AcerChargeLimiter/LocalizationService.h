#pragma once
#include <string>

struct AppStrings {
    std::wstring appTitle;
    std::wstring appSubtitle;
    std::wstring limitToggleTitle;
    std::wstring limitToggleDesc;
    std::wstring autoStartTitle;
    std::wstring autoStartDesc;
    std::wstring toggleOn;
    std::wstring toggleOff;
    std::wstring statusOptimized;
    std::wstring statusFullCharge;
    std::wstring deviceSupportedTitle;
    std::wstring deviceSupportedMsg;
    std::wstring deviceUnsupportedTitle;
    std::wstring deviceUnsupportedMsg;
    std::wstring noAdminTitle;
    std::wstring noAdminMsg;
    std::wstring developedBy;
    std::wstring githubTooltip;
    std::wstring mitmachimTooltip;
    std::wstring trayCurrentOptimized;
    std::wstring trayCurrentFull;
    std::wstring trayEnableOptimization;
    std::wstring trayDisableOptimization;
    std::wstring trayOpenWindow;
    std::wstring trayExit;
    std::wstring trayTooltipOptimized;
    std::wstring trayTooltipFull;
    std::wstring trayTooltipAdmin;
    std::wstring trayTooltipUnsupported;
    std::wstring versionPrefix;
    std::wstring updateAvailableTitle;
    std::wstring updateAvailableMsg;
    std::wstring btnUpdateNow;
    std::wstring btnDownloading;
    std::wstring updateFailedMsg;
    std::wstring updateRunningMsg;
    std::wstring reportDeviceTitle;
    std::wstring reportDeviceTooltip;
    std::wstring dontShowAgainTitle;
    std::wstring googleFormBaseUrl;
};

class LocalizationService {
public:
    static AppStrings GetStrings(bool isHebrew) {
        if (isHebrew) {
            return {
                L"מיטוב סוללה",
                L"ניהול הגדרות טעינה לשמירה על אורך חיי הסוללה במחשבי Acer",
                L"מיטוב סוללה",
                L"הגבלת הטעינה ל-80% כדי למנוע שחיקה ולשמור על בריאות הסוללה לאורך זמן",
                L"הפעלה אוטומטית עם Windows",
                L"עבודה ברקע והפעלה אוטומטית עם עליית המערכת",
                L"פעיל",
                L"מבוטל",
                L"מצב מיטוב פעיל — הסוללה תוגבל ל-80%",
                L"מצב טעינה מלאה — הסוללה תיטען עד 100%",
                L"המכשיר נתמך",
                L"ממשק WMI של Acer BatteryControl זוהה בהצלחה.",
                L"המכשיר אינו נתמך",
                L"ממשק ה-WMI עבור BatteryControl אינו זמין במחשב זה.",
                L"נדרשות הרשאות מנהל",
                L"יש להריץ את התוכנה כמנהל (Run as Administrator) כדי לשנות הגדרות WMI.",
                L"פותח על ידי talmidhon",
                L"פרופיל GitHub",
                L"פרופיל מתמחים טופ",
                L"מצב נוכחי: מיטוב פעיל (80%)",
                L"מצב נוכחי: טעינה מלאה (100%)",
                L"הפעל מיטוב סוללה (80%)",
                L"עבור לטעינה מלאה (100%)",
                L"פתח חלון ראשי",
                L"יציאה",
                L"מיטוב סוללה - 80% פעיל",
                L"מיטוב סוללה - 100% פעיל",
                L"מיטוב סוללה - נדרשות הרשאות מנהל",
                L"מיטוב סוללה - חומרה לא נתמכת",
                L"מיטוב סוללה גרסה ",
                L"עדכון גרסה זמין!",
                L"גרסה חדשה של התוכנה זמינה להורדה.",
                L"עדכן עכשיו",
                L"מוריד עדכון...",
                L"הורדת העדכון נכשלה. אנא נסה שוב מאוחר יותר.",
                L"ההתקנה רצה ברקע, התוכנה תופעל מחדש בקרוב...",
                L"דיווח תאימות",
                L"פתיחת תפריט דיווח על תאימות הדגם שלך",
                L"אל תציג הודעה זו שוב",
                L"https://docs.google.com/forms/d/e/1FAIpQLSciGvmEg3534ZYSrjAgbXgm6SNcC4MLma2QdZRSeORI4eRJWw/viewform?usp=pp_url" };
        }
        else {
            return {
                L"Battery Optimizer",
                L"Manage charge threshold settings to preserve battery health on Acer devices",
                L"Battery Optimization",
                L"Limits maximum charge to 80% to prolong long-term battery lifespan",
                L"Start with Windows",
                L"Run in background and start automatically with Windows",
                L"Enabled",
                L"Disabled",
                L"Optimization Active — Charging capped at 80%",
                L"Full Charge Mode — Charging up to 100%",
                L"Device Supported",
                L"Acer WMI BatteryControl interface detected.",
                L"Device Unsupported",
                L"Acer WMI BatteryControl interface is not available on this machine.",
                L"Administrator Privileges Required",
                L"Please run the application as Administrator to modify WMI settings.",
                L"Developed by talmidhon",
                L"GitHub Profile",
                L"Mitmachim Top Profile",
                L"Current Status: Optimized (80%)",
                L"Current Status: Full Charge (100%)",
                L"Enable Battery Optimization (80%)",
                L"Switch to Full Charge (100%)",
                L"Open Main Window",
                L"Exit",
                L"Battery Optimizer - 80% Active",
                L"Battery Optimizer - 100% Active",
                L"Battery Optimizer - Admin Required",
                L"Battery Optimizer - Unsupported Device",
                L"Battery Optimizer v",
                L"Software Update Available!",
                L"A new version of the application is available for download.",
                L"Update Now",
                L"Downloading...",
                L"Failed to download update. Please try again later.",
                L"Installation running in background, app will restart soon...",
                L"Report Compatibility",
                L"Open options to report model compatibility",
                L"Don't show this again",
                L"https://docs.google.com/forms/d/e/1FAIpQLSciGvmEg3534ZYSrjAgbXgm6SNcC4MLma2QdZRSeORI4eRJWw/viewform?usp=pp_url"
            };
        }
    }
};