#include "xmic_common.hpp"

int RunWslDetectionSelfTests() {
    std::wcout << L"\n[Self-test] WMI formatter, WSL, and CLI validation" << std::endl;
    bool passed = true;
    auto expect = [&passed](bool condition, const std::wstring& label) {
        if (!condition) {
            std::wcerr << L"[FAIL] " << label << std::endl;
            passed = false;
        } else {
            std::wcout << L"[PASS] " << label << std::endl;
        }
    };

    expect(FormatBytes(0) == L"0.00 MB", L"Zero bytes are formatted as a numeric size.");

    VARIANT nanValue;
    VariantInit(&nanValue);
    V_VT(&nanValue) = VT_R8;
    V_R8(&nanValue) = std::numeric_limits<double>::quiet_NaN();
    expect(VariantToJson(nanValue) == L"null", L"NaN serializes as the valid JSON token null.");
    VariantClear(&nanValue);

    SAFEARRAYBOUND floatBounds;
    floatBounds.lLbound = 0;
    floatBounds.cElements = 2;
    SAFEARRAY* floatArray = SafeArrayCreate(VT_R8, 1, &floatBounds);
    bool nonfiniteArrayIsValidJson = false;
    if (floatArray != NULL) {
        LONG index = 0;
        DOUBLE positiveInfinity = std::numeric_limits<double>::infinity();
        HRESULT putHr = SafeArrayPutElement(floatArray, &index, &positiveInfinity);
        index = 1;
        DOUBLE negativeInfinity = -std::numeric_limits<double>::infinity();
        if (SUCCEEDED(putHr)) {
            putHr = SafeArrayPutElement(floatArray, &index, &negativeInfinity);
        }
        if (SUCCEEDED(putHr)) {
            VARIANT arrayValue;
            VariantInit(&arrayValue);
            V_VT(&arrayValue) = static_cast<VARTYPE>(VT_ARRAY | VT_R8);
            V_ARRAY(&arrayValue) = floatArray;
            nonfiniteArrayIsValidJson = VariantToJson(arrayValue) == L"[null,null]";
            VariantClear(&arrayValue);
        } else {
            SafeArrayDestroy(floatArray);
        }
    }
    expect(nonfiniteArrayIsValidJson, L"Positive and negative infinity serialize as valid JSON null array elements.");

    SAFEARRAYBOUND unsupportedBounds;
    unsupportedBounds.lLbound = 0;
    unsupportedBounds.cElements = 1;
    SAFEARRAY* unsupportedArray = SafeArrayCreate(VT_CY, 1, &unsupportedBounds);
    bool unsupportedArrayIsExplicit = false;
    if (unsupportedArray != NULL) {
        LONG index = 0;
        CY currencyValue = {};
        currencyValue.int64 = 123;
        if (SUCCEEDED(SafeArrayPutElement(unsupportedArray, &index, &currencyValue))) {
            VARIANT arrayValue;
            VariantInit(&arrayValue);
            V_VT(&arrayValue) = static_cast<VARTYPE>(VT_ARRAY | VT_CY);
            V_ARRAY(&arrayValue) = unsupportedArray;
            const std::wstring expected = L"[\"<unsupported array element type " +
                std::to_wstring(static_cast<unsigned int>(VT_CY)) + L">\"]";
            unsupportedArrayIsExplicit = VariantToJson(arrayValue) == expected;
            VariantClear(&arrayValue);
        } else {
            SafeArrayDestroy(unsupportedArray);
        }
    }
    expect(unsupportedArrayIsExplicit, L"Unsupported array elements are represented explicitly, not as null.");

    WmiTargetConfig localTarget;
    WslDetectionResult localResult;
    CollectWslDetectionResult(localTarget, L"", WmiSecurityConfig(), localResult);

    expect(localResult.isLocalTarget, L"Local probe identifies the target as local.");
    expect(localResult.status != WslProbeError::Unknown, L"Local probe returns a classified status.");
    expect(localResult.foundDistros
        ? localResult.status == WslProbeError::None
        : localResult.status != WslProbeError::None,
        L"Local probe status agrees with whether distributions were found.");

    WmiTargetConfig remoteTarget;
    remoteTarget.node = L"remote-host";
    remoteTarget.transport = TransportMode::DcomRpc;
    WslDetectionResult remoteResult;
    bool remoteProbeResult = CollectWslDetectionResult(remoteTarget, L"", WmiSecurityConfig(), remoteResult);
    expect(!remoteProbeResult && remoteResult.status == WslProbeError::RemoteTargetSkipped &&
        !remoteResult.attemptedRegistry && !remoteResult.attemptedCli && !remoteResult.foundDistros,
        L"Remote targets are skipped before any registry or CLI probe.");

    WmiTargetConfig badTransportTarget;
    badTransportTarget.transport = TransportMode::WinRm;
    WslDetectionResult transportResult;
    bool transportProbeResult = CollectWslDetectionResult(badTransportTarget, L"", WmiSecurityConfig(), transportResult);
    expect(!transportProbeResult && transportResult.status == WslProbeError::UnsupportedTransport &&
        !transportResult.attemptedRegistry && !transportResult.attemptedCli,
        L"Non-DCOM transport is rejected before probing.");

    WslDetectionResult cliMissing;
    expect(!ApplyWslCliProbeResult(cliMissing, false, false, -1, L"") &&
        cliMissing.status == WslProbeError::WslNotInstalled &&
        cliMissing.attemptedCli && !cliMissing.cliSucceeded && cliMissing.errors.size() == 1,
        L"Missing wsl.exe is classified as unavailable, not a PowerShell failure.");

    WslDetectionResult cliLaunchFailed;
    expect(!ApplyWslCliProbeResult(cliLaunchFailed, true, false, -1, L"") &&
        cliLaunchFailed.status == WslProbeError::WslCliFailed &&
        !cliLaunchFailed.cliSucceeded && cliLaunchFailed.errors.size() == 1,
        L"Failure to start wsl.exe is preserved as a CLI failure.");

    WslDetectionResult cliNonzero;
    expect(!ApplyWslCliProbeResult(cliNonzero, true, true, 1, L"") &&
        cliNonzero.status == WslProbeError::WslCliFailed &&
        !cliNonzero.cliSucceeded && cliNonzero.errors.size() == 1,
        L"Non-zero wsl.exe exit status is classified as a CLI failure.");

    WslDetectionResult cliSuccess;
    expect(ApplyWslCliProbeResult(cliSuccess, true, true, 0, L"Ubuntu\r\nDebian\r\n") &&
        cliSuccess.status == WslProbeError::None && cliSuccess.cliSucceeded &&
        cliSuccess.foundDistros && cliSuccess.distros.size() == 2,
        L"Successful wsl.exe output returns exactly the listed distributions.");

    WslDetectionResult noDistros;
    expect(!ApplyWslCliProbeResult(noDistros, true, true, 0, L"") &&
        noDistros.status == WslProbeError::NoDistrosFound && noDistros.cliSucceeded,
        L"Successful empty wsl.exe output means no distributions were found.");

    WslDetectionResult preservedProbeFailure;
    preservedProbeFailure.status = WslProbeError::AccessDenied;
    preservedProbeFailure.errors.push_back(L"Registry access was denied.");
    expect(!ApplyWslCliProbeResult(preservedProbeFailure, false, false, -1, L"") &&
        preservedProbeFailure.status == WslProbeError::AccessDenied &&
        preservedProbeFailure.errors.size() == 2,
        L"CLI fallback does not overwrite an earlier registry probe failure.");

    std::wstring validationError;
    expect(ValidatePathVerbArgumentCount(L"get", 1, validationError) &&
        !ValidatePathVerbArgumentCount(L"get", 2, validationError),
        L"path get accepts one property list and rejects trailing arguments.");
    expect(!ValidatePathVerbArgumentCount(L"set", 0, validationError) &&
        !ValidatePathVerbArgumentCount(L"set", 2, validationError),
        L"path set rejects a missing assignment and trailing arguments.");
    expect(ValidatePathVerbArgumentCount(L"delete", 0, validationError) &&
        !ValidatePathVerbArgumentCount(L"delete", 1, validationError),
        L"path delete accepts no arguments after its verb.");

    std::wstring settingKey;
    std::wstring settingValue;
    bool resetSetting = false;
    std::wstring settingError;
    expect(!ParseInteractiveShellSetCommand({ L"set", L"node" }, settingKey, settingValue, resetSetting, settingError),
        L"Interactive shell rejects a missing setting value.");
    expect(ParseInteractiveShellSetCommand({ L"set", L"no-header", L"yes" }, settingKey, settingValue, resetSetting, settingError) &&
        settingKey == L"no-header" && settingValue == L"true" && !resetSetting,
        L"Interactive shell normalizes an explicit true boolean value.");
    expect(ParseInteractiveShellSetCommand({ L"set", L"no-header", L"false" }, settingKey, settingValue, resetSetting, settingError) &&
        settingValue == L"false" && !resetSetting,
        L"Interactive shell accepts an explicit false boolean value.");
    expect(ParseInteractiveShellSetCommand({ L"set", L"no-header", L"reset" }, settingKey, settingValue, resetSetting, settingError) &&
        settingKey == L"no-header" && settingValue.empty() && resetSetting,
        L"Interactive shell can reset the no-header boolean to its default.");
    expect(ParseInteractiveShellSetCommand({ L"set", L"node", L"reset" }, settingKey, settingValue, resetSetting, settingError) &&
        settingKey == L"node" && settingValue.empty() && resetSetting,
        L"Interactive shell supports explicit reset of saved settings.");
    expect(!ParseInteractiveShellSetCommand({ L"set", L"no-header", L"maybe" }, settingKey, settingValue, resetSetting, settingError) &&
        !ParseInteractiveShellSetCommand({ L"set", L"node", L"one", L"two" }, settingKey, settingValue, resetSetting, settingError),
        L"Interactive shell rejects invalid booleans and trailing set values.");

    if (!passed) {
        std::wcerr << L"[Self-test] One or more WMI formatter, WSL, or CLI checks failed." << std::endl;
        return EXIT_WMI_FAILURE;
    }

    std::wcout << L"[Self-test] WMI formatter, WSL, and CLI validation checks completed successfully." << std::endl;
    return EXIT_OK;
}
