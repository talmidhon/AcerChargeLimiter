#pragma once
#include <windows.h>
#include <string>
#include <taskschd.h>
#include <comdef.h>
#include <wil/com.h>
#include "Logger.h"

#pragma comment(lib, "taskschd.lib")

class AutoStartManager {
public:
    static bool SetAutoStart(bool enable) {
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        bool uninit = (hr == S_OK || hr == S_FALSE);

        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            Logger::Instance().Log(LogLevel::Error, L"Failed to initialize COM for AutoStartManager.");
            return false;
        }

        wil::com_ptr<ITaskService> pService;
        hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, pService.put_void());
        if (FAILED(hr)) {
            Logger::Instance().Log(LogLevel::Error, L"Failed to create ITaskService instance.");
            if (uninit) CoUninitialize();
            return false;
        }

        hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
        if (FAILED(hr)) {
            Logger::Instance().Log(LogLevel::Error, L"Failed to connect to Task Scheduler Service.");
            if (uninit) CoUninitialize();
            return false;
        }

        wil::com_ptr<ITaskFolder> pRootFolder;
        hr = pService->GetFolder(_bstr_t(L"\\"), pRootFolder.put());
        if (FAILED(hr) || !pRootFolder) {
            Logger::Instance().Log(LogLevel::Error, L"Failed to open root folder in Task Scheduler.");
            if (uninit) CoUninitialize();
            return false;
        }

        bool success = false;
        wil::com_ptr<IRegisteredTask> pTask;
        hr = pRootFolder->GetTask(_bstr_t(L"AcerChargeLimiter"), pTask.put());

        if (SUCCEEDED(hr) && pTask) {
            hr = pTask->put_Enabled(enable ? VARIANT_TRUE : VARIANT_FALSE);
            success = SUCCEEDED(hr);
        }
        else if (enable) {
            wil::com_ptr<ITaskDefinition> pTaskDef;
            hr = pService->NewTask(0, pTaskDef.put());
            if (SUCCEEDED(hr) && pTaskDef) {
                wil::com_ptr<IPrincipal> pPrincipal;
                if (SUCCEEDED(pTaskDef->get_Principal(pPrincipal.put())) && pPrincipal) {
                    pPrincipal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);
                }

                wil::com_ptr<ITriggerCollection> pTriggers;
                if (SUCCEEDED(pTaskDef->get_Triggers(pTriggers.put())) && pTriggers) {
                    wil::com_ptr<ITrigger> pTrigger;
                    pTriggers->Create(TASK_TRIGGER_LOGON, pTrigger.put());
                }

                wil::com_ptr<IActionCollection> pActions;
                if (SUCCEEDED(pTaskDef->get_Actions(pActions.put())) && pActions) {
                    wil::com_ptr<IAction> pAction;
                    if (SUCCEEDED(pActions->Create(TASK_ACTION_EXEC, pAction.put())) && pAction) {
                        wil::com_ptr<IExecAction> pExecAction = pAction.query<IExecAction>();
                        if (pExecAction) {
                            wchar_t szPath[MAX_PATH];
                            GetModuleFileNameW(NULL, szPath, MAX_PATH);
                            pExecAction->put_Path(_bstr_t(szPath));
                            pExecAction->put_Arguments(_bstr_t(L"--minimized"));
                        }
                    }
                }

                wil::com_ptr<ITaskSettings> pSettings;
                if (SUCCEEDED(pTaskDef->get_Settings(pSettings.put())) && pSettings) {
                    pSettings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
                    pSettings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
                    pSettings->put_ExecutionTimeLimit(_bstr_t(L"PT0S"));
                }

                wil::com_ptr<IRegisteredTask> pRegisteredTask;
                hr = pRootFolder->RegisterTaskDefinition(
                    _bstr_t(L"AcerChargeLimiter"),
                    pTaskDef.get(),
                    TASK_CREATE_OR_UPDATE,
                    _variant_t(),
                    _variant_t(),
                    TASK_LOGON_INTERACTIVE_TOKEN,
                    _variant_t(L""),
                    pRegisteredTask.put()
                );

                if (SUCCEEDED(hr)) {
                    success = true;
                }
            }
        }
        else {
            success = true;
        }

        if (uninit) CoUninitialize();
        return success;
    }

    static bool IsAutoStartEnabled() {
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        bool uninit = (hr == S_OK || hr == S_FALSE);

        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

        wil::com_ptr<ITaskService> pService;
        hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, pService.put_void());
        if (FAILED(hr)) {
            if (uninit) CoUninitialize();
            return false;
        }

        hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
        if (FAILED(hr)) {
            if (uninit) CoUninitialize();
            return false;
        }

        wil::com_ptr<ITaskFolder> pRootFolder;
        hr = pService->GetFolder(_bstr_t(L"\\"), pRootFolder.put());
        bool enabled = false;
        if (SUCCEEDED(hr) && pRootFolder) {
            wil::com_ptr<IRegisteredTask> pTask;
            hr = pRootFolder->GetTask(_bstr_t(L"AcerChargeLimiter"), pTask.put());
            if (SUCCEEDED(hr) && pTask) {
                VARIANT_BOOL isEnabled = VARIANT_FALSE;
                if (SUCCEEDED(pTask->get_Enabled(&isEnabled))) {
                    enabled = (isEnabled == VARIANT_TRUE);
                }
            }
        }

        if (uninit) CoUninitialize();
        return enabled;
    }
};