#pragma once
#include <windows.h>
#include <winnls.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <sstream>
#include <functional>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include "Logger.h"
#include <winrt/Windows.Web.Http.Filters.h>

struct UpdateInfo {
    bool hasUpdate{ false };
    bool isCritical{ false };
    std::wstring newVersion;
    std::wstring downloadUrl;
    std::wstring displayMessage;
};

class UpdateManager {
public:
    static winrt::Windows::Foundation::IAsyncAction CheckForUpdatesAsync(std::wstring currentVersionStr, std::function<void(UpdateInfo)> callback) {
        UpdateInfo info;
        try {
            winrt::Windows::Web::Http::HttpClient client;
            client.DefaultRequestHeaders().UserAgent().ParseAdd(L"AcerChargeLimiter-Updater");
            client.DefaultRequestHeaders().Append(L"Cache-Control", L"no-cache");

            std::wstring url = L"https://raw.githubusercontent.com/talmidhon/AcerChargeLimiter/main/version.json?t=" + std::to_wstring(GetTickCount64());
            winrt::Windows::Foundation::Uri requestUri(url);

            winrt::Windows::Web::Http::HttpResponseMessage response = co_await client.GetAsync(requestUri);

            if (response.IsSuccessStatusCode()) {
                winrt::hstring jsonString = co_await response.Content().ReadAsStringAsync();
                winrt::Windows::Data::Json::JsonObject jsonObject = winrt::Windows::Data::Json::JsonObject::Parse(jsonString);

                winrt::hstring latestVersionStr = jsonObject.GetNamedString(L"latest_version", L"0.0.0");
                std::wstring latestVersion = latestVersionStr.c_str();

                if (!latestVersion.empty() && (latestVersion[0] == L'v' || latestVersion[0] == L'V')) {
                    latestVersion = latestVersion.substr(1);
                }

                if (IsVersionNewer(latestVersion, currentVersionStr)) {
                    info.hasUpdate = true;
                    info.newVersion = latestVersion;
                    info.downloadUrl = jsonObject.GetNamedString(L"download_url", L"").c_str();
                    std::wstring sysLang = GetSystemLanguageCode();

                    bool ruleMatched = false;

                    if (jsonObject.HasKey(L"rules") && jsonObject.GetNamedValue(L"rules").ValueType() == winrt::Windows::Data::Json::JsonValueType::Array) {
                        auto rulesArray = jsonObject.GetNamedArray(L"rules");
                        for (uint32_t i = 0; i < rulesArray.Size(); ++i) {
                            auto rule = rulesArray.GetObjectAt(i);
                            std::wstring minVer = rule.HasKey(L"min_version") ? rule.GetNamedString(L"min_version").c_str() : L"";
                            std::wstring maxVer = rule.HasKey(L"max_version") ? rule.GetNamedString(L"max_version").c_str() : L"";

                            if (IsVersionInRange(currentVersionStr, minVer, maxVer)) {
                                ruleMatched = true;
                                if (rule.HasKey(L"is_critical")) {
                                    info.isCritical = rule.GetNamedBoolean(L"is_critical");
                                }

                                if (rule.HasKey(L"download_url")) {
                                    info.downloadUrl = rule.GetNamedString(L"download_url").c_str();
                                }

                                if (rule.HasKey(L"messages") && rule.GetNamedValue(L"messages").ValueType() == winrt::Windows::Data::Json::JsonValueType::Object) {
                                    auto msgObj = rule.GetNamedObject(L"messages");
                                    if (msgObj.HasKey(sysLang.c_str())) {
                                        info.displayMessage = msgObj.GetNamedString(sysLang.c_str()).c_str();
                                    }
                                    else if (msgObj.HasKey(L"en")) {
                                        info.displayMessage = msgObj.GetNamedString(L"en").c_str();
                                    }
                                }
                                break;
                            }
                        }
                    }

                    if (!ruleMatched && jsonObject.HasKey(L"messages") && jsonObject.GetNamedValue(L"messages").ValueType() == winrt::Windows::Data::Json::JsonValueType::Object) {
                        auto messagesObj = jsonObject.GetNamedObject(L"messages");
                        if (messagesObj.HasKey(sysLang.c_str())) {
                            info.displayMessage = messagesObj.GetNamedString(sysLang.c_str()).c_str();
                        }
                        else if (messagesObj.HasKey(L"en")) {
                            info.displayMessage = messagesObj.GetNamedString(L"en").c_str();
                        }
                    }

                    // החלפה דינמית אך ורק עבור המשתנה {latest_version}
                    if (!info.downloadUrl.empty()) {
                        info.downloadUrl = ReplaceAll(info.downloadUrl, L"{latest_version}", latestVersion);
                    }

                    if (!info.displayMessage.empty()) {
                        info.displayMessage = ReplaceAll(info.displayMessage, L"{latest_version}", latestVersion);
                    }
                }
            }
            else {
                std::wstring statusErr = L"Update check failed. HTTP Status: " + std::to_wstring((int)response.StatusCode());
                Logger::Instance().Log(LogLevel::Warning, statusErr);
            }
        }
        catch (const winrt::hresult_error& ex) {
            Logger::Instance().Log(LogLevel::Error, L"Exception during update check: " + std::wstring(ex.message().c_str()));
        }
        catch (...) {
            Logger::Instance().Log(LogLevel::Error, L"Unknown exception during update check.");
        }

        if (callback) {
            callback(info);
        }
    }

    static winrt::Windows::Foundation::IAsyncOperation<bool> DownloadAndInstallUpdateAsync(std::wstring downloadUrl) {
        Logger::Instance().Log(LogLevel::Info, L"Starting update download from URL: " + downloadUrl);

        try {
            winrt::Windows::Web::Http::Filters::HttpBaseProtocolFilter filter;
            filter.CacheControl().ReadBehavior(winrt::Windows::Web::Http::Filters::HttpCacheReadBehavior::MostRecent);
            filter.CacheControl().WriteBehavior(winrt::Windows::Web::Http::Filters::HttpCacheWriteBehavior::NoCache);

            winrt::Windows::Web::Http::HttpClient client(filter);
            client.DefaultRequestHeaders().UserAgent().ParseAdd(L"AcerChargeLimiter-Updater");

            winrt::Windows::Foundation::Uri downloadUri(downloadUrl);
            winrt::Windows::Web::Http::HttpResponseMessage response = co_await client.GetAsync(downloadUri);

            if (!response.IsSuccessStatusCode()) {
                std::wstring err = L"Failed to download update file. HTTP Status: " + std::to_wstring((int)response.StatusCode());
                Logger::Instance().Log(LogLevel::Error, err);
                co_return false;
            }

            wchar_t tempPath[MAX_PATH];
            GetTempPathW(MAX_PATH, tempPath);

            auto folder = co_await winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(tempPath);
            auto targetFile = co_await folder.CreateFileAsync(L"AcerChargeLimiter_Update_Setup.exe", winrt::Windows::Storage::CreationCollisionOption::ReplaceExisting);

            auto inputStream = co_await response.Content().ReadAsInputStreamAsync();
            auto outputStream = co_await targetFile.OpenAsync(winrt::Windows::Storage::FileAccessMode::ReadWrite);

            co_await winrt::Windows::Storage::Streams::RandomAccessStream::CopyAsync(inputStream, outputStream);
            outputStream.Close();

            std::wstring filePath = targetFile.Path().c_str();
            Logger::Instance().Log(LogLevel::Info, L"Update setup downloaded successfully to: " + filePath);

            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.cbSize = sizeof(sei);
            sei.lpVerb = L"runas";
            sei.lpFile = filePath.c_str();
            sei.lpParameters = L"/SILENT /NORESTART";
            sei.nShow = SW_NORMAL;

            if (ShellExecuteExW(&sei)) {
                Logger::Instance().Log(LogLevel::Info, L"Update installer launched successfully. Handing over control to the installer.");
                co_return true;
            }
            else {
                DWORD errCode = GetLastError();
                Logger::Instance().Log(LogLevel::Error, L"ShellExecuteExW failed to launch installer. Error code: " + std::to_wstring(errCode));
            }
        }
        catch (const winrt::hresult_error& ex) {
            Logger::Instance().Log(LogLevel::Error, L"Failed to download or run update installer: " + std::wstring(ex.message().c_str()));
        }
        catch (...) {
            Logger::Instance().Log(LogLevel::Error, L"Unknown error during update download.");
        }

        co_return false;
    }

private:
    static std::wstring ReplaceAll(std::wstring str, const std::wstring& from, const std::wstring& to) {
        if (from.empty()) return str;
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::wstring::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
        return str;
    }

    static std::wstring GetSystemLanguageCode() {
        wchar_t isoLang[9] = { 0 };
        if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SISO639LANGNAME, isoLang, 9) > 0) {
            return std::wstring(isoLang);
        }
        return L"en";
    }

    static std::vector<int> ParseVersion(const std::wstring& ver) {
        std::vector<int> parts;
        std::wstringstream ss(ver);
        std::wstring item;
        while (std::getline(ss, item, L'.')) {
            try { parts.push_back(std::stoi(item)); }
            catch (...) { parts.push_back(0); }
        }
        while (parts.size() < 4) parts.push_back(0);
        return parts;
    }

    static int CompareVersions(const std::wstring& v1, const std::wstring& v2) {
        auto p1 = ParseVersion(v1);
        auto p2 = ParseVersion(v2);
        for (size_t i = 0; i < 4; ++i) {
            if (p1[i] > p2[i]) return 1;
            if (p1[i] < p2[i]) return -1;
        }
        return 0;
    }

    static bool IsVersionInRange(const std::wstring& localVer, const std::wstring& minVer, const std::wstring& maxVer) {
        if (!minVer.empty() && CompareVersions(localVer, minVer) < 0) return false;
        if (!maxVer.empty() && CompareVersions(localVer, maxVer) > 0) return false;
        return true;
    }

    static bool IsVersionNewer(const std::wstring& remoteVersion, const std::wstring& localVersion) {
        return CompareVersions(remoteVersion, localVersion) > 0;
    }
};