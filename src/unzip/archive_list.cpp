#include "archive_entry.hpp"
#include "archive_list.hpp"
#include "unzip_options.hpp"

int listArchive(const UnzipOptions& opts, const std::vector<ArchiveEntry>& entries) {
        std::cout << "Archive:  " << opts.zip_filename << "\n";
        std::cout << "  Length      Date    Time    Name\n";
        std::cout << "---------  ---------- -----   -----\n";

        uint64_t total_size = 0;
        uint64_t total_files = 0;

        for (const auto& e : entries) {
            std::string date = e.date;
            std::string d = date.size() >= 10 ? date.substr(0, 10) : "          ";
            std::string t = date.size() >= 16 ? date.substr(11, 5) : "     ";

            std::cout << std::setw(9) << e.size << "  "
                      << d << " " << t << "   "
                      << e.name << "\n";

            total_size += e.size;
            total_files++;
        }

        std::cout << "---------                     -------\n";
        std::cout << std::setw(9) << total_size << "                     " << total_files << " file(s)\n";
        return 0;
    }
