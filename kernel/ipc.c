#include <kernel/ipc.h>
#include <kernel/memory_manager.h>

void jlos_pipe_init(jlos_pipe_t *pipe, uint32_t buf_size)
{
    if (buf_size == 0) {
        buf_size = JLOS_KERNEL_PIPE_BUF_SIZE;
    }
    if (buf_size > JLOS_KERNEL_PIPE_BUF_MAX_SIZE) {
        buf_size = JLOS_KERNEL_PIPE_BUF_MAX_SIZE;
    }
    pipe->buffer = (uint8_t *)jlos_malloc(sizeof(uint8_t) * buf_size);
    if (!pipe->buffer) {
        return;
    }
    jlos_memset((void *)pipe->buffer, 0, buf_size);
    pipe->buf_size = buf_size;
    pipe->read_pos = 0;
    pipe->write_pos = 0;
    pipe->count = 0;
    jlos_semaphore_init(&pipe->sem_write_slots, buf_size);
    jlos_semaphore_init(&pipe->sem_read_items, 0);
    jlos_mutex_init(&pipe->mutex);
    pipe->closed = false;
}

void jlos_pipe_close(jlos_pipe_t *pipe)
{
    if (!pipe) {
        return;
    }
    pipe->closed = true;
    jlos_semaphore_post(&pipe->sem_read_items);
    jlos_semaphore_post(&pipe->sem_write_slots);
}

void jlos_pipe_destroy(jlos_pipe_t *pipe)
{
    if (!pipe) return;
    if (pipe->buffer) {
        jlos_free(pipe->buffer);
        pipe->buffer = NULL;
    }
}

uint32_t jlos_pipe_write(jlos_pipe_t *pipe, const void *buf, uint32_t len)
{
    if (!pipe || !pipe->buffer || pipe->closed) {
        return 0;
    }
    uint8_t *buf1 = (uint8_t *)buf;
    uint32_t written = 0;
    for (uint32_t i = 0; i < len; i++) {
        jlos_semaphore_wait(&pipe->sem_write_slots);
        if (pipe->closed) {
            jlos_semaphore_post(&pipe->sem_write_slots);
            return written;
        }
        jlos_mutex_lock(&pipe->mutex);
        pipe->buffer[pipe->write_pos] = buf1[i];
        pipe->write_pos = JLOS_ARRAY_LIMIT_RANGE(pipe->write_pos, pipe->buf_size);
        pipe->count++;
        jlos_mutex_unlock(&pipe->mutex);

        jlos_semaphore_post(&pipe->sem_read_items);
        written++;
    }
    return written;
}

uint32_t jlos_pipe_read(jlos_pipe_t *pipe, void *buf, uint32_t len)
{
    if (!pipe || !pipe->buffer) {
        return 0;
    }
    uint8_t *buf1 = (uint8_t *)buf;
    uint32_t read = 0;
    for (uint32_t i = 0; i < len; i++) {
        jlos_semaphore_wait(&pipe->sem_read_items);
        if (pipe->closed && pipe->count == 0) {
            jlos_semaphore_post(&pipe->sem_read_items);
            return read;
        }
        jlos_mutex_lock(&pipe->mutex);
        buf1[i] = pipe->buffer[pipe->read_pos];
        pipe->read_pos = JLOS_ARRAY_LIMIT_RANGE(pipe->read_pos, pipe->buf_size);
        pipe->count--;
        jlos_mutex_unlock(&pipe->mutex);

        if (!pipe->closed) {
            jlos_semaphore_post(&pipe->sem_write_slots);
        }
        read++;
    }
    return read;
}