#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "mailx.hpp"
#include <sstream>

class MailxPayloadBuilder {
public:
    static std::wstring QuoteCmdArg(const std::wstring& arg);
    static std::wstring ReadStdinAll();
    static bool ReadFileAll(const std::wstring& path, std::wstring& out);
    static bool FileExists(const std::wstring& path);
    static std::wstring JsonEscape(const std::wstring& v);
    static void AppendJsonArray(std::wstringstream& ss, const std::vector<std::wstring>& items);
    static bool WriteJsonPayload(const MailOptions& opt, const std::wstring& path);
    static bool WritePsScript(const std::wstring& path);
    static std::wstring GetTempFilePath(const wchar_t* prefix, const wchar_t* ext);
};

class SmtpDispatcher {
public:
    static int RunPowerShellMail(const std::wstring& scriptPath, const std::wstring& payloadPath);
};

#endif // ENGINE_HPP
