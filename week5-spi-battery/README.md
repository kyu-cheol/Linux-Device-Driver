# Week 5 — SPI Battery Device

Adds a QEMU SSI battery peripheral and a Linux SPI driver. The driver exposes the battery percentage through sysfs and checks the value periodically, requesting shutdown below the configured threshold.

## Files

- `qemu/comento_soc.c` — QEMU board setup with SPI controller and peripheral wiring.
- `qemu/battery_device.c` — SSI battery model and configurable percentage property.
- `linux/battery_driver.c` — SPI transfers, sysfs attribute, timer, and workqueue.
- `device-tree/comento.dts` — SPI controller, chip select, and battery device declarations.

## Integration

Integrate these sources into compatible QEMU and Linux trees and build the matching DTS. The provided execution screenshot shows sysfs values changing and shutdown at 3%; the full build instructions were not included in the archive.
