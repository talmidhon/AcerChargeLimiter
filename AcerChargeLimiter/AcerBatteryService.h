#pragma once
#include <windows.h>
#include <Wbemidl.h>
#include <comdef.h>
#include <vector>

enum class BatteryLimitState {
    Limit80,
    Limit100,
    Unknown,
    Unsupported
};

class AcerBatteryService {
public:
    AcerBatteryService();
    ~AcerBatteryService();

    bool IsSupported();
    BatteryLimitState GetCurrentStatus();
    bool SetBatteryLimit(bool enable80Percent);

private:
    bool InitializeCom();
    bool ConnectWmi(IWbemServices** ppSvc);
    SAFEARRAY* CreateByteArray(const std::vector<BYTE>& bytes);

    bool m_comInitialized{ false };
};