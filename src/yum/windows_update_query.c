#include "windows_update_query.h"

int Yum_UpdateWindows(const YumContext* ctx) {
    (void)ctx;
    wprintf(L"Loaded plugins: fastestmirror, native-wua\n");
    wprintf(L"Querying Windows Update Agent subsystem...\n\n");

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        fwprintf(stderr, L"Fatal: CoInitializeEx failed (0x%08lX)\n", hr);
        return 1;
    }

    IUpdateSession* pSession = NULL;
    hr = CoCreateInstance(&CLSID_UpdateSession, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IUpdateSession, (void**)&pSession);
    if (FAILED(hr)) {
        fwprintf(stderr, L"Error: Failed to instantiate UpdateSession (0x%08lX)\n", hr);
        CoUninitialize();
        return 1;
    }

    IUpdateSearcher* pSearcher = NULL;
    hr = pSession->lpVtbl->CreateUpdateSearcher(pSession, &pSearcher);
    if (FAILED(hr)) {
        fwprintf(stderr, L"Error: Failed to create UpdateSearcher (0x%08lX)\n", hr);
        pSession->lpVtbl->Release(pSession);
        CoUninitialize();
        return 1;
    }

    BSTR criteria = SysAllocString(L"IsInstalled=0 and IsHidden=0 and Type='Software'");
    ISearchResult* pSearchResult = NULL;

    wprintf(L"Searching for pending OS updates against Microsoft Update catalogs...\n");
    hr = pSearcher->lpVtbl->Search(pSearcher, criteria, &pSearchResult);
    SysFreeString(criteria);

    if (FAILED(hr)) {
        fwprintf(stderr, L"Error: Search query failed (0x%08lX)\n", hr);
        pSearcher->lpVtbl->Release(pSearcher);
        pSession->lpVtbl->Release(pSession);
        CoUninitialize();
        return 1;
    }

    IUpdateCollection* pUpdates = NULL;
    hr = pSearchResult->lpVtbl->get_Updates(pSearchResult, &pUpdates);
    if (FAILED(hr) || !pUpdates) {
        fwprintf(stderr, L"Error: Failed to retrieve update collection.\n");
        pSearchResult->lpVtbl->Release(pSearchResult);
        pSearcher->lpVtbl->Release(pSearcher);
        pSession->lpVtbl->Release(pSession);
        CoUninitialize();
        return 1;
    }

    long count = 0;
    pUpdates->lpVtbl->get_Count(pUpdates, &count);

    if (count == 0) {
        wprintf(L"\nNo Windows updates available. Your system is up to date.\n");
    } else {
        wprintf(L"\n================================================================================\n");
        wprintf(L" Package                          KB Article                 Mandatory         \n");
        wprintf(L"================================================================================\n");

        for (long i = 0; i < count; ++i) {
            IUpdate* pUpdate = NULL;
            if (SUCCEEDED(pUpdates->lpVtbl->get_Item(pUpdates, i, &pUpdate))) {
                BSTR title = NULL;
                pUpdate->lpVtbl->get_Title(pUpdate, &title);

                VARIANT_BOOL isMandatory = VARIANT_FALSE;
                pUpdate->lpVtbl->get_IsMandatory(pUpdate, &isMandatory);

                IStringCollection* pKBs = NULL;
                BSTR kbId = NULL;
                if (SUCCEEDED(pUpdate->lpVtbl->get_KBArticleIDs(pUpdate, &pKBs)) && pKBs) {
                    long kbCount = 0;
                    pKBs->lpVtbl->get_Count(pKBs, &kbCount);
                    if (kbCount > 0) {
                        pKBs->lpVtbl->get_Item(pKBs, 0, &kbId);
                    }
                    pKBs->lpVtbl->Release(pKBs);
                }

                wchar_t kbFormatted[32];
                if (kbId && kbId[0] != L'\0') {
                    if (_wcsnicmp(kbId, L"KB", 2) == 0) {
                        wcsncpy_s(kbFormatted, 32, kbId, _TRUNCATE);
                    } else {
                        swprintf_s(kbFormatted, 32, L"KB%ls", kbId);
                    }
                } else {
                    wcsncpy_s(kbFormatted, 32, L"None", _TRUNCATE);
                }

                wprintf(L"[%2ld] %-50.50ls %-15ls %-10ls\n",
                        i + 1,
                        title ? title : L"Unknown",
                        kbFormatted,
                        (isMandatory == VARIANT_TRUE) ? L"Yes" : L"No");

                if (title) SysFreeString(title);
                if (kbId) SysFreeString(kbId);
                pUpdate->lpVtbl->Release(pUpdate);
            }
        }
        wprintf(L"\nFound %ld pending system update(s). Run 'yum sys-upgrade' to install them, or use Settings.\n", count);
    }

    pUpdates->lpVtbl->Release(pUpdates);
    pSearchResult->lpVtbl->Release(pSearchResult);
    pSearcher->lpVtbl->Release(pSearcher);
    pSession->lpVtbl->Release(pSession);
    CoUninitialize();

    return 0;
}
