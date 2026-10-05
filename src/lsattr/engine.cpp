#include "engine.hpp"

ScopedFindHandle::ScopedFindHandle(HANDLE handle) : m_handle(handle) {}

ScopedFindHandle::~ScopedFindHandle() {
    Close();
}

ScopedFindHandle::ScopedFindHandle(ScopedFindHandle&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = INVALID_HANDLE_VALUE;
}

ScopedFindHandle& ScopedFindHandle::operator=(ScopedFindHandle&& other) noexcept {
    if (this != &other) {
        Close();
        m_handle = other.m_handle;
        other.m_handle = INVALID_HANDLE_VALUE;
    }
    return *this;
}

HANDLE ScopedFindHandle::Get() const {
    return m_handle;
}

bool ScopedFindHandle::IsValid() const {
    return m_handle != INVALID_HANDLE_VALUE && m_handle != NULL;
}

void ScopedFindHandle::Close() {
    if (IsValid()) {
        FindClose(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
}

std::wstring AttributeInspector::GetFlags(DWORD attrs) {
    std::wstring flags;
    flags += (attrs & FILE_ATTRIBUTE_ARCHIVE) ? L'a' : L'-';
    flags += (attrs & FILE_ATTRIBUTE_HIDDEN) ? L'h' : L'-';
    flags += (attrs & FILE_ATTRIBUTE_SYSTEM) ? L's' : L'-';
    flags += (attrs & FILE_ATTRIBUTE_READONLY) ? L'i' : L'-';
    return flags;
}

std::wstring AttributeInspector::GetFlagsForPath(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return L"";
    }
    return GetFlags(attrs);
}

bool LsattrTraverser::Collect(const std::wstring& path, bool recursive, std::vector<AttrRow>& rows, std::wostream& err) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        err << L"lsattr: " << path << L": No such file or directory\n";
        return false;
    }

    rows.push_back({ AttributeInspector::GetFlags(attrs), path });

    if (recursive && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        std::wstring pattern = path;
        if (!pattern.empty() && pattern.back() != L'\\') {
            pattern.push_back(L'\\');
        }
        pattern.push_back(L'*');

        WIN32_FIND_DATAW findData = {};
        ScopedFindHandle hFind(FindFirstFileW(pattern.c_str(), &findData));
        if (hFind.IsValid()) {
            do {
                if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                    continue;
                }
                std::wstring child = path;
                if (!child.empty() && child.back() != L'\\') {
                    child.push_back(L'\\');
                }
                child += findData.cFileName;

                DWORD childAttrs = GetFileAttributesW(child.c_str());
                if (childAttrs != INVALID_FILE_ATTRIBUTES) {
                    rows.push_back({ AttributeInspector::GetFlags(childAttrs), child });
                }
            } while (FindNextFileW(hFind.Get(), &findData));
        }
    }

    return true;
}
