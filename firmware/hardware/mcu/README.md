# MCU support

MCU implementations provide processor startup and low-level capabilities without knowledge of aircraft behavior or external-device protocols.

The current application selects `stm32f405`. A future MCU directory should implement equivalent narrow capabilities rather than forcing application modules to branch on processor types.
