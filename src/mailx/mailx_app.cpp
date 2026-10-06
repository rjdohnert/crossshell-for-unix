#include "mailx_app.hpp"
#include "engine.hpp"
#include <iostream>

int MailxApplication::Run(int argc, wchar_t* argv[]) {
    MailOptions opt;
    if (!opt.Parse(argc, argv)) {
        opt.PrintHelp();
        return 1;
    }

    if (opt.showHelp) {
        opt.PrintHelp();
        return 0;
    }

    if (opt.showVersion) {
        opt.PrintVersion();
        return 0;
    }

    if (opt.to.empty()) {
        std::wcerr << L"mailx: missing recipient\n";
        return 1;
    }
    if (opt.from.empty()) {
        std::wcerr << L"mailx: missing sender address (-r FROM)\n";
        return 1;
    }

    for (const auto& a : opt.attachments) {
        if (!MailxPayloadBuilder::FileExists(a)) {
            std::wcerr << L"mailx: attachment not found: " << a << L"\n";
            return 1;
        }
    }

    if (!opt.bodyFile.empty()) {
        if (!MailxPayloadBuilder::ReadFileAll(opt.bodyFile, opt.body)) {
            std::wcerr << L"mailx: failed to read body file: " << opt.bodyFile << L"\n";
            return 1;
        }
    } else if (opt.body.empty()) {
        opt.body = MailxPayloadBuilder::ReadStdinAll();
    }

    std::wstring payloadPath = MailxPayloadBuilder::GetTempFilePath(L"mlx", L".json");
    std::wstring scriptPath = MailxPayloadBuilder::GetTempFilePath(L"mlx", L".ps1");
    if (payloadPath.empty() || scriptPath.empty()) {
        std::wcerr << L"mailx: failed to create temporary files\n";
        return 125;
    }

    int rc = 125;
    if (!MailxPayloadBuilder::WriteJsonPayload(opt, payloadPath)) {
        std::wcerr << L"mailx: failed to write payload\n";
    } else if (!MailxPayloadBuilder::WritePsScript(scriptPath)) {
        std::wcerr << L"mailx: failed to write sender script\n";
    } else {
        rc = SmtpDispatcher::RunPowerShellMail(scriptPath, payloadPath);
    }

    DeleteFileW(payloadPath.c_str());
    DeleteFileW(scriptPath.c_str());
    return rc;
}
