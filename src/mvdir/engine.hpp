#ifndef MVDIR_ENGINE_HPP
#define MVDIR_ENGINE_HPP

#include "mvdir.hpp"

class DirectoryMover {
public:
    static void PrepareForDeletion(const fs::path& p);
    static bool CopyAndRemoveDir(const fs::path& src, const fs::path& dest, std::wstring& err_msg);
};

#endif // MVDIR_ENGINE_HPP
