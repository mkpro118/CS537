#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "safequeue.h"

////////////////////////////////////////////////////////////////////////////////
///                  Abstract Priority Queue Implementation                  ///
////////////////////////////////////////////////////////////////////////////////

/**
 * Priority Queue constructor
 * @param  capacity The maximum capacity of the priority queue
 * @return          Pointer to a heap allocated priority queue
 */
priority_queue* pq_init(uint capacity) {
    priority_queue* pq = NULL;
    if (!capacity) {
        perror("pq_init failed because capacity = 0\n");
        goto end_op;
    }

    pq = malloc(sizeof(priority_queue));

    if (!pq)
        goto end_op;

    pq->size = 0;
    pq->capacity = capacity;

    // Should we use calloc?
    pq->queue = malloc(sizeof(pq_element*) * capacity);

    if (!pq->queue) {
        free(pq);
        pq = NULL;
        goto end_op;
    }

    if (!pq->queue) {
        perror("malloc failed in pq_init()\n");
        goto end_op;
    }

    pthread_mutex_init(&pq->pq_mutex, NULL);
    pthread_cond_init(&pq->pq_cond_fill, NULL);

    end_op:
    return pq;
}

/**
 * Priority Queue destructor
 * @param pq The Priority Queue to destroy
 */
void pq_destroy(priority_queue* pq, void (*cleanup)(void*)) {
    if (!pq)
        return;
    pthread_mutex_lock(&pq->pq_mutex);

    if (!pq->queue)
        goto end_op;

    // Free each pq_element in the queue
    for (uint i = 0; i < pq->size; i++) {
        if (!pq->queue[i])
            continue;

        if (pq->queue[i]->value) {
            if (cleanup)
                cleanup(pq->queue[i]->value);
            free(pq->queue[i]->value);
        }
        pq->queue[i]->value = NULL;

        free(pq->queue[i]);
        pq->queue[i] = NULL;
    }

    free(pq->queue);
    pq->queue = NULL;

    end_op:
    pthread_cond_destroy(&pq->pq_cond_fill);

    pthread_mutex_unlock(&pq->pq_mutex);
    pthread_mutex_destroy(&pq->pq_mutex);

    free(pq);
    pq = NULL;
}

/**
 * Checks if priority queue is full.
 * Assumes calling thread is holding pq->pq_mutex lock
 * @param  pq The Priority Queue to test
 * @return    1 if full, 0 if not full
 */
int is_pq_full(priority_queue* pq) { return pq->size == pq->capacity; }

/**
 * Checks if priority queue is empty.
 * Assumes calling thread is holding pq->pq_mutex lock
 * @param  pq The Priority Queue to test
 * @return    1 if empty, 0 if not empty
 */
int is_pq_empty(priority_queue* pq) { return pq->size == 0; }

// Get parent or child indices in the max-heap
static inline uint parent_idx(uint idx) { return !idx ? idx : (idx - 1) >> 1; }
static inline uint lchld(uint idx) { return (idx << 1) + 1; }
static inline uint rchld(uint idx) { return (idx << 1) + 2; }

/**
 * Enqueue an element in the priority queue
 * Fails if priority queue is full
 *
 * @param  pq      The Priority Queue to enqueue to
 * @param  pq_elem The element to add
 * @return         0 on success, -1 on failure
 */
int pq_enqueue(priority_queue* pq, pq_element* pq_elem) {
    int retval = -1;

    if (is_pq_full(pq)) {
        printf("Cannot add to a full queue\n");
        goto end_op;
    }

    pq->queue[pq->size++] = pq_elem;

    uint idx = pq->size - 1;
    uint p_idx = parent_idx(idx);

    // Percolate up
    while (pq->queue[idx]->priority > pq->queue[p_idx]->priority) {
        _PQ_SWAP_(pq, idx, p_idx)

        // Update indexes
        idx = p_idx;
        p_idx = parent_idx(idx);
    }

    retval = 0;

    end_op:
    return retval;
}

/**
 * Dequeues an element from the priority queue, and returns it
 * Returns NULL if priority queue is empty
 *
 * @param pq The Priority Queue to enqueue to
 */
void* pq_dequeue(priority_queue* pq) {
    pq_element* elem = NULL;

    if (is_pq_empty(pq)) {
        printf("Cannot dequeue from an empty queue\n");
        goto end_op;
    }

    elem = pq->queue[0]->value;
    pq->queue[0] = pq->queue[--pq->size];

    uint idx = 0;
    uint left_idx  = lchld(idx);
    uint right_idx = rchld(idx);

    uint swap;

    // Percolate down
    while (left_idx < pq->size) {
        swap = idx;
        if (pq->queue[left_idx]->priority >= pq->queue[swap]->priority)
            swap = left_idx;

        if (right_idx < pq->size && pq->queue[right_idx]->priority >= pq->queue[swap]->priority)
            swap = right_idx;

        if (swap == idx)
            break;

        _PQ_SWAP_(pq, idx, swap)

        // Update indexes
        idx = swap;
        left_idx = lchld(idx);
        right_idx = rchld(idx);
    }

    end_op:
    return elem;
}

////////////////////////////////////////////////////////////////////////////////
///              End Abstract Priority Queue Implementation                  ///
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
///                    Required Interface Implementation                     ///
////////////////////////////////////////////////////////////////////////////////

char EXIT_FLAG = 0;

priority_queue* create_queue(uint capacity) {
    return pq_init(capacity);
}

int add_work(priority_queue* pq, pq_element* elem) {
    int retval = 0;
    pthread_mutex_lock(&pq->pq_mutex);

    if (is_pq_full(pq)) {
        retval = -1;
        goto end_op;
    }

    pq_enqueue(pq, elem);

    pthread_cond_signal(&pq->pq_cond_fill);

    end_op:
    pthread_mutex_unlock(&pq->pq_mutex);
    return retval;
}

void* get_work(priority_queue* pq) {
    pthread_mutex_lock(&pq->pq_mutex);

    while (is_pq_empty(pq)) {
        if (EXIT_FLAG) {
            pthread_mutex_unlock(&pq->pq_mutex);
            return NULL;
        }
        pthread_cond_wait(&pq->pq_cond_fill, &pq->pq_mutex);
    }

    void* elem = pq_dequeue(pq);

    pthread_mutex_unlock(&pq->pq_mutex);
    return elem;
}

void* get_work_nonblocking(priority_queue* pq) {
    pthread_mutex_lock(&pq->pq_mutex);

    void* elem = pq_dequeue(pq);

    pthread_mutex_unlock(&pq->pq_mutex);

    return elem;
}

int is_queue_full(priority_queue* pq) { return is_pq_full(pq); }

int is_queue_empty(priority_queue* pq) { return is_pq_empty(pq); }


void destroy_queue(priority_queue* pq, void (*cleanup)(void*)) {
    pq_destroy(pq, cleanup);
}

////////////////////////////////////////////////////////////////////////////////
///                End Required Interface Implementation                     ///
////////////////////////////////////////////////////////////////////////////////
