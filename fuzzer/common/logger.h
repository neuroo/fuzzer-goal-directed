#ifndef LOGGER_H
#define LOGGER_H

#define XXX_ELPP_NO_EXCEPTION
#define ELPP_STL_LOGGING
#define ELPP_THREAD_SAFE
#define ELPP_USE_STD_THREADING 1
#define ELPP_NO_DEFAULT_LOG_FILE
#define ELPP_DISABLE_DEFAULT_CRASH_HANDLING
#define ELPP_DISABLE_PERFORMANCE_TRACKING
#include <easylogging++.h>

namespace instr {
void setupLogger(const std::string log_file = "instrument.log");
}

#endif
