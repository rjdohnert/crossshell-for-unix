#include "engine.hpp"

std::wstring StringConverter::ToWide(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_ACP, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), static_cast<int>(str.size()), &wstrTo[0], size_needed);
    return wstrTo;
}

fs::path PathValidator::NormalizeTrailingSeparator(const fs::path& p) {
    std::wstring path_str = p.wstring();
    while (path_str.length() > 1 && (path_str.back() == L'\\' || path_str.back() == L'/')) {
        if (path_str.length() == 3 && path_str[1] == L':') {
            break;
        }
        path_str.pop_back();
    }
    return fs::path(path_str);
}

bool PathValidator::IsDotOrDotDot(const fs::path& p) {
    fs::path filename = p.filename();
    return (filename == L"." || filename == L"..");
}

bool PathValidator::IsSubpathOrEqual(const fs::path& parent, const fs::path& child) {
    std::error_code ec;
    fs::path canon_parent = fs::weakly_canonical(parent, ec);
    fs::path canon_child = fs::weakly_canonical(child, ec);

    std::wstring parent_str = canon_parent.wstring();
    std::wstring child_str = canon_child.wstring();

    if (!parent_str.empty() && parent_str.back() != L'\\' && parent_str.back() != L'/') {
        parent_str += L'\\';
    }
    if (!child_str.empty() && child_str.back() != L'\\' && child_str.back() != L'/') {
        child_str += L'\\';
    }

    if (child_str.length() < parent_str.length()) {
        return false;
    }

    return _wcsnicmp(parent_str.c_str(), child_str.c_str(), parent_str.length()) == 0;
}

void DirectoryMover::PrepareForDeletion(const fs::path& p) {
    std::error_code ec;
    if (fs::is_symlink(p, ec)) return;

    if (fs::is_directory(p, ec)) {
        for (const auto& entry : fs::directory_iterator(p, fs::directory_options::skip_permission_denied, ec)) {
            PrepareForDeletion(entry.path());
        }
    }

    DWORD attrs = GetFileAttributesW(p.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
        SetFileAttributesW(p.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
    }
}

bool DirectoryMover::CopyAndRemoveDir(const fs::path& src, const fs::path& dest, std::wstring& err_msg) {
    std::error_code ec;

    if (fs::is_symlink(src, ec)) {
        fs::copy_symlink(src, dest, ec);
        if (ec) {
            err_msg = L"Failed to copy directory symlink: " + StringConverter::ToWide(ec.message());
            return false;
        }
        fs::remove(src, ec);
        return true;
    }

    fs::create_directories(dest, ec);
    if (ec) {
        err_msg = L"Failed to create target directory: " + StringConverter::ToWide(ec.message());
        return false;
    }

    for (const auto& entry : fs::recursive_directory_iterator(src, fs::directory_options::skip_permission_denied, ec)) {
        const auto& src_subpath = entry.path();
        auto rel_path = fs::relative(src_subpath, src, ec);
        fs::path dest_subpath = dest / rel_path;

        if (fs::is_symlink(src_subpath, ec)) {
            fs::copy_symlink(src_subpath, dest_subpath, ec);
        } else if (fs::is_directory(src_subpath, ec)) {
            fs::create_directory(dest_subpath, ec);
        } else if (fs::is_regular_file(src_subpath, ec)) {
            fs::copy_file(src_subpath, dest_subpath, fs::copy_options::overwrite_existing, ec);
            if (!ec) {
                auto last_time = fs::last_write_time(src_subpath, ec);
                if (!ec) fs::last_write_time(dest_subpath, last_time, ec);
            }
        }

        if (ec) {
            err_msg = L"Failed copying " + src_subpath.wstring() + L": " + StringConverter::ToWide(ec.message());
            PrepareForDeletion(dest);
            std::error_code clean_ec;
            fs::remove_all(dest, clean_ec);
            return false;
        }
    }

    auto src_time = fs::last_write_time(src, ec);
    if (!ec) fs::last_write_time(dest, src_time, ec);

    PrepareForDeletion(src);
    fs::remove_all(src, ec);
    if (ec) {
        err_msg = L"Copied to destination, but failed to delete original directory: " + StringConverter::ToWide(ec.message());
        return false;
    }

    return true;
}
