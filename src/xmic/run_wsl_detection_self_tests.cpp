#include "xmic_common.hpp"

int RunWslDetectionSelfTests() {
    std::wcout << L"\n[Self-test] WSL detection fallback" << std::endl;
    bool passed = true;

    WmiTargetConfig localTarget;
    WslDetectionResult localResult;
    CollectWslDetectionResult(localTarget, L"", WmiSecurityConfig(), localResult);

    if (localResult.status == WslProbeError::Unknown) {
        std::wcerr << L"[FAIL] Local WSL probe returned unknown status." << std::endl;
        passed = false;
    } else {
        std::wcout << L"[PASS] Local probe returned: " << DescribeWslProbeError(localResult.status) << std::endl;
    }

    WmiTargetConfig remoteTarget;
    remoteTarget.node = L"remote-host";
    remoteTarget.transport = TransportMode::DcomRpc;
    WslDetectionResult remoteResult;
    CollectWslDetectionResult(remoteTarget, L"", WmiSecurityConfig(), remoteResult);

    if (remoteResult.status != WslProbeError::RemoteTargetSkipped) {
        std::wcerr << L"[FAIL] Remote target probe should be skipped with a remote-target status." << std::endl;
        passed = false;
    } else {
        std::wcout << L"[PASS] Remote target probe correctly skipped." << std::endl;
    }

    WmiTargetConfig badTransportTarget;
    badTransportTarget.transport = TransportMode::WinRm;
    WslDetectionResult transportResult;
    CollectWslDetectionResult(badTransportTarget, L"", WmiSecurityConfig(), transportResult);

    if (transportResult.status != WslProbeError::UnsupportedTransport) {
        std::wcerr << L"[FAIL] Non-DCOM transport should be flagged as unsupported." << std::endl;
        passed = false;
    } else {
        std::wcout << L"[PASS] Non-DCOM transport correctly rejected." << std::endl;
    }

    if (!passed) {
        std::wcerr << L"[Self-test] One or more WSL fallback checks failed." << std::endl;
        return EXIT_WMI_FAILURE;
    }

    std::wcout << L"[Self-test] WSL fallback checks completed successfully." << std::endl;
    return EXIT_OK;
}
