#ifndef COMM_ENGINE_H
#define COMM_ENGINE_H

#include "comm.h"
#include "options.h"
#include "exit_code.h"
#include "line_stream.h"
#include "column_formatter.h"
#include "order_validator.h"

namespace CommUtil {

// Core Engine (Three-Way Merge Algorithm)
class CommEngine {
public:
    explicit CommEngine(Options opts);

    ExitCode execute();

private:
    Options opts_;
    std::unique_ptr<ILineStream> stream1_;
    std::unique_ptr<ILineStream> stream2_;
    ColumnFormatter formatter_;
    OrderValidator validator1_;
    OrderValidator validator2_;
};

} // namespace CommUtil

#endif // COMM_ENGINE_H
