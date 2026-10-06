#include "pbcopy_app.hpp"

int PbcopyApplication::Run(int argc, wchar_t* argv[]) {
    PbcopyOptions opts;
    bool exitEarly = false;
    if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
        return 1;
    }
    if (exitEarly) {
        return 0;
    }

    std::vector<uint8_t> raw_bytes = StreamReader::ReadAllStdin();
    std::wstring clipboard_text = EncodingConverter::ConvertToWString(raw_bytes);

    if (!ClipboardManager::SetClipboardText(clipboard_text)) {
        return 1;
    }

    return 0;
}
