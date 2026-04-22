#include "queue.h"

#include <stdlib.h>

/* READY uses a fixed-size circular queue because the process count is known. */
int queue_init(IntQueue *queue, int capacity) {
    queue->data = (int *)malloc(sizeof(int) * capacity);
    if (queue->data == NULL) {
        return 0;
    }

    queue->capacity = capacity;
    queue->size = 0;
    queue->front = 0;
    return 1;
}

void queue_free(IntQueue *queue) {
    free(queue->data);
    queue->data = NULL;
    queue->capacity = 0;
    queue->size = 0;
    queue->front = 0;
}

int queue_push(IntQueue *queue, int value) {
    int back;

    if (queue->size >= queue->capacity) {
        return 0;
    }

    /* Circular indexing avoids shuffling elements on every requeue. */
    back = (queue->front + queue->size) % queue->capacity;
    queue->data[back] = value;
    queue->size++;
    return 1;
}

int queue_pop(IntQueue *queue, int *value) {
    if (queue->size == 0) {
        return 0;
    }

    *value = queue->data[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;
    return 1;
}

int queue_is_empty(const IntQueue *queue) {
    return queue->size == 0;
}

int queue_size(const IntQueue *queue) {
    return queue->size;
}

int queue_at(const IntQueue *queue, int index, int *value_out) {
    int pos;

    if (index < 0 || index >= queue->size) {
        return 0;
    }

    /* Random access is only for visualization; scheduling still stays FIFO. */
    pos = (queue->front + index) % queue->capacity;
    *value_out = queue->data[pos];
    return 1;
}
