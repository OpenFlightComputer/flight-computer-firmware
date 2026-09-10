#ifndef OPENFLIGHTCOMPUTER_APPLICATION_STATE_H
#define OPENFLIGHTCOMPUTER_APPLICATION_STATE_H

#include "boot_status.h"
#include "fault.h"
#include "flight_configuration_service.h"
#include "imu_service.h"
#include "gyro_calibration.h"
#include "receiver_arming.h"
#include "receiver_failsafe.h"
#include "receiver_service.h"
#include "scheduler.h"
#include "system_state.h"
#include "task.h"
#include "usb_command_processor.h"

#include <stdbool.h>
#include <stdint.h>

extern task_registry_t firmware_task_registry;
extern scheduler_t firmware_scheduler;
extern system_state_machine_t firmware_system_state_machine;
extern fault_system_t firmware_fault_system;
extern usb_command_processor_t firmware_usb_command_processor;
extern receiver_service_t firmware_receiver_service;
extern receiver_arming_t firmware_receiver_arming;
extern receiver_failsafe_t firmware_receiver_failsafe;
extern receiver_failsafe_decision_t firmware_receiver_failsafe_decision;
extern flight_configuration_service_t firmware_flight_configuration_service;
extern imu_service_t firmware_imu_service;
extern gyro_calibration_t firmware_gyro_calibration;

extern volatile uint32_t firmware_flight_control_task_executions;
extern volatile uint32_t firmware_flight_control_submit_last_result;
extern volatile uint32_t firmware_receiver_dma_overruns;
extern volatile uint32_t firmware_receiver_dma_dropped_bytes;
extern volatile uint32_t firmware_receiver_failsafe_state;
extern volatile uint32_t firmware_receiver_failsafe_action;
extern volatile uint64_t firmware_receiver_link_age_us;
extern volatile uint32_t firmware_receiver_failsafe_transitions;
extern volatile bool firmware_receiver_stage_two_latched;
extern volatile bool firmware_receiver_recovery_ready;
extern volatile uint32_t firmware_high_rate_worst_case_budget_us;
extern volatile uint32_t firmware_high_rate_worst_case_utilization_permille;

#endif
