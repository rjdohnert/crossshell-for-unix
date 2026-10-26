#include "xmic_common.hpp"

std::wstring DescribeWmiConnectionFailure(HRESULT hr) {
    return DescribeWmiHResult(hr);
}
bool ConnectWmiServices(const WmiTargetConfig& target, const std::wstring& defaultNamespace, const WmiSecurityConfig& securityConfig, const std::wstring& wmiLocale, IWbemServices** ppSvc, bool reportError) {
    *ppSvc = NULL;
    if (target.transport == TransportMode::Ssh || target.transport == TransportMode::WinRm) {
        return false;
    }

    if (!target.node.empty() && target.node != L"." && _wcsicmp(target.node.c_str(), L"localhost") != 0) {
        if (target.user.empty()) {
            std::wcerr << L"[!] Remote WMI targets require explicit credentials or a configured profile." << std::endl;
        }
    }

    IWbemLocator* pLoc = NULL;
    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hr)) {
        std::wcerr << L"[!] Failed to create WbemLocator: " << DescribeWmiHResult(hr) << std::endl;
        return false;
    }

    std::wstring targetNs = defaultNamespace.empty() ? L"ROOT\\CIMV2" : defaultNamespace;
    std::wstring fullPath;
    if (target.node == L"." || target.node.empty()) {
        fullPath = targetNs;
    } else {
        fullPath.reserve(2 + target.node.size() + 1 + targetNs.size());
        fullPath.append(L"\\\\");
        fullPath.append(target.node);
        fullPath.push_back(L'\\');
        fullPath.append(targetNs);
    }

    _bstr_t bstrPath(fullPath.c_str());
    _bstr_t bstrUser(target.user.empty() ? NULL : target.user.c_str());
    _bstr_t bstrPass(target.password.empty() ? NULL : target.password.c_str());
    _bstr_t bstrLocale(wmiLocale.empty() ? NULL : wmiLocale.c_str());

    hr = pLoc->ConnectServer(bstrPath, bstrUser, bstrPass, bstrLocale, 0, 0, 0, ppSvc);
    pLoc->Release();

    if (FAILED(hr) || !*ppSvc) {
        if (reportError) {
            std::wcerr << L"[!] WMI connection failed for target '" << (target.node.empty() ? L"localhost" : target.node) << L"': " << DescribeWmiHResult(hr) << std::endl;
        }
        return false;
    }

    SEC_WINNT_AUTH_IDENTITY_W authIdent = { 0 };
    SEC_WINNT_AUTH_IDENTITY_W* pAuthIdent = NULL;

    if (!target.user.empty()) {
        authIdent.User = (RPC_WSTR)target.user.c_str();
        authIdent.UserLength = (ULONG)target.user.length();
        authIdent.Password = (RPC_WSTR)target.password.c_str();
        authIdent.PasswordLength = (ULONG)target.password.length();
        authIdent.Domain = (RPC_WSTR)target.domain.c_str();
        authIdent.DomainLength = (ULONG)target.domain.length();
        authIdent.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
        pAuthIdent = &authIdent;
    }

    hr = CoSetProxyBlanket(
        *ppSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        securityConfig.authnLevel,
        securityConfig.impLevel,
        pAuthIdent,
        EOAC_NONE
    );

    if (FAILED(hr)) {
        std::wcerr << L"[!] WMI proxy blanket setup failed: " << DescribeWmiHResult(hr) << std::endl;
        (*ppSvc)->Release();
        *ppSvc = NULL;
        return false;
    }

    return true;
}
