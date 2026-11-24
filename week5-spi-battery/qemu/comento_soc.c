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
#include "monitor/qdev.h"
#include "hw/sd/sd.h"
#include "hw/ssi/ssi.h"

#define NUM_IRQS 256                       // SPI INT NUM
#define ARCH_TIMER_NS_EL1_IRQ 14
#define COMENTO_IRQ_UART 0
#define COMENTO_IRQ_MMIO 1
//#define COMENTO_IRQ_LOGGING 2
#define COMENTO_IRQ_MMC 3                 // use SPI 3, SPI 4
#define COMENTO_IRQ_GPIOA 5
#define COMENTO_IRQ_GPIOB 6
#define COMENTO_IRQ_SPI 7
#define COMENTO_IRQ_DMA 15

#define COMENTO_BASE_MEM 0x40000000
#define COMENTO_BASE_GIC_REDIST 0x08000000
#define COMENTO_BASE_GIC_DIST 0x09000000
#define COMENTO_SIZE_GIC_REDIST 0x01000000
#define COMENTO_BASE_UART 0x09010000
#define COMENTO_BASE_MMIO 0x09011000
#define COMENTO_BASE_LOGGING 0X09012000
#define COMENTO_BASE_DMA 0x09013000
#define COMENTO_BASE_MMC 0X09014000
#define COMENTO_BASE_GPIOA 0x09015000
#define COMENTO_BASE_GPIOB 0x09016000
#define COMENTO_BASE_SPI 0x09017000

#define TYPE_COMENTO_MACHINE MACHINE_TYPE_NAME("comento")
#define COMENTO_CPU_TYPE ARM_CPU_TYPE_NAME("cortex-a72")

struct ComentoMachineClass {
        MachineClass parent;
};

struct ComentoMachineState {
        MachineState parent;
        DeviceState *gic;
        DeviceState *dma;
        DeviceState *gpioa;
        DeviceState *gpiob;
        DeviceState *spi;
        struct arm_boot_info bootinfo;
};
OBJECT_DECLARE_TYPE(ComentoMachineState, ComentoMachineClass, COMENTO_MACHINE);

static void create_gic(ComentoMachineState *vms);
static void create_uart(const ComentoMachineState *vms, MemoryRegion *mem);
static void create_mmio(const ComentoMachineState *vms, MemoryRegion *mem);
static void create_logging(const ComentoMachineState *vms, MemoryRegion *mem);
static void create_mmc(const ComentoMachineState *vms);
static void create_dma(ComentoMachineState *vms, MemoryRegion *mem);
static void create_gpioa(ComentoMachineState *vms, MemoryRegion *mem);
static void create_gpiob(ComentoMachineState *vms, MemoryRegion *mem);
static void create_leds_and_button(ComentoMachineState *vms);
static void create_seven_segment(ComentoMachineState *vms);
static void create_spi(ComentoMachineState *vms, MemoryRegion *mem);
static void create_spi_devs(ComentoMachineState *vms);

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

// =================================== DMA init =========================================
        create_dma(vms, sysmem);
// ======================================================================================
        
// ================================== Uart init =========================================        
        create_uart(vms, sysmem);
// ======================================================================================

// ================================== MMIO init ========================================= 
        create_mmio(vms, sysmem);
        create_logging(vms, sysmem);
// ======================================================================================

// =================================== MMC init =========================================
        create_mmc(vms);
// ======================================================================================

// =================================== GPIO init ========================================
        create_gpioa(vms, sysmem);
        create_leds_and_button(vms);

        create_gpiob(vms, sysmem);
        create_seven_segment(vms);
// ======================================================================================

// ==================================== SPI init ========================================
        create_spi(vms, sysmem);
        create_spi_devs(vms);        // 2 scale measurement sensors and battery chip.
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

static void create_mmio(const ComentoMachineState *vms, MemoryRegion *mem)
{
        hwaddr base = COMENTO_BASE_MMIO;
        int irq = COMENTO_IRQ_MMIO;
        char name[] = "mmio-comento";
        DeviceState *dev = qdev_new(name);
        SysBusDevice *s = SYS_BUS_DEVICE(dev);

        qdev_set_id(dev, name, NULL);

        sysbus_realize_and_unref(s, &error_fatal);
        memory_region_add_subregion(mem, base, sysbus_mmio_get_region(s, 0));
        sysbus_connect_irq(s, 0, qdev_get_gpio_in(vms->gic, irq));
        sysbus_connect_irq(s, 1, qdev_get_gpio_in(vms->dma, 0));
        sysbus_connect_irq(s, 2, qdev_get_gpio_in(vms->dma, 1));
}                          

static void create_logging(const ComentoMachineState *vms, MemoryRegion *mem)
{
        hwaddr base = COMENTO_BASE_LOGGING;
        //int irq = COMENTO_IRQ_LOGGING;
        char name[] = "logging-comento";
        DeviceState *dev = qdev_new(name);
        SysBusDevice *s = SYS_BUS_DEVICE(dev);

        qdev_set_id(dev, name, NULL);

        sysbus_realize_and_unref(s, &error_fatal);
        memory_region_add_subregion(mem, base, sysbus_mmio_get_region(s, 0));
        //sysbus_connect_irq(s, 0, qdev_get_gpio_in(vms->gic, irq));
        //sysbus_connect_irq(s, 1, qdev_get_gpio_in(vms->dma, 2));   // dma0, dma1 : mmio
                                                                     // dma3       : uart
}                          

static void create_dma(ComentoMachineState *vms, MemoryRegion *mem)
{
        hwaddr base = COMENTO_BASE_DMA;
        int irq = COMENTO_IRQ_DMA;
        SysBusDevice *busdev;
        int i;

        vms->dma = qdev_new("pl330");

        object_property_set_link(OBJECT(vms->dma), "memory", OBJECT(mem), &error_fatal);

        busdev = SYS_BUS_DEVICE(vms->dma);
        sysbus_realize_and_unref(busdev, &error_fatal);
        sysbus_mmio_map(busdev, 0, base);

        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(vms->gic, irq));

        for (i = 0; i < 16; i++) {
                sysbus_connect_irq(busdev, i + 1, qdev_get_gpio_in(vms->gic, irq + i + 1));
        }
}

static void create_mmc(const ComentoMachineState *vms)
{
        hwaddr base = COMENTO_BASE_MMC;
        int irq = COMENTO_IRQ_MMC;
        DeviceState *dev;
        DriveInfo *dinfo;
        // ARM PL181 MMC 인터페이스 장치 추가
        dev = sysbus_create_varargs("pl181", base, \
                                    qdev_get_gpio_in(vms->gic, irq), qdev_get_gpio_in(vms->gic, irq + 1), NULL);
        // QEMU의 -drive format=raw,file=<이미지 이름>,if=sd 옵션으로 지정된
        // SD 카드 이미지 정보 얻기
        dinfo = drive_get(IF_SD, 0, 0);
        if (dinfo) {
                DeviceState *card;
                card = qdev_new(TYPE_SD_CARD); // SD 카드 장치 추가
                qdev_prop_set_drive_err(card, "drive", blk_by_legacy_dinfo(dinfo), &error_fatal);
                qdev_realize_and_unref(card, qdev_get_child_bus(dev, "sd-bus"), &error_fatal); // SD 카드 삽입
        }
}

static void create_gpioa(ComentoMachineState *vms, MemoryRegion *mem)
{
        hwaddr base = COMENTO_BASE_GPIOA;
        int irq = COMENTO_IRQ_GPIOA;
        SysBusDevice *s;

        vms->gpioa = qdev_new("pl061");

        // internal pull-down resistor
        qdev_prop_set_uint32(vms->gpioa, "pullups", 0);
        qdev_prop_set_uint32(vms->gpioa, "pulldowns", 0xff);

        s = SYS_BUS_DEVICE(vms->gpioa);
        sysbus_realize_and_unref(s, &error_fatal);
        memory_region_add_subregion(mem, base, sysbus_mmio_get_region(s, 0));
        sysbus_connect_irq(s, 0, qdev_get_gpio_in(vms->gic, irq));
}

static void create_gpiob(ComentoMachineState *vms, MemoryRegion *mem)
{
        hwaddr base = COMENTO_BASE_GPIOB;
        int irq = COMENTO_IRQ_GPIOB;
        SysBusDevice *s;

        vms->gpiob = qdev_new("pl061");

        // internal pull-down resistor
        qdev_prop_set_uint32(vms->gpiob, "pullups", 0);
        qdev_prop_set_uint32(vms->gpiob, "pulldowns", 0xff);

        s = SYS_BUS_DEVICE(vms->gpiob);
        sysbus_realize_and_unref(s, &error_fatal);
        memory_region_add_subregion(mem, base, sysbus_mmio_get_region(s, 0));
        sysbus_connect_irq(s, 0, qdev_get_gpio_in(vms->gic, irq));
}

static void create_leds_and_button(ComentoMachineState *vms)
{
        DeviceState *dev;
        char name[] = "leds-and-button";
        dev = qdev_new(name);
        qdev_set_id(dev, name, NULL);

        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

        qdev_connect_gpio_out(vms->gpioa, 0, qdev_get_gpio_in(dev, 0));
        qdev_connect_gpio_out(vms->gpioa, 1, qdev_get_gpio_in(dev, 1));
        qdev_connect_gpio_out(vms->gpioa, 2, qdev_get_gpio_in(dev, 2));
        qdev_connect_gpio_out(dev, 0, qdev_get_gpio_in(vms->gpioa, 3));
}

static void create_seven_segment(ComentoMachineState *vms)
{
        DeviceState *dev;
        char name[] = "seven-segment";
        dev = qdev_new(name);
        qdev_set_id(dev, name, NULL);

        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

        qdev_connect_gpio_out(vms->gpiob, 0, qdev_get_gpio_in(dev, 0));
        qdev_connect_gpio_out(vms->gpiob, 1, qdev_get_gpio_in(dev, 1));
        qdev_connect_gpio_out(vms->gpiob, 2, qdev_get_gpio_in(dev, 2));
        qdev_connect_gpio_out(vms->gpiob, 3, qdev_get_gpio_in(dev, 3));
        qdev_connect_gpio_out(vms->gpiob, 4, qdev_get_gpio_in(dev, 4));
        qdev_connect_gpio_out(vms->gpiob, 5, qdev_get_gpio_in(dev, 5));
        qdev_connect_gpio_out(vms->gpiob, 6, qdev_get_gpio_in(dev, 6));
        qdev_connect_gpio_out(dev, 0, qdev_get_gpio_in(vms->gpiob, 7));
}

static void create_spi(ComentoMachineState *vms, MemoryRegion *mem)
{
        hwaddr base = COMENTO_BASE_SPI;
        int irq = COMENTO_IRQ_SPI;
        SysBusDevice *s;
        char name[] = "spi";

        vms->spi = qdev_new("pl022");

        qdev_set_id(vms->spi, name, NULL);

        s = SYS_BUS_DEVICE(vms->spi);
        sysbus_realize_and_unref(s, &error_fatal);
        memory_region_add_subregion(mem, base, sysbus_mmio_get_region(s, 0));
        sysbus_connect_irq(s, 0, qdev_get_gpio_in(vms->gic, irq));
}

static void create_spi_devs(ComentoMachineState *vms)
{
        DeviceState *dev;
        void *ssi = qdev_get_child_bus(vms->spi, "ssi");

        // connect PA4 pin and scale sensor
        dev = ssi_create_peripheral(ssi, "ssi-comento");
        qdev_connect_gpio_out(vms->gpioa, 4, qdev_get_gpio_in_named(dev, SSI_GPIO_CS, 0));

        // connect PA5 pin and scale sensor
        dev = ssi_create_peripheral(ssi, "ssi-comento");
        qdev_connect_gpio_out(vms->gpioa, 5, qdev_get_gpio_in_named(dev, SSI_GPIO_CS, 0));

        // connect PA6 pin and battery chip
        dev = ssi_create_peripheral(ssi, "ssi-battery");
        qdev_connect_gpio_out(vms->gpioa, 6, qdev_get_gpio_in_named(dev, SSI_GPIO_CS, 0));
}

static void comento_machine_class_init(ObjectClass *oc, void *data) {
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
        .name          = "comento-machine",
        .parent        = TYPE_MACHINE,
        .instance_size = sizeof(ComentoMachineState),
        .class_size    = sizeof(ComentoMachineClass),
        .class_init    = comento_machine_class_init,
};

static void machcomento_machine_init(void){
        type_register_static(&comento_machine_info);
}

type_init(machcomento_machine_init);
