#ifndef REPORTER_HPP
#define REPORTER_HPP

#include "pinky.hpp"

class PinkyReporter {
public:
    static void PrintSummary(const std::vector<PinkySessionSummary>& sessions);
    static void PrintProfile(const PinkyUserProfile& profile, bool printPlan);
};

#endif // REPORTER_HPP
