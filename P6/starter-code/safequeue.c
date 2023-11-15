#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "safequeue.h"

priority_queue* pq_init(unsigned int capacity) {
    if (!capacity) {
        perror("pq_init failed because capacity = 0\n");
        return NULL;
    }

    priority_queue* pq = malloc(sizeof(priority_queue));

    pq->size = 0;
    pq->capacity = capacity;
    pq->queue = malloc(sizeof(pq_element*) * capacity);

    if (!pq->queue) {
        perror("malloc failed in pq_init()\n");
        return NULL;
    }

    pthread_mutex_init(&pq->pq_mutex);

    return pq;
}

void pq_destroy(priority_queue* pq) {
    pthread_mutex_lock(&pq->pq_mutex);
    free(pq->queue);
    pthread_mutex_unlock(&pq->pq_mutex);

    pthread_mutex_destroy(&pq->pq_mutex);
    free(pq);
}

/**
 * Checks if priority queue is full.
 * Assumes calling thread is holding pq->pq_mutex lock
 * @param  pq The Priority Queue to test
 * @return    1 if full, 0 if not full
 */
static inline unsigned int is_pq_full(priority_queue* pq) {
    return pq->size == pq->capacity;
}

/**
 * Checks if priority queue is empty.
 * Assumes calling thread is holding pq->pq_mutex lock
 * @param  pq The Priority Queue to test
 * @return    1 if empty, 0 if not empty
 */
static inline unsigned int is_pq_empty(priority_queue* pq) {
    return !pq->size;
}

/**
 * Get the index of the max-heap parent node given a child index
 * @param  idx The index of the child
 * @return     Index of the parent
 */
static inline unsigned int parent_idx(unsigned int idx) {
    return !idx ? idx : (idx - 1) >> 1;
}

/**
 * Get the index of the max-heap left child node given a parent index
 * @param  idx The index of the parent
 * @return     Index of the left child
 */
static inline unsigned int left_child_idx(unsigned int idx) {
    return (idx << 1) + 1;
}

/**
 * Get the index of the max-heap right child node given a parent index
 * @param  idx The index of the parent
 * @return     Index of the right child
 */
static inline unsigned int right_child_idx(unsigned int idx) {
    return (idx << 1) + 2;
}

/**
 * Enqueue an element in the priority queue
 * Fails if priority queue is full
 *
 * @param  pq      The Priority Queue to enqueue to
 * @param  pq_elem The element to add
 * @return         0 on success, -1 on failure
 */
int pq_enqueue(priority_queue* pq, pq_element* pq_elem) {
    pthread_mutex_lock(&pq->pq_mutex);

    if (is_pq_full(pq)) {
        pthread_mutex_unlock(&pq->pq_mutex);
        perror("Cannot add to a full queue\n");
        return -1;
    }

    pq->queue[pq->size++] = pq_elem;

    unsigned int idx = pq->size - 1;
    unsigned int p_idx = parent_idx(idx);

    while (pq->queue[idx]->priority > pq->queue[p_idx]->priority) {
        // Swap elements
        pq_element* temp = pq->queue[idx];
        pq->queue[idx] = pq->queue[p_idx];
        pq->queue[p_idx] = temp;

        // Update indexes
        idx = p_idx;
        p_idx = parent_idx(idx);
    }

    pthread_mutex_unlock(&pq->pq_mutex);

    return 0;
}

/**
 * Dequeues an element from the priority queue, and returns it
 * Returns NULL if priority queue is empty
 *
 * @param pq The Priority Queue to enqueue to
 */
void* pq_dequeue(priority_queue* pq) {
    pthread_mutex_lock(&pq->pq_mutex);

    if (is_pq_empty(pq)) {
        perror("Cannot dequeue from an empty queue\n");
        pthread_mutex_unlock(&pq->pq_mutex);
        return NULL;
    }

    pq_element* elem = pq->queue[0];
    pq->queue[0] = pq->queue[--pq->size];

    unsigned int idx = 0;
    unsigned int left_idx = left_child_idx(idx);
    unsigned int right_idx = right_child_idx(idx);

    unsigned int swap_idx;

    while (left_idx < pq->size) {
        swap_idx = idx;
        if (pq->queue[left_idx]->priority >= pq->queue[swap_idx]->priority)
            swap_idx = left_idx;

        if (right_idx < pq->size && pq->queue[right_idx]->priority >= pq->queue[swap_idx]->priority)
            swap_idx = right_idx;

        if (swap_idx == idx)
            break;

        // Swap elements
        pq_element* temp = pq->queue[idx];
        pq->queue[idx] = pq->queue[swap_idx];
        pq->queue[swap_idx] = temp;

        // Update indexes
        idx = swap_idx;
        left_idx = left_child_idx(idx);
        right_idx = right_child_idx(idx);
    }

    pthread_mutex_unlock(&pq->pq_mutex);

    return elem;
}
