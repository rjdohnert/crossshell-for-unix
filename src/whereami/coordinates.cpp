#include "coordinates.hpp"

std::string Coordinates::to_raw() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << latitude << "," << longitude;
        return oss.str();
    }

std::string Coordinates::to_sexagesimal() const {
        auto format_dms = [](double deg, bool is_lat) {
            char dir = is_lat ? (deg >= 0 ? 'N' : 'S') : (deg >= 0 ? 'E' : 'W');
            double abs_deg = std::abs(deg);
            int d = static_cast<int>(abs_deg);
            double rem_m = (abs_deg - d) * 60.0;
            int m = static_cast<int>(rem_m);
            double s = (rem_m - m) * 60.0;

            std::ostringstream oss;
            oss << d << "\xC2\xB0 " << m << "' " << std::fixed << std::setprecision(2) << s << "\" " << dir;
            return oss.str();
        };

        return format_dms(latitude, true) + ", " + format_dms(longitude, false);
    }
