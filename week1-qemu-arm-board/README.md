# Week 1 — QEMU ARM Board

Adds an ARM Cortex-A72 machine to QEMU with RAM, GICv3, and two PL011 UARTs. The DTS describes the CPU, memory, PSCI, interrupt controller, timer, and UART nodes.

## Files

- `qemu/hw/arm/skc.c` — machine initialization and CPU/GIC/UART wiring.
- `device-tree/skc.dts` — Device Tree description for the guest kernel.

## Integration

Apply the machine source to a compatible QEMU tree and register it with that tree's build system. Compile the DTS with the kernel's Device Tree build tools. The archive does not include a QEMU build configuration or a boot command.
