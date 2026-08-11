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
    pipe->refcount = 1;
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
    jlos_free(pipe);
}

uint32_t jlos_pipe_write(jlos_pipe_t *pipe, const void *buf, uint32_t len)
{
    if (!pipe || !pipe->buffer || pipe->closed) {
        return 0;
    }
    uint8_t *buf1 = (uint8_t *)buf;
    uint32_t written = 0;
    while (written < len) {
        jlos_semaphore_wait(&pipe->sem_write_slots);
        if (pipe->closed) {
            jlos_semaphore_post(&pipe->sem_write_slots);
            return written;
        }
        jlos_mutex_lock(&pipe->mutex);
        uint32_t slot = pipe->buf_size - pipe->count;
        uint32_t free_contig = pipe->buf_size - pipe->write_pos;
        uint32_t can_write = len - written;
        if (can_write > slot) {
            can_write = slot;
        }
        if (can_write > free_contig) {
            can_write = free_contig;
        }
        for (uint32_t i = 0; i < can_write; i++) {
            pipe->buffer[pipe->write_pos] = buf1[written + i];
            pipe->write_pos = JLOS_ARRAY_LIMIT_RANGE(pipe->write_pos, pipe->buf_size);
        }
        pipe->count += can_write;
        written += can_write;
        jlos_mutex_unlock(&pipe->mutex);

        for (uint32_t i = 0; i < can_write; i++) {
            jlos_semaphore_post(&pipe->sem_read_items);
        }
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
    while (read < len) {
        jlos_semaphore_wait(&pipe->sem_read_items);
        if (pipe->closed && pipe->count == 0) {
            jlos_semaphore_post(&pipe->sem_read_items);
            return read;
        }
        jlos_mutex_lock(&pipe->mutex);
        
        uint32_t avail = pipe->count;
        uint32_t avail_contig = pipe->buf_size - pipe->read_pos;
        uint32_t can_read = len - read;
        if (can_read > avail) {
            can_read = avail;
        }
        if (can_read > avail_contig) {
            can_read = avail_contig;
        }
        for (uint32_t i = 0; i < can_read; i++) {
            buf1[read + i] = pipe->buffer[pipe->read_pos];
            pipe->read_pos = JLOS_ARRAY_LIMIT_RANGE(pipe->read_pos, pipe->buf_size);
        }
        
        pipe->count -= can_read;
        read += can_read;
        jlos_mutex_unlock(&pipe->mutex);

        if (!pipe->closed) {
            for (uint32_t i = 0; i < can_read; i++) {
                jlos_semaphore_post(&pipe->sem_write_slots);
            }   
        }
    }
    return read;
}

void jlos_mq_init(jlos_mq_t *mq)
{
    mq->head = NULL;
    mq->tail = NULL;
    mq->count = 0;
    jlos_semaphore_init(&mq->sem_read_items, 0);
    jlos_mutex_init(&mq->mutex);
}

void jlos_mq_destroy(jlos_mq_t *mq)
{
    jlos_mutex_lock(&mq->mutex);
    jlos_msg_node_t *node = mq->head;
    while (node) {
        jlos_msg_node_t *next = node->next;
        jlos_free(node);
        node = next;
    }
    mq->head = NULL;
    mq->tail = NULL;
    mq->count = 0;
    jlos_mutex_unlock(&mq->mutex);
}

uint32_t jlos_mq_send(jlos_mq_t *mq, const void *buf, uint32_t len)
{
    if (!mq || !buf || len == 0) {
        return 0;
    }
    if (len > JLOS_KERNEL_MQ_MSG_MAX_SIZE) {
        len = JLOS_KERNEL_MQ_MSG_MAX_SIZE;
    }
    jlos_msg_node_t *node = (jlos_msg_node_t *)jlos_malloc(sizeof(jlos_msg_node_t) + len);
    if (!node) {
        return 0;
    }
    jlos_memcpy(node->data, buf, len);
    node->len = len;
    node->next = NULL;
    jlos_mutex_lock(&mq->mutex);
    if (mq->tail) {
        mq->tail->next = node;
    } else {
        mq->head = node;
    }
    mq->tail = node;
    mq->count++;
    jlos_mutex_unlock(&mq->mutex);

    jlos_semaphore_post(&mq->sem_read_items);
    return len;
}

uint32_t jlos_mq_recv(jlos_mq_t *mq, void *buf, uint32_t max_len)
{
    if (!mq || !buf || max_len == 0) {
        return 0;
    }
    jlos_semaphore_wait(&mq->sem_read_items);

    jlos_mutex_lock(&mq->mutex);
    jlos_msg_node_t *node = mq->head;
    if (!node) {
        jlos_mutex_unlock(&mq->mutex);
        return 0;
    }
    mq->head = node->next;
    if (!mq->head) {
        mq->tail = NULL;
    }
    mq->count--;
    jlos_mutex_unlock(&mq->mutex);

    uint32_t copy_len = (node->len < max_len) ? node->len : max_len;
    jlos_memcpy(buf, node->data, copy_len);
    jlos_free(node);
    return copy_len;
}
