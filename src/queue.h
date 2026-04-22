#ifndef QUEUE_H
#define QUEUE_H

typedef struct {
    int *data;
    int capacity;
    int size;
    int front;
} IntQueue;

int queue_init(IntQueue *queue, int capacity);
void queue_free(IntQueue *queue);
int queue_push(IntQueue *queue, int value);
int queue_pop(IntQueue *queue, int *value);
int queue_is_empty(const IntQueue *queue);
int queue_size(const IntQueue *queue);
int queue_at(const IntQueue *queue, int index, int *value_out);

#endif
