#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "hw/ssi/ssi.h"
#include "sysemu/runstate.h"
#include "qapi/visitor.h"

#define TYPE_SSI_BATTERY "ssi-battery"

#define COMENTO_SSI_GET_BATTERY 1        // instruction code : get current battery

OBJECT_DECLARE_SIMPLE_TYPE(SSI_BATTERY_State, SSI_BATTERY);

struct SSI_BATTERY_State {
        SSIPeripheral parent_obj;
        uint8_t current_battery;
};

static uint32_t ssi_battery_transfer(SSIPeripheral *obj, uint32_t data)
{
        SSI_BATTERY_State *s = SSI_BATTERY(obj);

        if (data == COMENTO_SSI_GET_BATTERY)
                return 0;

        if (s->current_battery > 100 || s->current_battery < 0)
                return 0;
        
        return (uint8_t)s->current_battery;
}

static char *ssi_comento_get_battery(Object *obj, Error **errp)
{
        SSI_BATTERY_State *s = SSI_BATTERY(obj);
        char buf[8];
        
        sprintf(buf, "%d\n", s->current_battery);

        return g_strdup(buf);
}

static void ssi_comento_set_battery(Object *obj, const char *value, Error **errp)
{
        SSI_BATTERY_State *s = SSI_BATTERY(obj);

        sscanf(value, "%hhd", &s->current_battery);
}

static void ssi_battery_init(Object *obj)
{
        SSI_BATTERY_State *s = SSI_BATTERY(obj);

        s->current_battery = 100;
        object_property_add_str(obj, "percent", ssi_comento_get_battery, ssi_comento_set_battery);
}

static void ssi_battery_realize(SSIPeripheral *obj, Error **errp)
{
}

static void ssi_battery_class_init(Object *klass, void *data)
{
        SSIPeripheralClass *k = SSI_PERIPHERAL_CLASS(klass);

        k->cs_polarity = SSI_CS_HIGH;
        k->realize = ssi_battery_realize;
        k->transfer = ssi_battery_transfer;
}

static const TypeInfo ssi_battery_info = {
        .name          = TYPE_SSI_BATTERY,
        .parent        = TYPE_SSI_PERIPHERAL,
        .instance_size = sizeof(SSI_BATTERY_State),
        .instance_init = ssi_battery_init,
        .class_init    = ssi_battery_class_init,
};

static void ssi_battery_register_types(void)
{
        type_register_static(&ssi_battery_info);
}

type_init(ssi_battery_register_types);
