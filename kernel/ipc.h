#ifndef _JLOS_KERNEL_IPC_H
#define _JLOS_KERNEL_IPC_H

#include <common/types.h>
#include <kernel/sync.h>

#define JLOS_KERNEL_PIPE_BUF_SIZE       4096
#define JLOS_KERNEL_PIPE_BUF_MAX_SIZE   65536

#define JLOS_KERNEL_MQ_MSG_MAX_SIZE     256

typedef struct {
    uint8_t             *buffer;
    uint32_t            buf_size;
    uint32_t            read_pos;
    uint32_t            write_pos;
    uint32_t            count;
    jlos_semaphore_t    sem_write_slots;
    jlos_semaphore_t    sem_read_items;
    jlos_mutex_t        mutex;
    bool                closed;
    uint32_t            refcount;
} jlos_pipe_t;

typedef struct jlos_msg_node_t {
    uint32_t                len;
    struct jlos_msg_node_t  *next;
    uint8_t                 data[];
} jlos_msg_node_t;

typedef struct {
    jlos_msg_node_t     *head;
    jlos_msg_node_t     *tail;
    uint32_t            count;
    jlos_semaphore_t    sem_read_items;
    jlos_mutex_t        mutex;
} jlos_mq_t;

void jlos_pipe_init(jlos_pipe_t *pipe, uint32_t buf_size);
void jlos_pipe_close(jlos_pipe_t *pipe);
void jlos_pipe_destroy(jlos_pipe_t *pipe);
uint32_t jlos_pipe_write(jlos_pipe_t *pipe, const void *buf, uint32_t len);
uint32_t jlos_pipe_read(jlos_pipe_t *pipe, void *buf, uint32_t len);

void jlos_mq_init(jlos_mq_t *mq);
void jlos_mq_destroy(jlos_mq_t *mq);
uint32_t jlos_mq_send(jlos_mq_t *mq, const void *buf, uint32_t len);
uint32_t jlos_mq_recv(jlos_mq_t *mq, void *buf, uint32_t max_len);
#endif
