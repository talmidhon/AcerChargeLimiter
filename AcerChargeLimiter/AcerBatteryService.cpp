#include "pch.h"
#include "AcerBatteryService.h"
#include "Logger.h"
#include <wil/com.h>
#pragma comment(lib, "wbemuuid.lib")

AcerBatteryService::AcerBatteryService() {
    m_comInitialized = InitializeCom();
    if (m_comInitialized) {
        Logger::Instance().Log(LogLevel::Info, L"COM initialized successfully by AcerBatteryService.");
    }
    else {
        Logger::Instance().Log(LogLevel::Info, L"COM managed externally or already active on thread.");
    }
}

AcerBatteryService::~AcerBatteryService() {
    if (m_comInitialized) {
        CoUninitialize();
    }
}

bool AcerBatteryService::InitializeCom() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    return (hr == S_OK || hr == S_FALSE);
}

bool AcerBatteryService::ConnectWmi(IWbemServices** ppSvc) {
    if (!ppSvc) return false;
    *ppSvc = nullptr;

    wil::com_ptr<IWbemLocator> pLoc;
    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, pLoc.put_void());
    if (FAILED(hr)) {
        Logger::Instance().Log(LogLevel::Error, L"CoCreateInstance for CLSID_WbemLocator failed: " + std::to_wstring(hr));
        return false;
    }

    wil::com_ptr<IWbemServices> pSvc;
    hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, 0, 0, 0, 0, pSvc.put());

    if (FAILED(hr)) {
        Logger::Instance().Log(LogLevel::Error, L"Failed to connect to WMI ROOT\\WMI namespace: " + std::to_wstring(hr));
        return false;
    }

    hr = CoSetProxyBlanket(pSvc.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

    if (FAILED(hr)) {
        Logger::Instance().Log(LogLevel::Error, L"CoSetProxyBlanket failed: " + std::to_wstring(hr));
        return false;
    }

    *ppSvc = pSvc.detach();
    return true;
}

SAFEARRAY* AcerBatteryService::CreateByteArray(const std::vector<BYTE>& bytes) {
    SAFEARRAYBOUND bound;
    bound.cElements = static_cast<ULONG>(bytes.size());
    bound.lLbound = 0;

    SAFEARRAY* psa = SafeArrayCreate(VT_UI1, 1, &bound);
    if (!psa) return nullptr;

    BYTE* pData = nullptr;
    if (SUCCEEDED(SafeArrayAccessData(psa, (void**)&pData)) && pData) {
        memcpy(pData, bytes.data(), bytes.size());
        SafeArrayUnaccessData(psa);
    }
    else {
        SafeArrayDestroy(psa);
        return nullptr;
    }

    return psa;
}

bool AcerBatteryService::IsSupported() {
    wil::com_ptr<IWbemServices> pSvc;
    if (!ConnectWmi(pSvc.put())) {
        Logger::Instance().Log(LogLevel::Warning, L"IsSupported check failed: WMI connection couldn't be established.");
        return false;
    }

    wil::com_ptr<IEnumWbemClassObject> pEnumerator;
    HRESULT hr = pSvc->CreateInstanceEnum(
        _bstr_t(L"BatteryControl"),
        WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY,
        nullptr,
        pEnumerator.put()
    );

    if (FAILED(hr) || !pEnumerator) {
        Logger::Instance().Log(LogLevel::Warning, L"BatteryControl WMI class instance enum failed: " + std::to_wstring(hr));
        return false;
    }

    wil::com_ptr<IWbemClassObject> pInstance;
    ULONG uReturn = 0;
    hr = pEnumerator->Next(WBEM_INFINITE, 1, pInstance.put(), &uReturn);

    bool supported = (SUCCEEDED(hr) && uReturn > 0);
    Logger::Instance().Log(LogLevel::Info, std::wstring(L"BatteryControl WMI interface supported: ") + (supported ? L"YES" : L"NO"));

    return supported;
}

BatteryLimitState AcerBatteryService::GetCurrentStatus() {
    wil::com_ptr<IWbemServices> pSvc;
    if (!ConnectWmi(pSvc.put())) return BatteryLimitState::Unsupported;

    wil::com_ptr<IEnumWbemClassObject> pEnumerator;
    HRESULT hr = pSvc->ExecQuery(
        _bstr_t("WQL"),
        _bstr_t("SELECT * FROM BatteryControl"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        pEnumerator.put()
    );

    if (FAILED(hr) || !pEnumerator) {
        Logger::Instance().Log(LogLevel::Error, L"WQL ExecQuery failed when getting status.");
        return BatteryLimitState::Unsupported;
    }

    wil::com_ptr<IWbemClassObject> pInstance;
    ULONG uReturn = 0;
    pEnumerator->Next(WBEM_INFINITE, 1, pInstance.put(), &uReturn);

    if (uReturn == 0 || !pInstance) {
        Logger::Instance().Log(LogLevel::Warning, L"No BatteryControl instance found.");
        return BatteryLimitState::Unsupported;
    }

    _variant_t vtPath;
    pInstance->Get(L"__PATH", 0, &vtPath, nullptr, nullptr);

    wil::com_ptr<IWbemClassObject> pClass;
    pSvc->GetObject(_bstr_t(L"BatteryControl"), 0, nullptr, pClass.put(), nullptr);
    if (!pClass) return BatteryLimitState::Unsupported;

    wil::com_ptr<IWbemClassObject> pInClass;
    pClass->GetMethod(L"GetBatteryHealthControlStatus", 0, pInClass.put(), nullptr);
    if (!pInClass) return BatteryLimitState::Unsupported;

    wil::com_ptr<IWbemClassObject> pInParams;
    pInClass->SpawnInstance(0, pInParams.put());
    if (!pInParams) return BatteryLimitState::Unsupported;

    _variant_t vtNo((BYTE)1);
    _variant_t vtQuery((BYTE)1);

    SAFEARRAY* psaReserved = CreateByteArray({ 0, 0 });
    if (!psaReserved) return BatteryLimitState::Unsupported;

    VARIANT vtReserved;
    VariantInit(&vtReserved);
    vtReserved.vt = VT_ARRAY | VT_UI1;
    vtReserved.parray = psaReserved;

    pInParams->Put(L"uBatteryNo", 0, &vtNo, 0);
    pInParams->Put(L"uFunctionQuery", 0, &vtQuery, 0);
    pInParams->Put(L"uReserved", 0, &vtReserved, 0);

    wil::com_ptr<IWbemClassObject> pOutParams;
    hr = pSvc->ExecMethod(vtPath.bstrVal, _bstr_t(L"GetBatteryHealthControlStatus"), 0, nullptr, pInParams.get(), pOutParams.put(), nullptr);

    BatteryLimitState state = BatteryLimitState::Unknown;

    if (SUCCEEDED(hr) && pOutParams) {
        _variant_t vtStatus;
        pOutParams->Get(L"uFunctionStatus", 0, &vtStatus, nullptr, nullptr);

        if (vtStatus.vt == (VT_ARRAY | VT_UI1) && vtStatus.parray) {
            BYTE* pData = nullptr;
            SafeArrayAccessData(vtStatus.parray, (void**)&pData);
            state = (pData[0] == 1) ? BatteryLimitState::Limit80 : BatteryLimitState::Limit100;
            SafeArrayUnaccessData(vtStatus.parray);
        }
        else if (vtStatus.vt == VT_UI1) {
            state = (vtStatus.bVal == 1) ? BatteryLimitState::Limit80 : BatteryLimitState::Limit100;
        }
    }
    else {
        Logger::Instance().Log(LogLevel::Error, L"GetBatteryHealthControlStatus execution failed: " + std::to_wstring(hr));
    }

    VariantClear(&vtReserved);

    Logger::Instance().Log(LogLevel::Info, L"Current Status retrieved: " + std::wstring(state == BatteryLimitState::Limit80 ? L"80% Optimized" : L"100% Full"));

    return state;
}

bool AcerBatteryService::SetBatteryLimit(bool enable80Percent) {
    Logger::Instance().Log(LogLevel::Info, L"Attempting to set battery limit mode. Enable 80%: " + std::wstring(enable80Percent ? L"TRUE" : L"FALSE"));

    wil::com_ptr<IWbemServices> pSvc;
    if (!ConnectWmi(pSvc.put())) return false;

    wil::com_ptr<IEnumWbemClassObject> pEnumerator;
    HRESULT hr = pSvc->ExecQuery(
        _bstr_t("WQL"),
        _bstr_t("SELECT * FROM BatteryControl"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        pEnumerator.put()
    );

    if (FAILED(hr) || !pEnumerator) {
        Logger::Instance().Log(LogLevel::Error, L"WQL ExecQuery failed when setting battery limit.");
        return false;
    }

    wil::com_ptr<IWbemClassObject> pInstance;
    ULONG uReturn = 0;
    pEnumerator->Next(WBEM_INFINITE, 1, pInstance.put(), &uReturn);

    if (uReturn == 0 || !pInstance) {
        Logger::Instance().Log(LogLevel::Error, L"No BatteryControl instance found when setting limit.");
        return false;
    }

    _variant_t vtPath;
    pInstance->Get(L"__PATH", 0, &vtPath, nullptr, nullptr);

    wil::com_ptr<IWbemClassObject> pClass;
    pSvc->GetObject(_bstr_t(L"BatteryControl"), 0, nullptr, pClass.put(), nullptr);
    if (!pClass) return false;

    wil::com_ptr<IWbemClassObject> pInClass;
    pClass->GetMethod(L"SetBatteryHealthControl", 0, pInClass.put(), nullptr);
    if (!pInClass) return false;

    wil::com_ptr<IWbemClassObject> pInParams;
    pInClass->SpawnInstance(0, pInParams.put());
    if (!pInParams) return false;

    _variant_t vtNo((BYTE)1);
    _variant_t vtMask((BYTE)1);
    _variant_t vtStatus(enable80Percent ? (BYTE)1 : (BYTE)0);

    SAFEARRAY* psaReserved = CreateByteArray({ 0, 0, 0, 0, 0 });
    if (!psaReserved) return false;

    VARIANT vtReserved;
    VariantInit(&vtReserved);
    vtReserved.vt = VT_ARRAY | VT_UI1;
    vtReserved.parray = psaReserved;

    pInParams->Put(L"uBatteryNo", 0, &vtNo, 0);
    pInParams->Put(L"uFunctionMask", 0, &vtMask, 0);
    pInParams->Put(L"uFunctionStatus", 0, &vtStatus, 0);
    pInParams->Put(L"uReservedIn", 0, &vtReserved, 0);

    wil::com_ptr<IWbemClassObject> pOutParams;
    hr = pSvc->ExecMethod(vtPath.bstrVal, _bstr_t(L"SetBatteryHealthControl"), 0, nullptr, pInParams.get(), pOutParams.put(), nullptr);

    bool success = SUCCEEDED(hr);

    if (success) {
        Logger::Instance().Log(LogLevel::Info, L"SetBatteryHealthControl succeeded.");
    }
    else {
        Logger::Instance().Log(LogLevel::Error, L"SetBatteryHealthControl failed: " + std::to_wstring(hr));
    }

    VariantClear(&vtReserved);

    return success;
}