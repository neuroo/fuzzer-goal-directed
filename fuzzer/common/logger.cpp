
#include "logger.h"

namespace instr {

void setupLogger(const std::string log_file) {
  el::Configurations defaultConf;
  defaultConf.setToDefault();
  defaultConf.setGlobally(el::ConfigurationType::Format,
                          "[%level] %datetime (%thread) (%loc) %msg");
  defaultConf.setGlobally(el::ConfigurationType::Filename,
                          std::string(log_file));
  defaultConf.setGlobally(el::ConfigurationType::ToStandardOutput,
                          std::string("false"));
  el::Loggers::reconfigureLogger("default", defaultConf);
}
}
