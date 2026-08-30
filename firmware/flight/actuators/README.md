# Actuator contracts

This directory contains hardware-independent data and policy shared by future
actuator producers and consumers. `motor_command` defines an atomic normalized
four-motor snapshot, validation, exact-stop canonicalization, invalidation, and
freshness checks.

DShot framing belongs in a later hardware-independent peripheral protocol
module. Timer, DMA, and routed-pin implementations belong below the board and
MCU hardware boundaries. This milestone performs no output.
