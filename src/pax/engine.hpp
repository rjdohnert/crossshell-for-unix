#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "pax.hpp"
#include "options.hpp"

class PaxArchiver {
public:
    static void List(std::istream& in, bool verbose, const std::vector<Substitution>& subs);
    static void Read(std::istream& in, const fs::path& dest_dir, bool verbose, bool keep, bool update, const std::vector<Substitution>& subs);
    static void WriteTarEntry(std::ostream& out, const fs::path& path, const std::string& tar_path, bool verbose);
    static void Write(std::ostream& out, const std::vector<fs::path>& sources, bool verbose, const std::vector<Substitution>& subs);
    static void Copy(const std::vector<fs::path>& sources, const fs::path& dest_dir, bool verbose, bool keep, bool update, const std::vector<Substitution>& subs);
};

#endif // ENGINE_HPP
