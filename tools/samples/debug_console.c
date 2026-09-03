#include <tools/samples/debug_console.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <kernel/memory_manager.h>
#include <kernel/paging.h>
#include <kernel/printk.h>

#define JLOS_VGA_TEXT_BUFFER_VA  ((uint16_t *)PHYS_TO_VIRT(0xB8000))

typedef struct {
    jlos_keyboard_event_handler_t base;
} console_keyboard_handler_t;

static void console_keyboard_key_down(jlos_keyboard_event_handler_t* self, char c)
{
    (void)self;
    char foo[2] = {c, '\0'};
    printf(foo);
}

typedef struct {
    jlos_mouse_event_handler_t base;
    int8_t x;
    int8_t y;
} mouse_console_t;

static void mouse_console_mouse_move(jlos_mouse_event_handler_t* self, int32_t xoffset, int32_t yoffset)
{
    uint16_t *video_memory = JLOS_VGA_TEXT_BUFFER_VA;
    mouse_console_t* console = container_of(self, mouse_console_t, base);

    video_memory[80 * console->y + console->x] = ((video_memory[80 * console->y + console->x] & 0xF000) >> 4)
        | ((video_memory[80 * console->y + console->x] & 0x0F00) << 4)
        | ((video_memory[80 * console->y + console->x] & 0x00FF));

    console->x += xoffset;
    if (console->x < 0) console->x = 0;
    if (console->x >= 80) console->x = 79;
    console->y += yoffset;
    if (console->y < 0) console->y = 0;
    if (console->y >= 25) console->y = 24;

    video_memory[80 * console->y + console->x] = ((video_memory[80 * console->y + console->x] & 0xF000) >> 4)
        | ((video_memory[80 * console->y + console->x] & 0x0F00) << 4)
        | ((video_memory[80 * console->y + console->x] & 0x00FF));
}

void mouse_console_init(mouse_console_t *mouse_handler)
{
    mouse_handler->base.mouse_move = mouse_console_mouse_move;
    mouse_handler->x = 40;
    mouse_handler->y = 12;
    uint16_t *video_memory = JLOS_VGA_TEXT_BUFFER_VA;
    video_memory[80 * mouse_handler->y + mouse_handler->x] = ((video_memory[80 * mouse_handler->y + mouse_handler->x] & 0xF000) >> 4)
        | ((video_memory[80 * mouse_handler->y + mouse_handler->x] & 0x0F00) << 4)
        | ((video_memory[80 * mouse_handler->y + mouse_handler->x] & 0x00FF));
}

void debug_console_keyboard(jlos_irq_manager_t *interrupts, jlos_driver_manager_t *driver_manager_)
{
    console_keyboard_handler_t *kbhandler = (console_keyboard_handler_t *)jlos_kalloc(sizeof(console_keyboard_handler_t));
    jlos_keyboard_event_handler_init(&kbhandler->base);
    kbhandler->base.key_down = console_keyboard_key_down;

    jlos_keyboard_driver_t *keyboard = (jlos_keyboard_driver_t *)jlos_kalloc(sizeof(jlos_keyboard_driver_t));
    jlos_keyboard_driver_init(keyboard, interrupts, &kbhandler->base);
    jlos_driver_manager_add_driver(driver_manager_, (jlos_driver_t*)keyboard);
}

void debug_console_mouse(jlos_irq_manager_t *interrupts, jlos_driver_manager_t *driver_manager_)
{
    mouse_console_t *mouse_handler_ = (mouse_console_t *)jlos_kalloc(sizeof(mouse_console_t));
    jlos_mouse_event_handler_init(&mouse_handler_->base);
    mouse_console_init(mouse_handler_);

    jlos_mouse_driver_t *mouse = (jlos_mouse_driver_t *)jlos_kalloc(sizeof(jlos_mouse_driver_t));
    jlos_mouse_driver_init(mouse, interrupts, &mouse_handler_->base);
    jlos_driver_manager_add_driver(driver_manager_, (jlos_driver_t*)mouse);
}

void debug_console_init(jlos_irq_manager_t *interrupts, jlos_driver_manager_t *driver_manager_)
{
    debug_console_keyboard(interrupts, driver_manager_);
    debug_console_mouse(interrupts, driver_manager_);
}
