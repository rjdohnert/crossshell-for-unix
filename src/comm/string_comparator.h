#ifndef COMM_STRING_COMPARATOR_H
#define COMM_STRING_COMPARATOR_H

#include "comm.h"

namespace CommUtil {

// String Comparator Utility
class StringComparator {
public:
    static int compare(std::string_view a, std::string_view b, bool ignore_case);
};

} // namespace CommUtil

#endif // COMM_STRING_COMPARATOR_H
