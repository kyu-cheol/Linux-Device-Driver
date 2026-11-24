# Week 4 — GPIO and 7-Segment Display

Extends the QEMU board with GPIO controllers, LEDs, a button, and a seven-segment display model. Includes a Linux GPIO button interrupt path and a userspace libgpiod sample that drives digit patterns.

## Files

- `qemu/comento_soc.c` — QEMU board and GPIO/peripheral wiring.
- `qemu/seven_segment.c` — terminal-rendered seven-segment QEMU device.
- `linux/logging_dev_driver.c` — logging driver variant with button GPIO/IRQ handling.
- `userspace/seven_seg_sample.c` — GPIO line control example for digits 0–9.
- `device-tree/comento.dts` — GPIO and peripheral declarations.

## Integration

The userspace sample uses the libgpiod v1 line API and expects `/dev/gpiochip1`. QEMU and Linux build-system integration files are not included.
