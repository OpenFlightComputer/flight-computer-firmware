#include "fault.h"
#include "health.h"
#include "system_state.h"
#include "usb_health_response.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define RESPONSE_CAPACITY 768U

static uint64_t current_time_us;

static uint64_t fake_clock(void)
{
    return current_time_us;
}

static void initialize_system(system_state_machine_t *state_machine,
                              fault_system_t *fault_system,
                              const fault_definition_t *definitions,
                              size_t definition_count)
{
    system_state_machine_initialize(state_machine);
    assert(fault_system_initialize(fault_system,
                                   state_machine,
                                   definitions,
                                   definition_count) == FAULT_INIT_OK);
}

static void empty_health_response_is_exact(void)
{
    static const fault_definition_t definitions[] = {
        {1U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    };
    static const char expected[] =
        "{\"type\":\"response\",\"command\":\"health\",\"ok\":true,"
        "\"health\":\"OK\",\"state\":\"BOOT\","
        "\"fault_data_complete\":true,\"active_fault_count\":0,"
        "\"warning_count\":0,\"fault_count\":0,\"critical_count\":0,"
        "\"dropped_fault_count\":0,\"faults\":[],"
        "\"reported_fault_count\":0,\"truncated\":false}\n";
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    health_summary_t summary;
    char response[RESPONSE_CAPACITY];
    size_t length;

    initialize_system(&state_machine, &fault_system, definitions, 1U);
    assert(health_evaluate(&fault_system, &summary) == HEALTH_EVALUATE_OK);
    assert(usb_health_response_build(&summary,
                                     &fault_system,
                                     response,
                                     sizeof(response),
                                     &length));
    assert(length == sizeof(expected) - 1U);
    assert(memcmp(response, expected, length) == 0);
}

static void active_fault_metadata_is_serialized(void)
{
    static const fault_definition_t definitions[] = {
        {7U, FAULT_SEVERITY_FAULT, FAULT_SOURCE_USB},
    };
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    health_summary_t summary;
    char response[RESPONSE_CAPACITY];
    size_t length;

    initialize_system(&state_machine, &fault_system, definitions, 1U);
    assert(fault_system_attach_clock(&fault_system, fake_clock) ==
           FAULT_CLOCK_ATTACH_OK);
    current_time_us = 42U;
    assert(fault_system_report(&fault_system, 7U, false, 0U) ==
           FAULT_REPORT_RECORDED);
    current_time_us = 84U;
    assert(fault_system_report(&fault_system, 7U, true, 99U) ==
           FAULT_REPORT_UPDATED);
    assert(health_evaluate(&fault_system, &summary) == HEALTH_EVALUATE_OK);
    assert(usb_health_response_build(&summary,
                                     &fault_system,
                                     response,
                                     sizeof(response),
                                     &length));
    assert(strstr(response, "\"health\":\"DEGRADED\"") != NULL);
    assert(strstr(response,
                  "{\"id\":7,\"severity\":\"FAULT\","
                  "\"source\":\"USB\",\"occurrence_count\":2,"
                  "\"first_timestamp_us\":42,"
                  "\"last_timestamp_us\":84,\"context\":99}") != NULL);
    assert(strstr(response, "\"reported_fault_count\":1") != NULL);
    assert(strstr(response, "\"truncated\":false}\n") != NULL);
    assert(length == strlen(response));
}

static void response_truncation_is_explicit_and_valid(void)
{
    fault_definition_t definitions[FAULT_SYSTEM_CAPACITY];
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    health_summary_t summary;
    char response[RESPONSE_CAPACITY];
    size_t length;
    size_t index;

    for (index = 0U; index < FAULT_SYSTEM_CAPACITY; index++) {
        definitions[index] = (fault_definition_t){
            .id = (fault_id_t)(index + 1U),
            .severity = FAULT_SEVERITY_WARNING,
            .source = FAULT_SOURCE_APPLICATION,
        };
    }
    initialize_system(&state_machine,
                      &fault_system,
                      definitions,
                      FAULT_SYSTEM_CAPACITY);
    for (index = 0U; index < FAULT_SYSTEM_CAPACITY; index++) {
        assert(fault_system_report(&fault_system,
                                   definitions[index].id,
                                   false,
                                   0U) == FAULT_REPORT_RECORDED);
    }
    assert(health_evaluate(&fault_system, &summary) == HEALTH_EVALUATE_OK);
    assert(usb_health_response_build(&summary,
                                     &fault_system,
                                     response,
                                     sizeof(response),
                                     &length));
    assert(length < sizeof(response));
    assert(response[length - 1U] == '\n');
    assert(strstr(response, "\"active_fault_count\":16") != NULL);
    assert(strstr(response, "\"truncated\":true}\n") != NULL);
    assert(strstr(response, "\"reported_fault_count\":16") == NULL);
}

static void incomplete_data_and_worst_case_metadata_are_representable(void)
{
    static const fault_definition_t definitions[] = {
        {UINT16_MAX, FAULT_SEVERITY_FAULT, FAULT_SOURCE_STATE_MACHINE},
    };
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    health_summary_t summary;
    char response[RESPONSE_CAPACITY];
    size_t length;

    initialize_system(&state_machine, &fault_system, definitions, 1U);
    assert(fault_system_report(&fault_system, UINT16_MAX, true, UINT32_MAX) ==
           FAULT_REPORT_RECORDED);
    fault_system.records[0].first_timestamp_valid = true;
    fault_system.records[0].last_timestamp_valid = true;
    fault_system.records[0].first_timestamp_us = UINT64_MAX;
    fault_system.records[0].last_timestamp_us = UINT64_MAX;
    fault_system.records[0].occurrence_count = UINT32_MAX;
    fault_system.dropped_record_count = 1U;

    assert(health_evaluate(&fault_system, &summary) == HEALTH_EVALUATE_OK);
    assert(summary.state == HEALTH_STATE_UNKNOWN);
    assert(usb_health_response_build(&summary,
                                     &fault_system,
                                     response,
                                     sizeof(response),
                                     &length));
    assert(strstr(response, "\"health\":\"UNKNOWN\"") != NULL);
    assert(strstr(response, "\"fault_data_complete\":false") != NULL);
    assert(strstr(response, "\"id\":65535") != NULL);
    assert(strstr(response, "\"source\":\"STATE_MACHINE\"") != NULL);
    assert(strstr(response, "18446744073709551615") != NULL);
    assert(strstr(response, "\"context\":4294967295") != NULL);
    assert(strstr(response, "\"truncated\":false}\n") != NULL);
    assert(length == strlen(response));
}

static void invalid_or_small_destinations_are_rejected(void)
{
    static const fault_definition_t definitions[] = {
        {1U, FAULT_SEVERITY_WARNING, FAULT_SOURCE_APPLICATION},
    };
    system_state_machine_t state_machine;
    fault_system_t fault_system;
    health_summary_t summary;
    char response[16];
    size_t length = 1U;

    initialize_system(&state_machine, &fault_system, definitions, 1U);
    assert(health_evaluate(&fault_system, &summary) == HEALTH_EVALUATE_OK);
    assert(!usb_health_response_build(&summary,
                                      &fault_system,
                                      response,
                                      sizeof(response),
                                      &length));
    assert(length == 0U);
    assert(!usb_health_response_build(NULL,
                                      &fault_system,
                                      response,
                                      sizeof(response),
                                      &length));
}

int main(void)
{
    empty_health_response_is_exact();
    active_fault_metadata_is_serialized();
    response_truncation_is_explicit_and_valid();
    incomplete_data_and_worst_case_metadata_are_representable();
    invalid_or_small_destinations_are_rejected();
    return 0;
}
