# Actuator contracts

This directory contains hardware-independent data and policy shared by future
actuator producers and consumers. `motor_command` defines an atomic normalized
four-motor snapshot, validation, exact-stop canonicalization, invalidation, and
freshness checks. `motor_output` defines the instance-based facade and injected
backend contract for initialization, complete-command submission, and
unconditional force-stop.

DShot framing belongs in a later hardware-independent peripheral protocol
module. An application-owned adapter will bridge that lower API to this facade
without making the peripheral depend upward on flight types. Timer, DMA, and
routed-pin implementations belong below the board and MCU hardware boundaries.
The current facade has only a host-test fake backend and performs no production
output.
