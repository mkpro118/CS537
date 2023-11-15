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
static priority_queue* pq_init(unsigned int);
static void pq_destroy();
static int pq_enqueue(priority_queue*, pq_element*);
static void* pq_dequeue(priority_queue*);
static int is_pq_full(priority_queue*);
static int is_pq_empty(priority_queue*);

/* Required Interface */
priority_queue* create_queue(uint);
void add_work(priority_queue*, pq_element*);
void* get_work(priority_queue*);
void* get_work_nonblocking(priority_queue*);

// Additional for clean up
void destroy_queue(priority_queue*);

#endif
