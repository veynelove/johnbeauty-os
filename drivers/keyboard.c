#include <drivers/keyboard.h>
#include <kernel/printk.h>

#define JLOS_KERNEL_LOG_SUBSYS "kbd"

void jlos_keyboard_event_handler_init(jlos_keyboard_event_handler_t* self)
{
    self->key_down = jlos_keyboard_event_handler_key_down;
    self->on_key_up = jlos_keyboard_event_handler_on_key_up;
}

void jlos_keyboard_event_handler_key_down(jlos_keyboard_event_handler_t* self, char key)
{
    (void)self;
    (void)key;
}

void jlos_keyboard_event_handler_on_key_up(jlos_keyboard_event_handler_t* self, char key)
{
    (void)self;
    (void)key;
}

void jlos_keyboard_driver_init(jlos_keyboard_driver_t* self, jlos_irq_manager_t *manager, jlos_keyboard_event_handler_t *handler)
{
    jlos_io8_init(&self->dataport, 0x60);
    jlos_io8_init(&self->commandport, 0x64);
    
    self->handler = handler;
    
    jlos_irq_handler_init(&self->base_handler, manager, 0x21);
    self->base_handler.handle_interrupt = (jlos_irq_handler_func_t)jlos_keyboard_driver_handle_interrupt;
    
    jlos_driver_init(&self->base_driver);
    self->base_driver.activate = (void (*)(jlos_driver_t*))jlos_keyboard_driver_activate;
    
    jlos_irq_manager_register(manager, 0x21, &self->base_handler);
}

void jlos_keyboard_driver_destroy(jlos_keyboard_driver_t* self)
{
    (void)self;
}

void jlos_keyboard_driver_activate(jlos_keyboard_driver_t* self)
{
    while (jlos_io8_read(&self->commandport) & 0x1) {
        jlos_io8_read(&self->dataport);
    }
    jlos_io8_write(&self->commandport, 0xAE);
    jlos_io8_write(&self->commandport, 0x20);
    uint8_t status = (jlos_io8_read(&self->dataport) | 1) & ~0x10;
    jlos_io8_write(&self->commandport, 0x60);
    jlos_io8_write(&self->dataport, status);

    jlos_io8_write(&self->dataport, 0xF4);
}

uint32_t jlos_keyboard_driver_handle_interrupt(jlos_keyboard_driver_t* self, uint32_t esp)
{
    jlos_keyboard_driver_t* keyboard = container_of((jlos_irq_handler_t *)self, jlos_keyboard_driver_t, base_handler);
    
    uint8_t status = jlos_io8_read(&keyboard->commandport);
    if (!(status & 0x01)) {
        return esp;
    }
    uint8_t key = jlos_io8_read(&keyboard->dataport);
    if (keyboard->handler == NULL) return esp;
    static bool shift = false;
    switch (key) {
        case 0x29: if(shift) keyboard->handler->key_down(keyboard->handler, '~'); else keyboard->handler->key_down(keyboard->handler, '`'); break;
        case 0x02: if(shift) keyboard->handler->key_down(keyboard->handler, '!'); else keyboard->handler->key_down(keyboard->handler, '1'); break;
        case 0x03: if(shift) keyboard->handler->key_down(keyboard->handler, '@'); else keyboard->handler->key_down(keyboard->handler, '2'); break;
        case 0x04: if(shift) keyboard->handler->key_down(keyboard->handler, '#'); else keyboard->handler->key_down(keyboard->handler, '3'); break;
        case 0x05: if(shift) keyboard->handler->key_down(keyboard->handler, '$'); else keyboard->handler->key_down(keyboard->handler, '4'); break;
        case 0x06: if(shift) keyboard->handler->key_down(keyboard->handler, '%'); else keyboard->handler->key_down(keyboard->handler, '5'); break;
        case 0x07: if(shift) keyboard->handler->key_down(keyboard->handler, '^'); else keyboard->handler->key_down(keyboard->handler, '6'); break;
        case 0x08: if(shift) keyboard->handler->key_down(keyboard->handler, '&'); else keyboard->handler->key_down(keyboard->handler, '7'); break;
        case 0x09: if(shift) keyboard->handler->key_down(keyboard->handler, '*'); else keyboard->handler->key_down(keyboard->handler, '8'); break;
        case 0x0A: if(shift) keyboard->handler->key_down(keyboard->handler, '('); else keyboard->handler->key_down(keyboard->handler, '9'); break;
        case 0x0B: if(shift) keyboard->handler->key_down(keyboard->handler, ')'); else keyboard->handler->key_down(keyboard->handler, '0'); break;
        case 0x0C: if(shift) keyboard->handler->key_down(keyboard->handler, '_'); else keyboard->handler->key_down(keyboard->handler, '-'); break;
        case 0x0D: if(shift) keyboard->handler->key_down(keyboard->handler, '+'); else keyboard->handler->key_down(keyboard->handler, '='); break;

        case 0x1A: if(shift) keyboard->handler->key_down(keyboard->handler, '{'); else keyboard->handler->key_down(keyboard->handler, '['); break;
        case 0x1B: if(shift) keyboard->handler->key_down(keyboard->handler, '}'); else keyboard->handler->key_down(keyboard->handler, ']'); break;
        case 0x2B: if(shift) keyboard->handler->key_down(keyboard->handler, '|'); else keyboard->handler->key_down(keyboard->handler, '\\'); break;
        case 0x27: if(shift) keyboard->handler->key_down(keyboard->handler, ':'); else keyboard->handler->key_down(keyboard->handler, ';'); break;
        case 0x28: if(shift) keyboard->handler->key_down(keyboard->handler, '"'); else keyboard->handler->key_down(keyboard->handler, '\''); break;
        case 0x33: if(shift) keyboard->handler->key_down(keyboard->handler, '<'); else keyboard->handler->key_down(keyboard->handler, ','); break;
        case 0x34: if(shift) keyboard->handler->key_down(keyboard->handler, '>'); else keyboard->handler->key_down(keyboard->handler, '.'); break;
        case 0x35: if(shift) keyboard->handler->key_down(keyboard->handler, '?'); else keyboard->handler->key_down(keyboard->handler, '/'); break;

        case 0x10: if(shift) keyboard->handler->key_down(keyboard->handler, 'Q'); else keyboard->handler->key_down(keyboard->handler, 'q'); break;
        case 0x11: if(shift) keyboard->handler->key_down(keyboard->handler, 'W'); else keyboard->handler->key_down(keyboard->handler, 'w'); break;
        case 0x12: if(shift) keyboard->handler->key_down(keyboard->handler, 'E'); else keyboard->handler->key_down(keyboard->handler, 'e'); break;
        case 0x13: if(shift) keyboard->handler->key_down(keyboard->handler, 'R'); else keyboard->handler->key_down(keyboard->handler, 'r'); break;
        case 0x14: if(shift) keyboard->handler->key_down(keyboard->handler, 'T'); else keyboard->handler->key_down(keyboard->handler, 't'); break;
        case 0x15: if(shift) keyboard->handler->key_down(keyboard->handler, 'Y'); else keyboard->handler->key_down(keyboard->handler, 'y'); break;
        case 0x16: if(shift) keyboard->handler->key_down(keyboard->handler, 'U'); else keyboard->handler->key_down(keyboard->handler, 'u'); break;
        case 0x17: if(shift) keyboard->handler->key_down(keyboard->handler, 'I'); else keyboard->handler->key_down(keyboard->handler, 'i'); break;
        case 0x18: if(shift) keyboard->handler->key_down(keyboard->handler, 'O'); else keyboard->handler->key_down(keyboard->handler, 'o'); break;
        case 0x19: if(shift) keyboard->handler->key_down(keyboard->handler, 'P'); else keyboard->handler->key_down(keyboard->handler, 'p'); break;
        case 0x1E: if(shift) keyboard->handler->key_down(keyboard->handler, 'A'); else keyboard->handler->key_down(keyboard->handler, 'a'); break;
        case 0x1F: if(shift) keyboard->handler->key_down(keyboard->handler, 'S'); else keyboard->handler->key_down(keyboard->handler, 's'); break;
        case 0x20: if(shift) keyboard->handler->key_down(keyboard->handler, 'D'); else keyboard->handler->key_down(keyboard->handler, 'd'); break;
        case 0x21: if(shift) keyboard->handler->key_down(keyboard->handler, 'F'); else keyboard->handler->key_down(keyboard->handler, 'f'); break;
        case 0x22: if(shift) keyboard->handler->key_down(keyboard->handler, 'G'); else keyboard->handler->key_down(keyboard->handler, 'g'); break;
        case 0x23: if(shift) keyboard->handler->key_down(keyboard->handler, 'H'); else keyboard->handler->key_down(keyboard->handler, 'h'); break;
        case 0x24: if(shift) keyboard->handler->key_down(keyboard->handler, 'J'); else keyboard->handler->key_down(keyboard->handler, 'j'); break;
        case 0x25: if(shift) keyboard->handler->key_down(keyboard->handler, 'K'); else keyboard->handler->key_down(keyboard->handler, 'k'); break;
        case 0x26: if(shift) keyboard->handler->key_down(keyboard->handler, 'L'); else keyboard->handler->key_down(keyboard->handler, 'l'); break;
        case 0x2C: if(shift) keyboard->handler->key_down(keyboard->handler, 'Z'); else keyboard->handler->key_down(keyboard->handler, 'z'); break;
        case 0x2D: if(shift) keyboard->handler->key_down(keyboard->handler, 'X'); else keyboard->handler->key_down(keyboard->handler, 'x'); break;
        case 0x2E: if(shift) keyboard->handler->key_down(keyboard->handler, 'C'); else keyboard->handler->key_down(keyboard->handler, 'c'); break;
        case 0x2F: if(shift) keyboard->handler->key_down(keyboard->handler, 'V'); else keyboard->handler->key_down(keyboard->handler, 'v'); break;
        case 0x30: if(shift) keyboard->handler->key_down(keyboard->handler, 'B'); else keyboard->handler->key_down(keyboard->handler, 'b'); break;
        case 0x31: if(shift) keyboard->handler->key_down(keyboard->handler, 'N'); else keyboard->handler->key_down(keyboard->handler, 'n'); break;
        case 0x32: if(shift) keyboard->handler->key_down(keyboard->handler, 'M'); else keyboard->handler->key_down(keyboard->handler, 'm'); break;

        case 0x1C: keyboard->handler->key_down(keyboard->handler, '\n'); break;
        case 0x0E: keyboard->handler->key_down(keyboard->handler, '\b'); break;
        case 0x39: keyboard->handler->key_down(keyboard->handler, ' '); break;
        
        case 0x2A: case 0x36: shift = true; break;
        case 0xAA: case 0xB6: shift = false; break;
        case 0x3A: break;
        
        default:
            break;
    }
    return esp;
}