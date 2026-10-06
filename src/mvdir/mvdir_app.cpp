#include "mvdir_app.hpp"

int MvdirApplication::Run(int argc, wchar_t* argv[]) {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    MvdirOptions opts;
    if (!m_parser.Parse(argc, argv, opts)) {
        return (argc > 1 && (std::wstring(argv[1]) == L"-h" || std::wstring(argv[1]) == L"--help" || std::wstring(argv[1]) == L"/?")) ? 0 : 1;
    }

    if (opts.show_help) {
        OptionParser::PrintHelp();
        return 0;
    }

    if (opts.show_version) {
        OptionParser::PrintVersion();
        return 0;
    }

    const auto& src = opts.source;
    const auto& dest_input = opts.destination;

    if (PathValidator::IsDotOrDotDot(src) || PathValidator::IsDotOrDotDot(dest_input)) {
        std::wcerr << PROG_NAME << L": Cannot move or overwrite '.' or '..'\n";
        return 1;
    }

    if (src.has_root_path() && src == src.root_path()) {
        std::wcerr << PROG_NAME << L": Cannot move root directory (" << src.wstring() << L")\n";
        return 1;
    }

    std::error_code ec;
    if (!fs::exists(src, ec)) {
        std::wcerr << PROG_NAME << L": " << src.wstring() << L": No such file or directory\n";
        return 1;
    }

    if (!fs::is_directory(src, ec)) {
        std::wcerr << PROG_NAME << L": " << src.wstring() << L": Not a directory\n";
        return 1;
    }

    fs::path target_path;
    if (fs::exists(dest_input, ec)) {
        if (fs::is_directory(dest_input, ec)) {
            target_path = dest_input / src.filename();
            if (fs::exists(target_path, ec)) {
                std::wcerr << PROG_NAME << L": " << target_path.wstring() << L": Directory already exists\n";
                return 1;
            }
        } else {
            std::wcerr << PROG_NAME << L": " << dest_input.wstring() << L": Destination is a file, not a directory\n";
            return 1;
        }
    } else {
        target_path = dest_input;
        fs::path parent = target_path.parent_path();
        if (!parent.empty() && !fs::is_directory(parent, ec)) {
            std::wcerr << PROG_NAME << L": " << parent.wstring() << L": Parent directory does not exist\n";
            return 1;
        }
    }

    if (PathValidator::IsSubpathOrEqual(src, target_path)) {
        std::wcerr << PROG_NAME << L": Cannot move directory " << src.wstring() 
                   << L" into itself or a subdirectory of itself (" << target_path.wstring() << L")\n";
        return 1;
    }

    fs::rename(src, target_path, ec);
    if (!ec) {
        return 0;
    }

    if (ec == std::errc::cross_device_link || ec.value() == ERROR_NOT_SAME_DEVICE) {
        std::wstring copy_err;
        if (DirectoryMover::CopyAndRemoveDir(src, target_path, copy_err)) {
            return 0;
        } else {
            std::wcerr << PROG_NAME << L": " << copy_err << L"\n";
            return 1;
        }
    }

    std::wcerr << PROG_NAME << L": Failed to move " << src.wstring() 
               << L" to " << target_path.wstring() 
               << L": " << StringConverter::ToWide(ec.message()) << L"\n";
    return 1;
}
