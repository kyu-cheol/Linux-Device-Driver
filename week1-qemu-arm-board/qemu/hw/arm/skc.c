#include "qemu/osdep.h"
#include "qemu/datadir.h"
#include "qemu/units.h"
#include "qemu/option.h"
#include "qemu/module.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/sysbus.h"
#include "hw/loader.h"
#include "hw/qdev-properties.h"
#include "hw/arm/boot.h"
#include "sysemu/runstate.h"
#include "sysemu/sysemu.h"
#include "target/arm/cpu.h"
#include "hw/intc/arm_gicv3_common.h"
#include "hw/char/pl011.h"

#define NUM_IRQS 256                       // SPI INT NUM
#define ARCH_TIMER_NS_EL1_IRQ 14
#define COMENTO_IRQ_UART 0

#define COMENTO_BASE_MEM 0x80000000
#define COMENTO_BASE_GIC_REDIST 0x03000000
#define COMENTO_BASE_GIC_DIST 0x04000000
#define COMENTO_SIZE_GIC_REDIST 0x01000000
#define COMENTO_BASE_UART 0x04010000
#define COMENTO_BASE_UART2 0x04020000

#define TYPE_COMENTO_MACHINE MACHINE_TYPE_NAME("skc")
#define COMENTO_CPU_TYPE ARM_CPU_TYPE_NAME("cortex-a72")

struct ComentoMachineClass {
        MachineClass parent;
};

struct ComentoMachineState {
        MachineState parent;
        DeviceState *gic;
        struct arm_boot_info bootinfo;
};
OBJECT_DECLARE_TYPE(ComentoMachineState, ComentoMachineClass, COMENTO_MACHINE);

static void create_gic(ComentoMachineState *vms);
static void create_uart(const ComentoMachineState *vms, MemoryRegion *mem);
static void create_uart2(const ComentoMachineState *vms, MemoryRegion *mem);

static void machcomento_init(MachineState *machine){
        int index;
        unsigned int smp_cpus = machine->smp.cpus;                
        ComentoMachineState *vms = COMENTO_MACHINE(machine);
        MemoryRegion *sysmem = get_system_memory();

// ================================== Memory init ======================================        
        memory_region_add_subregion(sysmem, COMENTO_BASE_MEM, machine->ram);
        vms->bootinfo.ram_size = machine->ram_size;
        vms->bootinfo.loader_start = COMENTO_BASE_MEM;
// =====================================================================================

// ================================== CPU init =========================================
        for (index = 0; index < smp_cpus; index++) {
                Object *cpuobj;
                CPUState *cs;
                cpuobj = object_new(COMENTO_CPU_TYPE);
                object_property_set_bool(cpuobj, "has_el3", false, NULL);  // trustzone not use
                object_property_set_bool(cpuobj, "has_el2", false, NULL);  // hypevisor not use
                if (object_property_find(cpuobj, "reset-cbar")) {
                        object_property_set_int(cpuobj, "reset-cbar", COMENTO_BASE_GIC_DIST, &error_abort);
                }                                                          // regist GIC address to system reg(CBAR)
                object_property_set_link(cpuobj, "memory", OBJECT(sysmem), &error_abort); // link CPU and MEM
                qdev_realize(DEVICE(cpuobj), NULL, &error_fatal);
                cs = CPU(cpuobj);
                cs->cpu_index = index;
                object_unref(cpuobj);
        }
        vms->bootinfo.psci_conduit = QEMU_PSCI_CONDUIT_HVC;                // PSCI at EL2
// ======================================================================================

// =================================== GIC init =========================================        
        create_gic(vms);
// ======================================================================================

// ================================== Uart init =========================================        
        create_uart(vms, sysmem);
        create_uart2(vms, sysmem);
// ======================================================================================
        
        arm_load_kernel(ARM_CPU(first_cpu), machine, &vms->bootinfo);
}

static void create_gic(ComentoMachineState *vms)
{
        MachineState *ms = MACHINE(vms);
        SysBusDevice *gicbusdev;
        unsigned int smp_cpus = ms->smp.cpus;

        vms->gic = qdev_new("arm-gicv3");

        qdev_prop_set_uint32(vms->gic, "revision", 3);
        qdev_prop_set_uint32(vms->gic, "num-cpu", smp_cpus);       // notify cpu num to GIC
        qdev_prop_set_uint32(vms->gic, "num-irq", NUM_IRQS + 32);  // notify irq(spi, ppi, sgi) num to GIC

        uint32_t redist_capacity = COMENTO_SIZE_GIC_REDIST / GICV3_REDIST_SIZE; // able 128
        uint32_t redist_count = MIN(smp_cpus, redist_capacity);                 // use 2
        qdev_prop_set_uint32(vms->gic, "len-redist-region-count", 1);
        qdev_prop_set_uint32(vms->gic, "redist-region-count[0]", redist_count);

        gicbusdev = SYS_BUS_DEVICE(vms->gic);
        sysbus_realize_and_unref(gicbusdev, &error_fatal);
        sysbus_mmio_map(gicbusdev, 0, COMENTO_BASE_GIC_DIST);     // memory mapping distributer
        sysbus_mmio_map(gicbusdev, 1, COMENTO_BASE_GIC_REDIST);   // memory mapping redistributer

        int i;
        for (i = 0; i < smp_cpus; i++) {
                DeviceState *cpudev = DEVICE(qemu_get_cpu(i));
                int ppibase = NUM_IRQS + i * GIC_INTERNAL + GIC_NR_SGIS;

                qdev_connect_gpio_out(cpudev, 0, qdev_get_gpio_in(vms->gic, ppibase + ARCH_TIMER_NS_EL1_IRQ));
                sysbus_connect_irq(gicbusdev, i, qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
                sysbus_connect_irq(gicbusdev, i + smp_cpus, qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
        }
}

static void create_uart(const ComentoMachineState *vms, MemoryRegion *mem)
{
        hwaddr base = COMENTO_BASE_UART;
        int irq = COMENTO_IRQ_UART;
        DeviceState *dev = qdev_new(TYPE_PL011);
        SysBusDevice *s = SYS_BUS_DEVICE(dev);

        qdev_prop_set_chr(dev, "chardev", serial_hd(0));
        sysbus_realize_and_unref(s, &error_fatal);

        memory_region_add_subregion(mem, base, sysbus_mmio_get_region(s, 0));
        sysbus_connect_irq(s, 0, qdev_get_gpio_in(vms->gic, irq));
}

static void create_uart2(const ComentoMachineState *vms, MemoryRegion *mem)
{
        hwaddr base = COMENTO_BASE_UART2;
        int irq = COMENTO_IRQ_UART;
        DeviceState *dev = qdev_new(TYPE_PL011);
        SysBusDevice *s = SYS_BUS_DEVICE(dev);

        qdev_prop_set_chr(dev, "chardev", serial_hd(1));
        sysbus_realize_and_unref(s, &error_fatal);

        memory_region_add_subregion(mem, base, sysbus_mmio_get_region(s, 0));
        sysbus_connect_irq(s, 0, qdev_get_gpio_in(vms->gic, irq));
}

static void comento_machine_class_init(ObjectClass *oc, void *data){
        MachineClass *mc = MACHINE_CLASS(oc);
        mc->desc = "Comento board";
        mc->init = machcomento_init;
        mc->max_cpus = 128;
        mc->block_default_type = IF_SD;
        mc->no_cdrom = 1;
        mc->minimum_page_bits = 12;
        mc->default_ram_id = "mach-coemento.ram";
}

static const TypeInfo comento_machine_info = {
        .name          = "skc-machine",
        .parent        = TYPE_MACHINE,
        .instance_size = sizeof(ComentoMachineState),
        .class_size    = sizeof(ComentoMachineClass),
        .class_init    = comento_machine_class_init,
};

static void machcomento_machine_init(void){
        type_register_static(&comento_machine_info);
}

type_init(machcomento_machine_init);
