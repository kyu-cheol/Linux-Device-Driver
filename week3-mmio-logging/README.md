# Week 3 — MMIO Logging Device

Models a custom QEMU SysBus peripheral and provides a Linux AMBA driver that exposes a character device. The driver includes DMA-based transmit handling and an ioctl to clear the log.

## Files

- `qemu/logging_dev.c` — QEMU MMIO register model and log output.
- `linux/logging_dev_driver.c` — Linux AMBA, character-device, DMAengine, and interrupt code.
- `device-tree/comento.dts` — peripheral address, interrupt, clock, and DMA declarations.

## Integration

Add the QEMU model and kernel driver to compatible source trees, register their build entries, and use the matching DTS. The complete QEMU/Linux trees and build scripts are not included.
