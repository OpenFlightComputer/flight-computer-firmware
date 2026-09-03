# Common firmware utilities

This directory contains small allocation-free, hardware-independent utilities
that may be used by application, flight, peripheral, and hardware layers. It
must not depend on any of those higher-level modules.

`uint64_decimal` provides bounded decimal conversion without relying on
embedded `printf` long-long support. Its optional minimum width preserves the
canonical logger's zero padding, and its maximum output is 20 digits plus a
terminating null byte.
