# Boards

A board implementation provides the generic `board.h` API selected by the build. Application code includes that API without knowing the PCB revision, MCU family, HAL, pins, or peripheral instances.

Board code may depend on its selected MCU implementation. MCU code must not depend on a board directory.
