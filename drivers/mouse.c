#include <drivers/mouse.h>
#include <kernel/printk.h>

#define JLOS_KERNEL_LOG_SUBSYS "mouse"

void jlos_mouse_event_handler_init(jlos_mouse_event_handler_t* self)
{
    self->on_activate = jlos_mouse_event_handler_on_activate;
    self->on_mouse_down = jlos_mouse_event_handler_on_mouse_down;
    self->on_mouse_up = jlos_mouse_event_handler_on_mouse_up;
    self->mouse_move = jlos_mouse_event_handler_mouse_move;
}

void jlos_mouse_event_handler_on_activate(jlos_mouse_event_handler_t* self)
{
    (void)self;
}

void jlos_mouse_event_handler_on_mouse_down(jlos_mouse_event_handler_t* self, uint8_t button)
{
    (void)self;
    (void)button;
}

void jlos_mouse_event_handler_on_mouse_up(jlos_mouse_event_handler_t* self, uint8_t button)
{
    (void)self;
    (void)button;
}

void jlos_mouse_event_handler_mouse_move(jlos_mouse_event_handler_t* self, int32_t xoffset, int32_t yoffset)
{
    (void)self;
    (void)xoffset;
    (void)yoffset;
}

void jlos_mouse_driver_init(jlos_mouse_driver_t* self, jlos_irq_manager_t *manager, jlos_mouse_event_handler_t *handler)
{
    jlos_io8_init(&self->dataport, 0x60);
    jlos_io8_init(&self->commandport, 0x64);
    
    self->handler = handler;
    self->offset = 0;
    self->buttons = 0;
    
    jlos_irq_handler_init(&self->base_handler, manager, 0x2C);
    self->base_handler.handle_interrupt = (jlos_irq_handler_func_t)jlos_mouse_driver_handle_interrupt;
    
    jlos_driver_init(&self->base_driver);
    self->base_driver.activate = (void (*)(jlos_driver_t*))jlos_mouse_driver_activate;
    
    jlos_irq_manager_register(manager, 0x2C, &self->base_handler);
}

void jlos_mouse_driver_destroy(jlos_mouse_driver_t* self)
{
    (void)self;
}

void jlos_mouse_driver_activate(jlos_mouse_driver_t* self)
{
    self->offset = 0;
    self->buttons = 0;

    jlos_io8_write(&self->commandport, 0xA8);
    jlos_io8_write(&self->commandport, 0x20);
    uint8_t status = jlos_io8_read(&self->dataport) | 2;
    jlos_io8_write(&self->commandport, 0x60);
    jlos_io8_write(&self->dataport, status);

    jlos_io8_write(&self->commandport, 0xD4);
    jlos_io8_write(&self->dataport, 0xF4);
    jlos_io8_read(&self->dataport);
}

uint32_t jlos_mouse_driver_handle_interrupt(jlos_mouse_driver_t* self, uint32_t esp)
{
    jlos_mouse_driver_t* mouse = container_of((jlos_irq_handler_t *)self, jlos_mouse_driver_t, base_handler);

    uint8_t status = jlos_io8_read(&mouse->commandport);
    if (!(status & 0x20)) {  // 检查 bit 5：鼠标数据可用
        return esp;
    }
    mouse->buffer[mouse->offset] = jlos_io8_read(&mouse->dataport);
    if (mouse->handler == NULL) {
        return esp;
    }
    if (mouse->offset == 0 && !(mouse->buffer[0] & 0x08)) {
        return esp;
    } 
    mouse->offset = (mouse->offset + 1) % 3;

    if (mouse->offset == 0) {
        if (mouse->buffer[1] != 0 || mouse->buffer[2] != 0) {
            mouse->handler->mouse_move(mouse->handler, (int8_t)mouse->buffer[1], -((int8_t)mouse->buffer[2]));
        }
        for (uint8_t i = 0; i < 3; i++) {
            if ((mouse->buffer[0] & (0x1 << i)) != (mouse->buttons & (0x1 << i))) {
                if (mouse->buttons & (0x1 << i)) {
                    mouse->handler->on_mouse_up(mouse->handler, i + 1);
                } else {    
                    mouse->handler->on_mouse_down(mouse->handler, i + 1);
                }
            }
        }
        mouse->buttons = mouse->buffer[0];
    }
    return esp;
}