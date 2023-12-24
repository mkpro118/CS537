#ifndef __SAFEQUEUE_H__
#define __SAFEQUEUE_H__

// Alias for the unsigned integer
typedef unsigned int uint;

// Represent an element in the priority queue
typedef struct {
    unsigned int priority; // Priority of the element
    void* value;           // Pointer to the element
} pq_element;

// Represent a priority queue
typedef struct {
    unsigned int size;           // Number of elements currently in the queue
    unsigned int capacity;       // Maximum number of elements in the queue
    pq_element** queue;          // Array for the Max-Heap for the queue
    pthread_mutex_t pq_mutex;    // Lock for the priority queue fields
    pthread_cond_t pq_cond_fill; // CV to block while queue is empty
} priority_queue;

// Macro for swapping two elements in the priority queue
#ifndef _PQ_SWAP_
#define _PQ_SWAP_(x, y, z) {\
    pq_element* temp = (x)->queue[(y)];\
    (x)->queue[(y)] = (x)->queue[(z)];\
    (x)->queue[(z)] = temp;\
}
#endif // _PQ_SWAP_

// Abstract Priority Queue Implementation
priority_queue* pq_init(unsigned int);
void pq_destroy(priority_queue*, void (*)(void*));
int pq_enqueue(priority_queue*, pq_element*);
void* pq_dequeue(priority_queue*);
int is_pq_full(priority_queue*);
int is_pq_empty(priority_queue*);

// Required Interface
priority_queue* create_queue(uint);
int add_work(priority_queue*, pq_element*);
void* get_work(priority_queue*);
void* get_work_nonblocking(priority_queue*);

// Additional for ease of use & readability
int is_queue_full(priority_queue*);
int is_queue_empty(priority_queue*);

// Additional for clean up
extern char EXIT_FLAG;
void destroy_queue(priority_queue*, void(*)(void*));

#endif // __SAFEQUEUE_H__
