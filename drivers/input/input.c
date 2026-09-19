#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <kernel/memory_manager.h>
#include <kernel/console.h>
#include <kernel/printk.h>
#include <kernel/initcall.h>
#include <hal/display.h>

extern jlos_irq_manager_t       *jlos_active_irq_manager;
extern jlos_driver_manager_t    *g_driver_manager_ptr;

typedef struct {
    jlos_keyboard_event_handler_t base;
} console_keyboard_handler_t;

static void console_keyboard_key_down(jlos_keyboard_event_handler_t* self, char c)
{
    (void)self;
    jlos_console_putc(c);
}

typedef struct {
    jlos_mouse_event_handler_t  base;
    int8_t                      x;
    int8_t                      y;
    bool                        visible;
} mouse_console_t;

static void mouse_console_mouse_move(jlos_mouse_event_handler_t* self, int32_t xoffset, int32_t yoffset)
{
    mouse_console_t* console = container_of(self, mouse_console_t, base);

    if (console->visible) {
        jlos_hal_display_invert_at((uint32_t)console->x, (uint32_t)console->y);
    }

    console->x += xoffset;
    if (console->x < 0) {
        console->x = 0;
    }
    if (console->x >= (int32_t)jlos_console_get_cols()) {
        console->x = (int32_t)jlos_console_get_cols() - 1;
    }
    console->y += yoffset;
    if (console->y >= (int32_t)jlos_console_get_rows()) {
        console->y = (int32_t)jlos_console_get_rows() - 1;
    }
    jlos_hal_display_invert_at((uint32_t)console->x, (uint32_t)console->y);
    console->visible = true;
}   

static void input_device_init(void)
{
    console_keyboard_handler_t *kbhandler = (console_keyboard_handler_t *)jlos_kalloc(sizeof(console_keyboard_handler_t));
    jlos_keyboard_event_handler_init(&kbhandler->base);
    kbhandler->base.key_down = console_keyboard_key_down;

    jlos_keyboard_driver_t *keyboard = (jlos_keyboard_driver_t *)jlos_kalloc(sizeof(jlos_keyboard_driver_t));
    jlos_keyboard_driver_init(keyboard, jlos_active_irq_manager, &kbhandler->base);
    jlos_driver_manager_add_driver(g_driver_manager_ptr, (jlos_driver_t*)keyboard);

    mouse_console_t *mouse_handler = (mouse_console_t *)jlos_kalloc(sizeof(mouse_console_t));
    jlos_mouse_event_handler_init(&mouse_handler->base);
    mouse_handler->base.mouse_move = mouse_console_mouse_move;
    mouse_handler->x = (int32_t)(jlos_console_get_cols() / 2);
    mouse_handler->y = (int32_t)(jlos_console_get_rows() / 2);
    mouse_handler->visible = false;

    jlos_mouse_driver_t *mouse = (jlos_mouse_driver_t *)jlos_kalloc(sizeof(jlos_mouse_driver_t));
    jlos_mouse_driver_init(mouse, jlos_active_irq_manager, &mouse_handler->base);
    jlos_driver_manager_add_driver(g_driver_manager_ptr, (jlos_driver_t*)mouse);
}

JLOS_INITCALL(JLOS_INITCALL_DEVICE, input_device_init);
