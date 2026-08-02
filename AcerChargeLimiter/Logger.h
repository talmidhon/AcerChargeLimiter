#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <windows.h>
#include <shlobj.h>

#pragma comment(lib, "shell32.lib")

enum class LogLevel {
    Info,
    Warning,
    Error
};

class Logger {
public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    void Log(LogLevel level, const std::wstring& message) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_initialized) {
            Initialize();
        }

        if (!m_logFile.is_open()) return;

        std::wstring levelStr;
        switch (level) {
        case LogLevel::Info:    levelStr = L"[INFO]"; break;
        case LogLevel::Warning: levelStr = L"[WARN]"; break;
        case LogLevel::Error:   levelStr = L"[ERR ]"; break;
        }

        SYSTEMTIME st;
        GetLocalTime(&st);

        wchar_t timeBuffer[64];
        swprintf_s(timeBuffer, L"%04d-%02d-%02d %02d:%02d:%02d.%03d",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        std::wstring fullMessage = std::wstring(timeBuffer) + L" " + levelStr + L" " + message + L"\n";

        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, fullMessage.c_str(), (int)fullMessage.size(), nullptr, 0, nullptr, nullptr);
        std::string utf8Str(sizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, fullMessage.c_str(), (int)fullMessage.size(), &utf8Str[0], sizeNeeded, nullptr, nullptr);

        m_logFile << utf8Str;
        m_logFile.flush();
    }

private:
    Logger() = default;
    ~Logger() {
        if (m_logFile.is_open()) {
            m_logFile.close();
        }
    }

    void Initialize() {
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
            std::wstring logDir = std::wstring(path) + L"\\AcerChargeLimiter";
            CreateDirectoryW(logDir.c_str(), NULL);
            std::wstring logFilePath = logDir + L"\\app.log";

            m_logFile.open(logFilePath, std::ios::out | std::ios::app);
            m_initialized = true;
        }
    }

    std::ofstream m_logFile;
    std::mutex m_mutex;
    bool m_initialized{ false };
};