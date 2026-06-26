#include <tools/samples/debug_console.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <kernel/memory_manager.h>

extern void printf(const char *);

#define offsetof(type, member) ((size_t)((char*)&((type*)0)->member))

typedef struct {
    jlos_keyboard_event_handler_t base;
} printf_keyboard_event_handler_t;

static void printf_keyboard_key_down(jlos_keyboard_event_handler_t* self, char c)
{
    char foo[2] = {c, '\0'};
    printf(foo);
}

typedef struct {
    jlos_mouse_event_handler_t base;
    int8_t m_x;
    int8_t m_y;
} mouse_console_t;

static void mouse_console_mouse_move(jlos_mouse_event_handler_t* self, int32_t xoffset, int32_t yoffset)
{
    static uint16_t *video_memory = (uint16_t *)0xb8000;
    mouse_console_t* console = (mouse_console_t*)((char*)self - offsetof(mouse_console_t, base));

    video_memory[80 * console->m_y + console->m_x] = ((video_memory[80 * console->m_y + console->m_x] & 0xF000) >> 4)
        | ((video_memory[80 * console->m_y + console->m_x] & 0x0F00) << 4)
        | ((video_memory[80 * console->m_y + console->m_x] & 0x00FF));

    console->m_x += xoffset;
    if (console->m_x < 0) console->m_x = 0;
    if (console->m_x >= 80) console->m_x = 79;
    console->m_y += yoffset;
    if (console->m_y < 0) console->m_y = 0;
    if (console->m_y >= 25) console->m_y = 24;

    video_memory[80 * console->m_y + console->m_x] = ((video_memory[80 * console->m_y + console->m_x] & 0xF000) >> 4)
        | ((video_memory[80 * console->m_y + console->m_x] & 0x0F00) << 4)
        | ((video_memory[80 * console->m_y + console->m_x] & 0x00FF));
}

void mouse_console_init(mouse_console_t *mouse_handler)
{
    mouse_handler->base.mouse_move = mouse_console_mouse_move;
    mouse_handler->m_x = 40;
    mouse_handler->m_y = 12;
    uint16_t *video_memory = (uint16_t *)0xb8000;
    video_memory[80 * mouse_handler->m_y + mouse_handler->m_x] = ((video_memory[80 * mouse_handler->m_y + mouse_handler->m_x] & 0xF000) >> 4)
        | ((video_memory[80 * mouse_handler->m_y + mouse_handler->m_x] & 0x0F00) << 4)
        | ((video_memory[80 * mouse_handler->m_y + mouse_handler->m_x] & 0x00FF));
}

void debug_console_keyboard(jlos_interrupt_manager_t *interrupts, jlos_driver_manager_t *driver_manager_)
{
    printf_keyboard_event_handler_t *kbhandler = (printf_keyboard_event_handler_t *)jlos_malloc(sizeof(printf_keyboard_event_handler_t));
    jlos_keyboard_event_handler_init(&kbhandler->base);
    kbhandler->base.key_down = printf_keyboard_key_down;

    jlos_keyboard_driver_t *keyboard = (jlos_keyboard_driver_t *)jlos_malloc(sizeof(jlos_keyboard_driver_t));
    jlos_keyboard_driver_init(keyboard, interrupts, &kbhandler->base);
    jlos_driver_manager_add_driver(driver_manager_, (jlos_driver_t*)keyboard);
}

void debug_console_mouse(jlos_interrupt_manager_t *interrupts, jlos_driver_manager_t *driver_manager_)
{
    mouse_console_t *mouse_handler_ = (mouse_console_t *)jlos_malloc(sizeof(mouse_console_t));
    jlos_mouse_event_handler_init(&mouse_handler_->base);
    mouse_console_init(mouse_handler_);

    jlos_mouse_driver_t *mouse = (jlos_mouse_driver_t *)jlos_malloc(sizeof(jlos_mouse_driver_t));
    jlos_mouse_driver_init(mouse, interrupts, &mouse_handler_->base);
    jlos_driver_manager_add_driver(driver_manager_, (jlos_driver_t*)mouse);
}

void debug_console_init(jlos_interrupt_manager_t *interrupts, jlos_driver_manager_t *driver_manager_)
{
    debug_console_keyboard(interrupts, driver_manager_);
    debug_console_mouse(interrupts, driver_manager_);
}
