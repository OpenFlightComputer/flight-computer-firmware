#ifndef OPENFLIGHTCOMPUTER_LOGGING_CONFIG_H
#define OPENFLIGHTCOMPUTER_LOGGING_CONFIG_H

#include "logging.h"

log_threshold_t logging_default_global_threshold(void);
const char *logging_level_name(log_level_t level);
const char *logging_module_name(log_module_t module);

#endif
