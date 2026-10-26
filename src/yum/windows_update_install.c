#include "windows_update_install.h"
#include "privilege_check.h"

int Yum_SystemUpgrade(const YumContext* ctx) {
    if (!Yum_IsElevated()) {
        fwprintf(stderr, L"yum sys-upgrade requires an Administrator terminal. No updates were installed.\n");
        return 1;
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        fwprintf(stderr, L"yum: COM initialization failed (0x%08lX)\n", (unsigned long)hr);
        return 1;
    }

    int status = 1;
    const wchar_t* failure = NULL;
    IUpdateSession* session = NULL;
    IUpdateSearcher* searcher = NULL;
    ISearchResult* search_result = NULL;
    IUpdateCollection* available = NULL;
    IUpdateCollection* downloaded = NULL;
    IUpdateDownloader* downloader = NULL;
    IDownloadResult* download_result = NULL;
    IUpdateInstaller* installer = NULL;
    IInstallationResult* install_result = NULL;
    IUpdateInstallationResult* item_result = NULL;
    IUpdate* update = NULL;
    BSTR criteria = NULL, client_id = NULL, title = NULL, license = NULL;
    OperationResultCode result_code = orcNotStarted;
    long count = 0, install_count = 0, installed = 0, failed = 0, download_failed = 0;
    VARIANT_BOOL reboot = VARIANT_FALSE;
    CLSID collection_class;

#define YUM_WUA_TRY(expression, description) do { hr = (expression); if (FAILED(hr)) { failure = (description); goto cleanup_upgrade; } } while (0)

    YUM_WUA_TRY(CoCreateInstance(&CLSID_UpdateSession, NULL, CLSCTX_INPROC_SERVER,
        &IID_IUpdateSession, (void**)&session), L"create Windows Update session");
    client_id = SysAllocString(L"CrossShell yum sys-upgrade");
    criteria = SysAllocString(L"IsInstalled=0 and IsHidden=0 and Type='Software'");
    if (!client_id || !criteria) { hr = E_OUTOFMEMORY; failure = L"allocate search parameters"; goto cleanup_upgrade; }
    YUM_WUA_TRY(session->lpVtbl->put_ClientApplicationID(session, client_id), L"identify update client");
    YUM_WUA_TRY(session->lpVtbl->CreateUpdateSearcher(session, &searcher), L"create update searcher");
    wprintf(L"Loaded plugins: fastestmirror, native-wua\nSearching the configured Windows Update service...\n");
    YUM_WUA_TRY(searcher->lpVtbl->Search(searcher, criteria, &search_result), L"search Windows Update");
    YUM_WUA_TRY(search_result->lpVtbl->get_ResultCode(search_result, &result_code), L"read search result");
    if (result_code != orcSucceeded) {
        fwprintf(stderr, L"yum: Windows Update search did not complete successfully (result %d).\n", (int)result_code);
        goto cleanup_upgrade;
    }
    YUM_WUA_TRY(search_result->lpVtbl->get_Updates(search_result, &available), L"retrieve pending updates");
    YUM_WUA_TRY(available->lpVtbl->get_Count(available, &count), L"count pending updates");
    if (!count) { wprintf(L"No pending non-hidden Windows software updates.\n"); status = 0; goto cleanup_upgrade; }

    wprintf(L"\nPending Windows updates:\n");
    for (long index = 0; index < count; ++index) {
        YUM_WUA_TRY(available->lpVtbl->get_Item(available, index, &update), L"retrieve update details");
        YUM_WUA_TRY(update->lpVtbl->get_Title(update, &title), L"read update title");
        wprintf(L"  [%ld] %ls\n", index + 1, title ? title : L"Untitled update");
        VARIANT_BOOL accepted = VARIANT_FALSE;
        YUM_WUA_TRY(update->lpVtbl->get_EulaAccepted(update, &accepted), L"read license acceptance");
        if (accepted != VARIANT_TRUE) {
            YUM_WUA_TRY(update->lpVtbl->get_EulaText(update, &license), L"read update license");
            wprintf(L"\nLicense for this update:\n%ls\n\n", license ? license : L"See Windows Update for license terms.");
            SysFreeString(license); license = NULL;
        }
        SysFreeString(title); title = NULL;
        update->lpVtbl->Release(update); update = NULL;
    }
    if (!ctx->assume_yes) {
        wchar_t answer[16];
        wprintf(L"Download and install these %ld update(s), accepting any displayed licenses? [y/N] ", count);
        fflush(stdout);
        if (!fgetws(answer, (int)(sizeof(answer) / sizeof(answer[0])), stdin) ||
            (answer[0] != L'y' && answer[0] != L'Y')) {
            wprintf(L"Cancelled. No updates were downloaded or installed.\n");
            status = 0; goto cleanup_upgrade;
        }
    }
    for (long index = 0; index < count; ++index) {
        YUM_WUA_TRY(available->lpVtbl->get_Item(available, index, &update), L"retrieve update for license acceptance");
        VARIANT_BOOL accepted = VARIANT_FALSE;
        YUM_WUA_TRY(update->lpVtbl->get_EulaAccepted(update, &accepted), L"read update license state");
        if (accepted != VARIANT_TRUE) YUM_WUA_TRY(update->lpVtbl->AcceptEula(update), L"accept confirmed update license");
        update->lpVtbl->Release(update); update = NULL;
    }

    YUM_WUA_TRY(session->lpVtbl->CreateUpdateDownloader(session, &downloader), L"create update downloader");
    YUM_WUA_TRY(downloader->lpVtbl->put_Updates(downloader, available), L"select updates for download");
    wprintf(L"\nDownloading %ld update(s)...\n", count);
    YUM_WUA_TRY(downloader->lpVtbl->Download(downloader, &download_result), L"download updates");
    YUM_WUA_TRY(download_result->lpVtbl->get_ResultCode(download_result, &result_code), L"read download result");
    if (ctx->verbose) wprintf(L"Download operation result: %d\n", (int)result_code);
    if (result_code != orcSucceeded && result_code != orcSucceededWithErrors) {
        fwprintf(stderr, L"yum: update download did not succeed (result %d).\n", (int)result_code);
        goto cleanup_upgrade;
    }
    if (FAILED(CLSIDFromProgID(L"Microsoft.Update.UpdateColl", &collection_class))) {
        static const GUID clsid_coll = { 0x07F854D7, 0xE363, 0x4623, { 0x87, 0x6E, 0x76, 0x37, 0x31, 0x24, 0xE1, 0x90 } };
        collection_class = clsid_coll;
    }
    YUM_WUA_TRY(CoCreateInstance(&collection_class, NULL, CLSCTX_INPROC_SERVER,
        &IID_IUpdateCollection, (void**)&downloaded), L"create installation collection");
    for (long index = 0; index < count; ++index) {
        YUM_WUA_TRY(available->lpVtbl->get_Item(available, index, &update), L"retrieve downloaded update");
        VARIANT_BOOL ready = VARIANT_FALSE;
        YUM_WUA_TRY(update->lpVtbl->get_IsDownloaded(update, &ready), L"verify downloaded update");
        if (ready == VARIANT_TRUE) {
            long added_index = 0;
            YUM_WUA_TRY(downloaded->lpVtbl->Add(downloaded, update, &added_index), L"queue downloaded update");
        } else {
            ++download_failed;
            YUM_WUA_TRY(update->lpVtbl->get_Title(update, &title), L"read failed download title");
            fwprintf(stderr, L"Not downloaded; skipping: %ls\n", title ? title : L"Untitled update");
            SysFreeString(title); title = NULL;
        }
        update->lpVtbl->Release(update); update = NULL;
    }
    YUM_WUA_TRY(downloaded->lpVtbl->get_Count(downloaded, &install_count), L"count installable updates");
    if (!install_count) { fwprintf(stderr, L"No updates were downloaded successfully.\n"); goto cleanup_upgrade; }
    YUM_WUA_TRY(session->lpVtbl->CreateUpdateInstaller(session, &installer), L"create update installer");
    YUM_WUA_TRY(installer->lpVtbl->put_Updates(installer, downloaded), L"select updates for installation");
    VARIANT_BOOL busy = VARIANT_FALSE, reboot_before = VARIANT_FALSE;
    YUM_WUA_TRY(installer->lpVtbl->get_IsBusy(installer, &busy), L"check installer availability");
    if (busy == VARIANT_TRUE) { fwprintf(stderr, L"Windows Update is already installing updates. Try again later.\n"); goto cleanup_upgrade; }
    YUM_WUA_TRY(installer->lpVtbl->get_RebootRequiredBeforeInstallation(installer, &reboot_before), L"check pending restart");
    if (reboot_before == VARIANT_TRUE) {
        wprintf(L"Windows requires a restart before installation. Restart manually, then run yum sys-upgrade again.\n");
        status = 3010; goto cleanup_upgrade;
    }
    YUM_WUA_TRY(installer->lpVtbl->put_AllowSourcePrompts(installer, VARIANT_FALSE), L"disable installer source prompts");
    wprintf(L"Installing %ld update(s)...\n", install_count);
    YUM_WUA_TRY(installer->lpVtbl->Install(installer, &install_result), L"install updates");
    YUM_WUA_TRY(install_result->lpVtbl->get_ResultCode(install_result, &result_code), L"read installation result");
    YUM_WUA_TRY(install_result->lpVtbl->get_RebootRequired(install_result, &reboot), L"read restart requirement");
    for (long index = 0; index < install_count; ++index) {
        OperationResultCode item_code = orcNotStarted;
        HRESULT item_hr = E_FAIL;
        YUM_WUA_TRY(downloaded->lpVtbl->get_Item(downloaded, index, &update), L"retrieve installed update");
        YUM_WUA_TRY(update->lpVtbl->get_Title(update, &title), L"read installed update title");
        YUM_WUA_TRY(install_result->lpVtbl->GetUpdateResult(install_result, index, &item_result), L"read individual installation result");
        YUM_WUA_TRY(item_result->lpVtbl->get_ResultCode(item_result, &item_code), L"read individual result code");
        YUM_WUA_TRY(item_result->lpVtbl->get_HResult(item_result, &item_hr), L"read individual error code");
        if (item_code == orcSucceeded && SUCCEEDED(item_hr)) { ++installed; wprintf(L"Installed: %ls\n", title ? title : L"Untitled update"); }
        else { ++failed; fwprintf(stderr, L"Failed: %ls (result %d, HRESULT 0x%08lX)\n", title ? title : L"Untitled update", (int)item_code, (unsigned long)item_hr); }
        item_result->lpVtbl->Release(item_result); item_result = NULL;
        SysFreeString(title); title = NULL;
        update->lpVtbl->Release(update); update = NULL;
    }
    wprintf(L"\nWindows Update summary: %ld installed, %ld installation failures, %ld download failures.\n", installed, failed, download_failed);
    if (reboot == VARIANT_TRUE) wprintf(L"A restart is required. Restart manually when convenient; yum will not reboot automatically.\n");
    status = (failed || download_failed || result_code != orcSucceeded) ? 1 : ((reboot == VARIANT_TRUE) ? 3010 : 0);

cleanup_upgrade:
    if (failure) fwprintf(stderr, L"yum: failed to %ls (HRESULT 0x%08lX)\n", failure, (unsigned long)hr);
    SysFreeString(criteria); SysFreeString(client_id); SysFreeString(title); SysFreeString(license);
    if (item_result) item_result->lpVtbl->Release(item_result);
    if (update) update->lpVtbl->Release(update);
    if (install_result) install_result->lpVtbl->Release(install_result);
    if (installer) installer->lpVtbl->Release(installer);
    if (download_result) download_result->lpVtbl->Release(download_result);
    if (downloader) downloader->lpVtbl->Release(downloader);
    if (downloaded) downloaded->lpVtbl->Release(downloaded);
    if (available) available->lpVtbl->Release(available);
    if (search_result) search_result->lpVtbl->Release(search_result);
    if (searcher) searcher->lpVtbl->Release(searcher);
    if (session) session->lpVtbl->Release(session);
    CoUninitialize();
#undef YUM_WUA_TRY
    return status;
}

