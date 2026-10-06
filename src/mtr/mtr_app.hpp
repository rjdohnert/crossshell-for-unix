#ifndef MTR_APP_HPP
#define MTR_APP_HPP

#include "mtr.hpp"
#include "options.hpp"
#include "engine.hpp"
#include "reporter.hpp"

class MTR {
    MtrOptions options_;
    IN_ADDR targetAddr_{};
    std::string targetIpStr_;
    DnsService dns_;
    std::unique_ptr<IcmpEngine> prober_;
    std::vector<HopRecord> hops_;
    bool isPaused_{ false };

public:
    explicit MTR(MtrOptions opts);
    bool setup();
    void execute();

private:
    void processKeyboard(bool& keepRunning);
};

#endif // MTR_APP_HPP
