#include "evr_engine.hpp"

int EvrEngine::rpmvercmp(const std::string& a, const std::string& b) {
        if (a == b) return 0;
        size_t ix1 = 0, ix2 = 0;
        while (ix1 < a.size() || ix2 < b.size()) {
            while (ix1 < a.size() && !isalnum((unsigned char)a[ix1]) && a[ix1] != '~' && a[ix1] != '^') ix1++;
            while (ix2 < b.size() && !isalnum((unsigned char)b[ix2]) && b[ix2] != '~' && b[ix2] != '^') ix2++;

            // Handle tilde separator (sorts earlier than anything)
            if (ix1 < a.size() && a[ix1] == '~') {
                if (ix2 >= b.size() || b[ix2] != '~') return -1;
                ix1++; ix2++; continue;
            }
            if (ix2 < b.size() && b[ix2] == '~') return 1;

            // Handle caret separator (sorts earlier than normal, later than tilde)
            if (ix1 < a.size() && a[ix1] == '^') {
                if (ix2 >= b.size()) return 1;
                if (b[ix2] != '^') return -1;
                ix1++; ix2++; continue;
            }
            if (ix2 < b.size() && b[ix2] == '^') return (ix1 >= a.size()) ? -1 : 1;

            if (ix1 >= a.size() || ix2 >= b.size()) break;

            bool isNum1 = isdigit((unsigned char)a[ix1]);
            bool isNum2 = isdigit((unsigned char)b[ix2]);

            // Digits are always newer than letters
            if (isNum1 && !isNum2) return 1;
            if (!isNum1 && isNum2) return -1;

            size_t start1 = ix1, start2 = ix2;

            if (isNum1 && isNum2) {
                while (ix1 < a.size() && isdigit((unsigned char)a[ix1])) ix1++;
                while (ix2 < b.size() && isdigit((unsigned char)b[ix2])) ix2++;

                std::string seg1 = a.substr(start1, ix1 - start1);
                std::string seg2 = b.substr(start2, ix2 - start2);

                size_t z1 = seg1.find_first_not_of('0');
                size_t z2 = seg2.find_first_not_of('0');
                std::string s1 = (z1 == std::string::npos) ? "0" : seg1.substr(z1);
                std::string s2 = (z2 == std::string::npos) ? "0" : seg2.substr(z2);

                if (s1.size() < s2.size()) return -1;
                if (s1.size() > s2.size()) return 1;
                int cmp = s1.compare(s2);
                if (cmp != 0) return cmp;
            } else {
                while (ix1 < a.size() && isalpha((unsigned char)a[ix1])) ix1++;
                while (ix2 < b.size() && isalpha((unsigned char)b[ix2])) ix2++;

                std::string seg1 = a.substr(start1, ix1 - start1);
                std::string seg2 = b.substr(start2, ix2 - start2);

                int cmp = seg1.compare(seg2);
                if (cmp != 0) return cmp;
            }
        }

        if (ix1 >= a.size() && ix2 >= b.size()) return 0;
        if (ix1 < a.size()) {
            return (a[ix1] == '~') ? -1 : 1;
        }
        return (b[ix2] == '~') ? 1 : -1;
    }
