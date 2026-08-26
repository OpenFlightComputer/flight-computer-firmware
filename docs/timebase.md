# Monotonic microsecond timebase

Milestone 0.4 provides one firmware-wide clock:

```c
uint64_t time_us(void);
```

The value is elapsed microseconds since the timebase was started during
`board_initialize()`. It has no wall-clock or calendar meaning. Callers may
subtract two returned values with unsigned arithmetic to measure elapsed time.

## Flight Computer V1 selection

Flight Computer V1 reserves STM32F405 TIM5 as an internal, free-running
timebase. TIM5 does not use a GPIO pin, capture channel, compare channel, or
DMA stream in this role.

The system clock is 168 MHz and APB1 is divided by four, making PCLK1 42 MHz.
STM32F405 timer clocks are doubled when their APB prescaler is not one, so
TIM5 receives 84 MHz. The board requests a 1 MHz counter and the MCU backend
programs a prescaler divisor of 84 (`PSC = 83`). Each counter increment is
therefore exactly one microsecond.

TIM5 is a 32-bit timer and is allowed to count from zero through
`0xffffffff`. Its hardware counter wraps every `2^32` microseconds, or about
71.58 minutes.

## Extending TIM5 to 64 bits

The TIM5 update interrupt increments a volatile 32-bit software overflow
word. A normal timestamp combines that upper word with the TIM5 counter:

```text
(overflow count << 32) | TIM5 counter
```

An interrupt can occur while those two pieces are being read. `time_us()`
therefore captures the overflow word before and after reading the timer. If
the words differ, it retries. It also checks TIM5's update-pending flag. When
hardware has wrapped but the interrupt handler has not yet run, the read adds
the pending overflow itself and uses a second counter sample taken after the
flag check.

TIM5 uses interrupt priority zero. Its handler only clears the update flag and
increments the overflow word, and it runs once per 71.58 minutes. The highest
configurable priority prevents another firmware interrupt that calls
`time_us()` from observing the handler halfway through those two operations.

The resulting 64-bit microsecond value wraps after roughly 584,500 years.
Correct wrap tracking requires that global interrupt masking remain shorter
than one complete 71.58-minute TIM5 period, ensuring that at most one overflow
can be awaiting service. Normal bounded critical sections are many orders of
magnitude shorter than that limit.

## Initialization and failures

Board initialization supplies and owns these V1 choices:

- expected TIM5 input clock: 84 MHz;
- counter frequency: 1 MHz;
- TIM5 interrupt priority: zero.

The STM32 backend independently reads the active APB1 clock and prescaler. It
rejects a clock mismatch, a zero or non-integral requested frequency, a
prescaler outside the timer's 16-bit range, or an invalid interrupt priority.
Board initialization reports any such failure as
`BOARD_INIT_TIMEBASE_CONFIGURATION_ERROR`; the application exposes boot status
`103` and halts.

## Verification boundary

Native tests execute the same snapshot resolver used by the firmware. They
cover ordinary reads, a hardware wrap pending before interrupt service, an
interrupt occurring during a read, and monotonic progression across the
32-bit boundary.

Debug and Release builds verify integration, interrupt-vector ownership, and
retained symbols. Physical frequency accuracy and continuous operation across
a real TIM5 overflow remain board checks; no powered board/ST-Link session was
available during implementation.
