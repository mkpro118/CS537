#ifndef __SAFEQUEUE_H__
#define __SAFEQUEUE_H__

typedef unsigned int uint;

typedef struct {
    unsigned int priority;
    void* value;
} pq_element;

typedef struct {
    unsigned int size;
    unsigned int capacity;
    pq_element** queue;
    pthread_mutex_t pq_mutex;
    pthread_cond_t pq_cond_empty;
    pthread_cond_t pq_cond_fill;
} priority_queue;

#ifndef _PQ_SWAP_
#define _PQ_SWAP_(x, y, z) {\
    pq_element* temp = (x)->queue[(y)];\
    (x)->queue[(y)] = (x)->queue[(z)];\
    (x)->queue[(z)] = temp;\
}
#endif


/* Abstract Priority Queue Implementation */
priority_queue* pq_init(unsigned int);
void pq_destroy();
int pq_enqueue(priority_queue*, pq_element*);
void* pq_dequeue(priority_queue*);
int is_pq_full(priority_queue*);
int is_pq_empty(priority_queue*);

/* Required Interface */
void create_queue();
void add_work();
void get_work();
void get_work_nonblocking();

#endif
