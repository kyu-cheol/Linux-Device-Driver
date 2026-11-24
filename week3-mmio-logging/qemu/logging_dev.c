#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "qemu/osdep.h"
#include "hw/irq.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"

#define TYPE_LOGGING_COMENTO "logging-comento"
#define LOGGING_DR 0x00     /* Data register */
#define LOGGING_FR 0x04     /* Flag register */
#define LOGGING_RIS 0x08    /* Raw Interrupt Status Register */
#define LOGGING_ICR 0x0c    /* Interrupt Clear Register */
#define LOGGING_IMSC 0x10   /* Interrupt Mask Set/Clear Register */
#define LOGGING_MIS 0x14    /* Masked Interrupt Status Register */
#define LOGGING_RR 0x18     /* Remove Register */

#define LOGGING_FLAG_TXFE (1 << 3)
#define LOGGING_FLAG_TXFF (1 << 2)
#define LOGGING_FLAG_RXFE (1 << 1)
#define LOGGING_FLAG_RXFF (1 << 0)

#define LOGGING_INT_TX (1 << 1)
#define LOGGING_INT_RX (1 << 0)

#define LOGGING_RM     (1 << 0)

#define LOGGING_FIFO_SIZE 32

// Perpheral ID – 0x0000_0002, Cell ID 0xB105_F00D
static const unsigned char comento_logging_id[] = {0x02, 0x00, 0x00, 0x00, 0x0d, 0xf0, 0x05, 0xb1};

struct fifo {
        int front, rear;
        uint8_t buf[LOGGING_FIFO_SIZE];
};

struct LOGGING_COMENTO_State {
        SysBusDevice parent_obj;
        MemoryRegion iomem;
        qemu_irq irq;
        //qemu_irq tx_busy;
        
        uint32_t flag, ris, imsc;
        struct fifo transmit, receive;
};

OBJECT_DECLARE_SIMPLE_TYPE(LOGGING_COMENTO_State, LOGGING_COMENTO);

// ======================================= FIFO ADT =========================================
static void logging_fifo_enqueue(struct fifo *fifo, uint8_t data)
{
        fifo->buf[fifo->rear++] = data;
        fifo->rear %= LOGGING_FIFO_SIZE;
}

static uint8_t logging_fifo_dequeue(struct fifo *fifo)
{
        uint8_t ret = fifo->buf[fifo->front++];
        fifo->front %= LOGGING_FIFO_SIZE;
        return ret;
}

static bool logging_fifo_is_full(struct fifo *fifo)
{
        return fifo->front == ((fifo->rear + 1) % LOGGING_FIFO_SIZE);
}

static bool logging_fifo_is_empty(struct fifo *fifo)
{
        return fifo->front == fifo->rear;
}
// ====================================================================================


static void comento_logging_update(LOGGING_COMENTO_State *s)
{
        uint32_t flags = s->ris & s->imsc;

        qemu_set_irq(s->irq, flags ? 1 : 0);
}

static uint64_t comento_logging_read(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
        LOGGING_COMENTO_State *s = (LOGGING_COMENTO_State *)opaque;
        uint64_t r = 0;

        switch (offset) {

        case LOGGING_DR:
                if (!logging_fifo_is_empty(&s->receive)) {
                        r = logging_fifo_dequeue(&s->receive);
                }
                if (logging_fifo_is_empty(&s->receive)) {
                        s->flag &= ~LOGGING_FLAG_RXFF;
                        s->flag |= LOGGING_FLAG_RXFE;
                        s->ris &= ~LOGGING_INT_RX;
                }
                comento_logging_update(s);
                break;
        case LOGGING_FR:
                r = s->flag;
                break;
        case LOGGING_RIS:
                r = s->ris;
                break;
        case LOGGING_IMSC:
                r = s->imsc;
                break;
        case LOGGING_MIS:
                r = s->ris & s->imsc;
                break;
        case 0xfe0 ... 0xfff:
                r = comento_logging_id[(offset - 0xfe0) >> 2];
                break;
        }
        
        return r;
}

static void comento_logging_write(void * opaque, hwaddr offset, uint64_t value, unsigned size)
{
        LOGGING_COMENTO_State *s = (LOGGING_COMENTO_State *)opaque;
        
        switch (offset) {
        case LOGGING_DR:
                uint32_t fd = open("/tmp/qemu-mmio.lg", O_WRONLY | O_APPEND | O_CREAT, 0644);
                write(fd, &value, 1);
                close(fd);
                break;
        case LOGGING_RR:
                if (value == LOGGING_RM) {
                        system("cat /dev/null > /tmp/qemu-mmio.lg");
                }
                break;
        case LOGGING_ICR:
                s->ris ^= value;
                comento_logging_update(s);
                break;
        case LOGGING_IMSC:
                s->imsc = value;
                comento_logging_update(s);
                break;
        }
}
/*
static char *comento_get_data(Object *obj, Error **errp)
{
        MMIO_COMENTO_State *s = MMIO_COMENTO(obj);
        char *ret = g_malloc(COMENTO_FIFO_SIZE + 1);
        int i;

        for (i = 0; !comento_fifo_is_empty(&s->transmit); i++) {
                ret[i] = comento_fifo_dequeue(&s->transmit);
        }
        ret[i] = '\0';
        
        s->flag &= ~COMENTO_FLAG_TXFF;
        s->flag |= COMENTO_FLAG_TXFE;

        s->ris |= COMENTO_INT_TX;
        comento_mmio_update(s);

        return ret;
}

static void comento_set_data(Object *obj, const char *value, Error **errp)
{
        MMIO_COMENTO_State *s = MMIO_COMENTO(obj);
        int count = strlen(value);
        int i;

        if (count == 0) {
                return;
        }
        for (i = 0; i <count && !comento_fifo_is_full(&s->receive); i++) {
                comento_fifo_enqueue(&s->receive, value[i]);
        }

        s->flag &= ~COMENTO_FLAG_RXFE;

        if (comento_fifo_is_full(&s->receive)) {
                s->flag |= COMENTO_FLAG_RXFF;
        }

        s->ris |= COMENTO_INT_RX;
        comento_mmio_update(s);
}
*/
static const MemoryRegionOps comento_logging_ops = {
        .read = comento_logging_read,
        .write = comento_logging_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
};

static void comento_logging_init(Object *obj)
{
        LOGGING_COMENTO_State *s = LOGGING_COMENTO(obj);
        SysBusDevice *dev = SYS_BUS_DEVICE(obj);

        memory_region_init_io(&s->iomem, obj, &comento_logging_ops, s, "logging-comento", 0x1000);

        sysbus_init_mmio(dev, &s->iomem);
        sysbus_init_irq(dev, &s->irq);

        s->flag = LOGGING_FLAG_TXFE;

        //object_property_add_str(obj, "data", comento_get_data, comento_set_data);
}

static const TypeInfo comento_logging_info = {
        .name = TYPE_LOGGING_COMENTO,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(LOGGING_COMENTO_State),
        .instance_init = comento_logging_init,
};

static void comento_logging_register_types(void)
{
        type_register_static(&comento_logging_info);
}

type_init(comento_logging_register_types)
