#include "string_comparator.h"

namespace CommUtil {

int StringComparator::compare(std::string_view a, std::string_view b, bool ignore_case) {
    if (!ignore_case) {
        if (a < b) return -1;
        if (a > b) return 1;
        return 0;
    }

    auto it_a = a.begin();
    auto it_b = b.begin();

    while (it_a != a.end() && it_b != b.end()) {
        unsigned char ca = static_cast<unsigned char>(std::tolower(*it_a));
        unsigned char cb = static_cast<unsigned char>(std::tolower(*it_b));
        if (ca < cb) return -1;
        if (ca > cb) return 1;
        ++it_a;
        ++it_b;
    }

    if (a.length() < b.length()) return -1;
    if (a.length() > b.length()) return 1;
    return 0;
}

} // namespace CommUtil
