#ifndef OPENFLIGHTCOMPUTER_LOGGING_H
#define OPENFLIGHTCOMPUTER_LOGGING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LOGGING_QUEUE_CAPACITY 32U
#define LOGGING_MESSAGE_CAPACITY 96U
#define LOGGING_FORMATTED_LINE_CAPACITY 160U

typedef uint64_t (*logging_clock_t)(void);

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
    LOG_LEVEL_COUNT,
} log_level_t;

typedef enum {
    LOG_THRESHOLD_DEBUG = 0,
    LOG_THRESHOLD_INFO,
    LOG_THRESHOLD_WARN,
    LOG_THRESHOLD_ERROR,
    LOG_THRESHOLD_FATAL,
    LOG_THRESHOLD_OFF,
    LOG_THRESHOLD_COUNT,
} log_threshold_t;

typedef enum {
    LOG_MODULE_SYSTEM = 0,
    LOG_MODULE_BOARD,
    LOG_MODULE_TIMEBASE,
    LOG_MODULE_TASK,
    LOG_MODULE_SCHEDULER,
    LOG_MODULE_STATE,
    LOG_MODULE_FAULT,
    LOG_MODULE_COUNT,
} log_module_t;

typedef struct {
    uint64_t timestamp_us;
    uint64_t sequence;
    log_level_t level;
    log_module_t module;
    uint16_t message_length;
    bool timestamp_valid;
    bool truncated;
    char message[LOGGING_MESSAGE_CAPACITY];
} log_record_t;

typedef enum {
    LOG_BACKEND_ACCEPTED = 0,
    LOG_BACKEND_BUSY,
    LOG_BACKEND_ERROR,
    LOG_BACKEND_RESULT_COUNT,
} logging_backend_result_t;

typedef logging_backend_result_t (*logging_backend_try_write_t)(
    const log_record_t *record,
    void *context);

typedef struct {
    logging_backend_try_write_t try_write;
    void *context;
} logging_backend_t;

typedef struct {
    uint32_t enqueued_count;
    uint32_t filtered_count;
    uint32_t dropped_count;
    uint32_t dropped_by_level[LOG_LEVEL_COUNT];
    uint32_t truncated_count;
    uint32_t format_error_count;
    uint32_t drained_count;
    uint32_t backend_busy_count;
    uint32_t backend_error_count;
} logging_statistics_t;

typedef struct {
    log_record_t records[LOGGING_QUEUE_CAPACITY];
    log_threshold_t module_thresholds[LOG_MODULE_COUNT];
    bool module_override_enabled[LOG_MODULE_COUNT];
    logging_clock_t clock;
    logging_backend_t backend;
    logging_statistics_t statistics;
    uint64_t next_sequence;
    size_t head;
    size_t count;
    log_threshold_t global_threshold;
    bool backend_attached;
    bool initialized;
} logging_system_t;

typedef enum {
    LOGGING_CLOCK_ATTACH_OK = 0,
    LOGGING_CLOCK_ATTACH_INVALID_ARGUMENT,
} logging_clock_attach_result_t;

typedef enum {
    LOGGING_CONFIG_OK = 0,
    LOGGING_CONFIG_INVALID_ARGUMENT,
} logging_config_result_t;

typedef enum {
    LOGGING_WRITE_ENQUEUED = 0,
    LOGGING_WRITE_FILTERED,
    LOGGING_WRITE_DROPPED_FULL,
    LOGGING_WRITE_FORMAT_ERROR,
    LOGGING_WRITE_INVALID_ARGUMENT,
} logging_write_result_t;

typedef enum {
    LOGGING_BACKEND_ATTACH_OK = 0,
    LOGGING_BACKEND_ATTACH_INVALID_ARGUMENT,
} logging_backend_attach_result_t;

typedef enum {
    LOGGING_DRAIN_IDLE = 0,
    LOGGING_DRAIN_ACCEPTED,
    LOGGING_DRAIN_BACKEND_BUSY,
    LOGGING_DRAIN_BACKEND_ERROR,
    LOGGING_DRAIN_NO_BACKEND,
    LOGGING_DRAIN_INVALID_STATE,
} logging_drain_result_t;

typedef enum {
    LOGGING_FORMAT_OK = 0,
    LOGGING_FORMAT_INVALID_ARGUMENT,
    LOGGING_FORMAT_BUFFER_TOO_SMALL,
    LOGGING_FORMAT_ERROR,
} logging_format_result_t;

extern logging_system_t firmware_logging_system;

void logging_initialize(void);
logging_clock_attach_result_t logging_attach_clock(logging_clock_t clock);
logging_config_result_t logging_set_global_threshold(log_threshold_t threshold);
logging_config_result_t logging_set_module_threshold(log_module_t module,
                                                     log_threshold_t threshold);
logging_config_result_t logging_clear_module_threshold(log_module_t module);
bool logging_should_emit(log_level_t level, log_module_t module);
void logging_note_filtered(log_level_t level, log_module_t module);
logging_write_result_t logging_write(log_level_t level,
                                     log_module_t module,
                                     const char *format,
                                     ...);
logging_backend_attach_result_t logging_attach_backend(
    const logging_backend_t *backend);
logging_drain_result_t logging_drain_once(void);
const log_record_t *logging_peek(void);
size_t logging_queue_count(void);
logging_format_result_t logging_format_record(const log_record_t *record,
                                              char *output,
                                              size_t output_capacity,
                                              size_t *output_length);

#define LOG_AT_LEVEL(level, module, ...)                                  \
    do {                                                                  \
        if (logging_should_emit((level), (module))) {                      \
            (void)logging_write((level), (module), __VA_ARGS__);           \
        } else {                                                          \
            logging_note_filtered((level), (module));                     \
        }                                                                 \
    } while (0)

#define LOG_DEBUG(module, ...)                                            \
    LOG_AT_LEVEL(LOG_LEVEL_DEBUG, (module), __VA_ARGS__)
#define LOG_INFO(module, ...)                                             \
    LOG_AT_LEVEL(LOG_LEVEL_INFO, (module), __VA_ARGS__)
#define LOG_WARN(module, ...)                                             \
    LOG_AT_LEVEL(LOG_LEVEL_WARN, (module), __VA_ARGS__)
#define LOG_ERROR(module, ...)                                            \
    LOG_AT_LEVEL(LOG_LEVEL_ERROR, (module), __VA_ARGS__)
#define LOG_FATAL(module, ...)                                            \
    LOG_AT_LEVEL(LOG_LEVEL_FATAL, (module), __VA_ARGS__)

#endif
