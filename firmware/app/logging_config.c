#include "logging_config.h"

log_threshold_t logging_default_global_threshold(void)
{
    return LOG_THRESHOLD_INFO;
}

const char *logging_level_name(log_level_t level)
{
    switch (level) {
    case LOG_LEVEL_DEBUG:
        return "DEBUG";
    case LOG_LEVEL_INFO:
        return "INFO";
    case LOG_LEVEL_WARN:
        return "WARN";
    case LOG_LEVEL_ERROR:
        return "ERROR";
    case LOG_LEVEL_FATAL:
        return "FATAL";
    case LOG_LEVEL_COUNT:
        break;
    }

    return "UNKNOWN";
}

const char *logging_module_name(log_module_t module)
{
    switch (module) {
    case LOG_MODULE_SYSTEM:
        return "SYSTEM";
    case LOG_MODULE_BOARD:
        return "BOARD";
    case LOG_MODULE_TIMEBASE:
        return "TIMEBASE";
    case LOG_MODULE_TASK:
        return "TASK";
    case LOG_MODULE_SCHEDULER:
        return "SCHEDULER";
    case LOG_MODULE_STATE:
        return "STATE";
    case LOG_MODULE_FAULT:
        return "FAULT";
    case LOG_MODULE_COUNT:
        break;
    }

    return "UNKNOWN";
}
