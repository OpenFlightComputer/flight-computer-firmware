# Actuator contracts

This directory contains hardware-independent data and policy shared by future
actuator producers and consumers. `motor_command` defines an atomic normalized
four-motor snapshot, validation, exact-stop canonicalization, invalidation, and
freshness checks. `motor_mapping` owns an atomic logical-to-physical
permutation, disarmed/stopped configuration preconditions, and complete-command
reordering. `motor_output` defines the instance-based facade and injected
backend contract for initialization, complete-command submission, and
unconditional force-stop.

DShot framing belongs in a later hardware-independent peripheral protocol
module. An application-owned adapter will bridge that lower API to this facade
without making the peripheral depend upward on flight types. Timer, DMA, and
routed-pin implementations belong below the board and MCU hardware boundaries.
The mapping and facade remain unconnected to application state and have only
host-test consumers. They perform no production output. A future
application-owned adapter must derive the mapping configuration booleans from
the real lifecycle and force-stop results; callers must not assert them
optimistically.
