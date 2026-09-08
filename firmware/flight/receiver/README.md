# Receiver contracts

This directory owns protocol- and hardware-independent receiver data contracts.
`receiver_source.h` defines one complete decoded 16-channel frame and an
injected, non-blocking source that returns at most one frame per call.
`receiver_normalization` validates and copies channel/calibration configuration
and converts a raw frame into a timestamp-preserving normalized control
snapshot. `receiver_freshness` classifies that timestamp without owning state.

The CRSF parser/source adapter sits below this interface and owns framing, CRC
validation, and protocol values. The selected board backend separately owns
UART configuration, DMA, and interrupts. The source reports a frame only after
all 16 channels have been decoded and validated. It writes into caller-owned
storage and must not retain that pointer after returning.

The application receiver service owns timestamps, raw and normalized snapshots,
freshness configuration, and service statistics. Display formatting, USB
serialization, fault policy, motor commands, and lifecycle transitions do not
belong here.
