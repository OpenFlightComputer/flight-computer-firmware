#ifndef OPENFLIGHTCOMPUTER_FAULT_H
#define OPENFLIGHTCOMPUTER_FAULT_H

#include "system_state.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FAULT_SYSTEM_CAPACITY 16U
#define FAULT_DEFINITION_CAPACITY 32U
#define FAULT_ID_INVALID UINT16_C(0)

typedef uint16_t fault_id_t;
typedef uint64_t (*fault_clock_t)(void);

typedef enum {
    FAULT_SEVERITY_WARNING = 0,
    FAULT_SEVERITY_FAULT,
    FAULT_SEVERITY_CRITICAL,
    FAULT_SEVERITY_COUNT,
} fault_severity_t;

typedef enum {
    FAULT_SOURCE_APPLICATION = 0,
    FAULT_SOURCE_BOARD,
    FAULT_SOURCE_MCU,
    FAULT_SOURCE_SCHEDULER,
    FAULT_SOURCE_STATE_MACHINE,
    FAULT_SOURCE_USB,
    FAULT_SOURCE_COUNT,
} fault_source_t;

typedef struct {
    fault_id_t id;
    fault_severity_t severity;
    fault_source_t source;
} fault_definition_t;

typedef struct {
    fault_id_t id;
    fault_severity_t severity;
    fault_source_t source;
    uint64_t first_timestamp_us;
    uint64_t last_timestamp_us;
    uint32_t context;
    uint32_t occurrence_count;
    bool first_timestamp_valid;
    bool last_timestamp_valid;
    bool context_valid;
    bool active;
} fault_record_t;

typedef enum {
    FAULT_INIT_OK = 0,
    FAULT_INIT_INVALID_ARGUMENT,
    FAULT_INIT_INVALID_CATALOG,
} fault_init_result_t;

typedef enum {
    FAULT_CLOCK_ATTACH_OK = 0,
    FAULT_CLOCK_ATTACH_INVALID_ARGUMENT,
} fault_clock_attach_result_t;

typedef enum {
    FAULT_REPORT_RECORDED = 0,
    FAULT_REPORT_UPDATED,
    FAULT_REPORT_INVALID_ARGUMENT,
    FAULT_REPORT_UNKNOWN_ID,
    FAULT_REPORT_CAPACITY_EXCEEDED,
    FAULT_REPORT_CRITICAL_TRANSITION_FAILED,
} fault_report_result_t;

typedef enum {
    FAULT_CLEAR_OK = 0,
    FAULT_CLEAR_INVALID_ARGUMENT,
    FAULT_CLEAR_UNKNOWN_ID,
    FAULT_CLEAR_NOT_ACTIVE,
    FAULT_CLEAR_CRITICAL_LATCHED,
} fault_clear_result_t;

typedef struct {
    system_state_machine_t *state_machine;
    const fault_definition_t *definitions;
    size_t definition_count;
    fault_clock_t clock;
    fault_record_t records[FAULT_SYSTEM_CAPACITY];
    size_t active_count;
    uint32_t dropped_record_count;
    uint32_t critical_transition_failure_count;
    bool initialized;
} fault_system_t;

fault_init_result_t fault_system_initialize(
    fault_system_t *system,
    system_state_machine_t *state_machine,
    const fault_definition_t *definitions,
    size_t definition_count);
fault_clock_attach_result_t fault_system_attach_clock(fault_system_t *system,
                                                      fault_clock_t clock);
fault_report_result_t fault_system_report(fault_system_t *system,
                                          fault_id_t id,
                                          bool context_valid,
                                          uint32_t context);
fault_clear_result_t fault_system_clear(fault_system_t *system, fault_id_t id);
const fault_record_t *fault_system_record_for_id(const fault_system_t *system,
                                                fault_id_t id);
size_t fault_system_active_count(const fault_system_t *system);
const char *fault_severity_name(fault_severity_t severity);
const char *fault_source_name(fault_source_t source);

#endif
