# Actuator contracts

This directory contains hardware-independent data and policy shared by future
actuator producers and consumers. `motor_command` defines an atomic normalized
four-motor snapshot, validation, exact-stop canonicalization, invalidation, and
freshness checks. `motor_mapping` owns an atomic logical-to-physical
permutation, disarmed/stopped configuration preconditions, and complete-command
reordering. `motor_output` defines the instance-based facade and injected
backend contract for initialization, complete-command submission, and
unconditional force-stop.

DShot framing belongs in the hardware-independent peripheral protocol module.
The application-owned motor controller will bridge that lower API to this facade
without making the peripheral depend upward on flight types. Timer, DMA, and
routed-pin implementations belong below the board and MCU hardware boundaries.
The mapping and facade remain independently testable mechanism layers and
perform no physical output. `app/motor_control` privately owns their one
production instance, derives mapping conditions from real lifecycle and
force-stop results, and is the only production module permitted to submit raw
motor output.
