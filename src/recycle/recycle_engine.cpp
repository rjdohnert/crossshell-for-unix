#include "recycle_engine.hpp"

bool RecycleEngine::RecycleItem(const fs::path& targetPath, bool quiet) {
        std::error_code ec;
        fs::path absPath = fs::absolute(targetPath, ec);
        if (ec) {
            if (!quiet) {
                std::wcerr << L"[ERROR] Invalid path: " << targetPath.wstring() << L"\n";
            }
            return false;
        }

        if (!fs::exists(absPath, ec)) {
            if (!quiet) {
                std::wcerr << L"[ERROR] Not found: " << absPath.wstring() << L"\n";
            }
            return false;
        }

        std::wstring pathBuffer = absPath.wstring();
        pathBuffer.push_back(L'\0');

        SHFILEOPSTRUCTW fileOp = { nullptr };
        fileOp.wFunc = FO_DELETE;
        fileOp.pFrom = pathBuffer.c_str();
        fileOp.pTo = nullptr;
        fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

        int result = SHFileOperationW(&fileOp);

        if (result != 0 || fileOp.fAnyOperationsAborted) {
            if (!quiet) {
                std::wcerr << L"[ERROR] Failed to recycle: " << absPath.wstring()
                           << L" (Code: " << result << L")\n";
            }
            return false;
        }

        return true;
    }
