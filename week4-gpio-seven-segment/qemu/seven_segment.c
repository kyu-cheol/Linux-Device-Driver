#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "sysemu/runstate.h"
#include "qapi/visitor.h"
#include "chardev/char-fe.h"

#define TYPE_GPIOB_COMENTO "seven-segment"
OBJECT_DECLARE_SIMPLE_TYPE(GPIOB_COMENTO_State, GPIOB_COMENTO);

struct GPIOB_COMENTO_State {
        SysBusDevice parent_obj;
        int64_t key_state;
        qemu_irq key_pin;
};

int arr[7][6] = {
	{ 1, 0, 2, 0, 3, 0 },        // A LED
	{ 4, 1, 4, 2, 4, 3 },        // B LED
	{ 4, 5, 4, 6, 4, 7 },        // C LED
	{ 1, 8, 2, 8, 3, 8 },        // D LED
	{ 0, 7, 0, 6, 0, 5 },        // E LED
	{ 0, 3, 0, 2, 0, 1 },        // F LED
	{ 1, 4, 2, 4, 3, 4 }         // G LED
};

void move_cur(int x, int y) {
	printf("\033[%dd\033[%dG", y, x);
}

static void gpiob_comento_handler(void *opaque, int n, int level)
{
        int init_x = 3;
        int init_y = 15;
        int i;
        
        if (level) {
                //printf("[%d]LED ON\n", n);

                for (i = 0; i < 6; i += 2) {
			move_cur(init_x + arr[n][i], init_y + arr[n][i+1]);
			putchar('*');
		}
                printf("\n");
        } else {
                //printf("[%d]LED OFF\n", n);

                for (i = 0; i < 6; i += 2) {
			move_cur(init_x + arr[n][i], init_y + arr[n][i+1]);
			putchar(' ');
		}
                printf("\n");
        }
}

static char *gpiob_comento_get_key(Object *obj, Error **errp)
{
        GPIOB_COMENTO_State *s = GPIOB_COMENTO(obj);

        return s->key_state ? g_strdup("True") : g_strdup("False");
}

static void gpiob_comento_set_key(Object *obj, const char *value, Error **errp)
{
        GPIOB_COMENTO_State *s = GPIOB_COMENTO(obj);

        if (g_strcmp0(value, "true") && g_strcmp0(value, "True")) {
                s->key_state = 0;
                qemu_irq_lower(s->key_pin);
        } else {
                s->key_state = 1;
                qemu_irq_raise(s->key_pin);
        }
}

static void gpiob_comento_init(Object *obj)
{
        GPIOB_COMENTO_State *s = GPIOB_COMENTO(obj);
        DeviceState *dev = DEVICE(obj);

        qdev_init_gpio_in(dev, gpiob_comento_handler, 7);
        qdev_init_gpio_out(dev, &s->key_pin, 1);

        s->key_state = 0;

        object_property_add_str(obj, "key", gpiob_comento_get_key, gpiob_comento_set_key);
}

static const TypeInfo gpiob_comento_info = {
        .name          = TYPE_GPIOB_COMENTO,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(GPIOB_COMENTO_State),
        .instance_init = gpiob_comento_init,
};

static void gpiob_comento_register_types(void)
{
        type_register_static(&gpiob_comento_info);
}

type_init(gpiob_comento_register_types);
