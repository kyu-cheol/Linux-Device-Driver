# QEMU SoC Linux Device Driver Porting

## Overview

This repository collects exercises on modeling an ARM-based virtual SoC in QEMU and implementing Linux interfaces for its peripherals. The exercises cover QEMU machine setup, Device Tree, MMIO, AMBA, GPIO, DMA, SPI, Linux system calls, and character devices.

The directories are organized by assignment. They are source snapshots from separate exercises; this repository does not include complete QEMU or Linux kernel source trees and is not a single, fully integrated build.

## Development Environment

- QEMU ARM system emulation
- ARM Cortex-A72 virtual CPU
- Linux Kernel
- C
- Device Tree Source (DTS)
- Linux AMBA, GPIO, DMAengine, and SPI APIs
- libgpiod (legacy line API used by the sample)

Exact upstream versions and build configuration were not included with the exercise archives.

## Implemented Drivers and Exercises

| Directory | Scope |
| --- | --- |
| `week1-qemu-arm-board/` | QEMU ARM machine, GICv3, memory, and dual PL011 UART setup |
| `week3-mmio-logging/` | QEMU MMIO logging peripheral and Linux AMBA/DMA driver |
| `week4-gpio-seven-segment/` | GPIO, LEDs, button IRQ, QEMU 7-segment model, and libgpiod sample |
| `week5-spi-battery/` | QEMU SSI battery peripheral and Linux SPI/sysfs driver |
| `assignment1-kernel-syscalls/` | Kernel integer queue exposed through enqueue/dequeue system calls |
| `assignment2-ring-buffer-char-device/` | Linux character device backed by a 4096-byte ring buffer |

## Project Structure

```text
.
├── README.md
├── week1-qemu-arm-board/
│   ├── qemu/hw/arm/skc.c
│   └── device-tree/skc.dts
├── week3-mmio-logging/
│   ├── qemu/logging_dev.c
│   ├── linux/logging_dev_driver.c
│   └── device-tree/comento.dts
├── week4-gpio-seven-segment/
│   ├── qemu/comento_soc.c
│   ├── qemu/seven_segment.c
│   ├── linux/logging_dev_driver.c
│   ├── userspace/seven_seg_sample.c
│   └── device-tree/comento.dts
├── week5-spi-battery/
│   ├── qemu/comento_soc.c
│   ├── qemu/battery_device.c
│   ├── linux/battery_driver.c
│   └── device-tree/comento.dts
├── assignment1-kernel-syscalls/
│   ├── linux/kernel/queue_syscalls.c
│   ├── linux/include/uapi/asm-generic/unistd.h
│   └── userspace/queue_app.c
└── assignment2-ring-buffer-char-device/
    └── linux/drivers/ringbuffer/main.c
```

## Build & Run

The archives contain exercise source files, not complete QEMU/Linux build trees, kernel configurations, toolchain files, or integration scripts. As a result, there is no repository-wide one-command build or run procedure.

To use an exercise:

1. Check its `README.md` for the QEMU/Linux integration points.
2. Apply the files to compatible QEMU and Linux source trees.
3. Build QEMU, the kernel, and the Device Tree using the configuration for the target exercise.
4. Boot the resulting image in QEMU and run the corresponding userspace sample where provided.

For the userspace samples, build with the matching target toolchain. `seven_seg_sample.c` uses the libgpiod v1 line API; install a compatible development package and link it when compiling. The queue sample requires a kernel image with the assignment's system-call implementation and syscall numbers.

## Verification Notes

- The supplied week 1 screenshot shows data written to `/dev/ttyAMA1` arriving at a host `netcat` listener.
- The supplied week 5 screenshot shows the battery sysfs value changing from 100 to 53, 30, and 3, followed by shutdown at the low-battery threshold.
- These screenshots are not included in this repository. Source code is available for the GPIO/7-segment and kernel queue/ring-buffer exercises; separate execution evidence was not included for every exercise.

## References

- Comento, *개발자를 위한 시스템 반도체 SW개발 기초 (디바이스 드라이버 개발)*, course exercise archives.
- `week1_과제.zip`, `3주차 과제.Zip`, `4주차 과제.Zip`, `5주차 과제.Zip`.
- `assignment1.zip` and `assignment2.zip`.

## License

This repository currently contains a GPL-2.0 license file. The exercise files originate from course materials; check their original notices and redistribution terms before redistributing them.
