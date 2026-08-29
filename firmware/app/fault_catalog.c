#include "fault_catalog.h"

#include <stddef.h>

static const fault_definition_t definitions[] = {
    {
        .id = FAULT_ID_MCU_INITIALIZATION,
        .severity = FAULT_SEVERITY_CRITICAL,
        .source = FAULT_SOURCE_MCU,
    },
    {
        .id = FAULT_ID_CLOCK_CONFIGURATION,
        .severity = FAULT_SEVERITY_CRITICAL,
        .source = FAULT_SOURCE_MCU,
    },
    {
        .id = FAULT_ID_CLOCK_FREQUENCY,
        .severity = FAULT_SEVERITY_CRITICAL,
        .source = FAULT_SOURCE_MCU,
    },
    {
        .id = FAULT_ID_TIMEBASE_CONFIGURATION,
        .severity = FAULT_SEVERITY_CRITICAL,
        .source = FAULT_SOURCE_MCU,
    },
    {
        .id = FAULT_ID_TASK_REGISTRATION,
        .severity = FAULT_SEVERITY_CRITICAL,
        .source = FAULT_SOURCE_APPLICATION,
    },
    {
        .id = FAULT_ID_SCHEDULER_INITIALIZATION,
        .severity = FAULT_SEVERITY_CRITICAL,
        .source = FAULT_SOURCE_SCHEDULER,
    },
    {
        .id = FAULT_ID_SCHEDULER_RUNTIME,
        .severity = FAULT_SEVERITY_CRITICAL,
        .source = FAULT_SOURCE_SCHEDULER,
    },
    {
        .id = FAULT_ID_STATE_MACHINE_TRANSITION,
        .severity = FAULT_SEVERITY_CRITICAL,
        .source = FAULT_SOURCE_STATE_MACHINE,
    },
    {
        .id = FAULT_ID_FAULT_CLOCK_ATTACHMENT,
        .severity = FAULT_SEVERITY_CRITICAL,
        .source = FAULT_SOURCE_APPLICATION,
    },
    {
        .id = FAULT_ID_LOGGING_CLOCK_ATTACHMENT,
        .severity = FAULT_SEVERITY_FAULT,
        .source = FAULT_SOURCE_APPLICATION,
    },
    {
        .id = FAULT_ID_USB_LOGGING_INITIALIZATION,
        .severity = FAULT_SEVERITY_FAULT,
        .source = FAULT_SOURCE_USB,
    },
    {
        .id = FAULT_ID_USB_COMMAND_INITIALIZATION,
        .severity = FAULT_SEVERITY_FAULT,
        .source = FAULT_SOURCE_USB,
    },
};

const fault_definition_t *firmware_fault_catalog(size_t *definition_count)
{
    if (definition_count != NULL) {
        *definition_count = sizeof(definitions) / sizeof(definitions[0]);
    }

    return definitions;
}
