#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "safequeue.h"

#define NUM_THREADS 10
#define NUM_OPERATIONS 100

void test_pq_basic() {
    // Test Case 1: Initialization
    printf("========================================\n");
    printf("Testing pq_init\n");
    priority_queue* pq = pq_init(10);
    assert(pq != NULL);
    printf("pq != NULL: PASSED\n");
    assert(pq->capacity == 10);
    printf("pq->capacity == 10: PASSED\n");
    assert(pq->size == 0);
    printf("pq->size == 0: PASSED\n");

    // Prepare a pq_element for testing
    pq_element* elem = malloc(sizeof(pq_element));
    int val = 1;
    elem->value = (void*) &val;
    elem->priority = 1;

    // Test Case 2: Enqueue
    printf("\n\n========================================\n");
    printf("Testing pq_enqueue\n");
    int enqueue_result = pq_enqueue(pq, elem);
    assert(enqueue_result == 0);
    printf("enqueue_result == 0: PASSED\n");
    assert(pq->size == 1);
    printf("pq->size == 1: PASSED\n");

    // Test Case 3: Enqueue when Full
    printf("\n\n========================================\n");
    printf("Testing pq_enqueue after full\n");
    for (int i = 0; i < 9; i++) {
        pq_element* new_elem = malloc(sizeof(pq_element));
        new_elem->value = (void*) &i;
        new_elem->priority = i;
        pq_enqueue(pq, new_elem);
    }

    pq_element* overflow_elem = malloc(sizeof(pq_element));
    int overflow_value = 10;
    overflow_elem->value = (void*) &overflow_value;
    overflow_elem->priority = 10;
    enqueue_result = pq_enqueue(pq, overflow_elem);
    assert(enqueue_result == -1);
    printf("enqueue_result == -1: PASSED\n");
    assert(pq->size == 10);
    printf("pq->size == 10: PASSED\n");

    // Test Case 4: Dequeue
    printf("\n\n========================================\n");
    printf("Testing pq_dequeue\n");
    void* dequeued_elem = pq_dequeue(pq);
    assert(dequeued_elem == elem->value);
    printf("dequeued_elem == elem->value: PASSED\n");
    assert(pq->size == 9);
    printf("pq->size == 9: PASSED\n");

    printf("\n\n========================================\n");

    // Test Case 5: Dequeue when Empty
    printf("Testing pq_dequeue when empty\n");
    while (!(pq->size == 0)) {
        pq_dequeue(pq);
    }
    dequeued_elem = pq_dequeue(pq);
    assert(dequeued_elem == NULL);
    printf("dequeued_elem == NULL: PASSED\n");
    assert(pq->size == 0);
    printf("pq->size == 0: PASSED\n");

    // Test Case 6: Destroy
    pq_destroy(pq);
}

void test_pq_order() {
    printf("\n\n========================================\n");
    printf("Testing pq_dequeue order\n");
    // Initialization
    priority_queue* pq = pq_init(10);
    assert(pq != NULL);
    assert(pq->capacity == 10);
    assert(pq->size == 0);

    // Prepare pq_elements for testing
    pq_element* elems[10];
    for (int i = 0; i < 10; i++) {
        elems[i] = malloc(sizeof(pq_element));
        elems[i]->value = (void*) &i;
        elems[i]->priority = i;
    }

    // Enqueue elements with increasing priority
    for (int i = 0; i < 10; i++) {
        int enqueue_result = pq_enqueue(pq, elems[i]);
        assert(enqueue_result == 0);
        assert(pq->size == i + 1);
    }

    // Dequeue elements and check if they are in decreasing order of priority
    for (int i = 9; i >= 0; i--) {
        void* dequeued_elem = pq_dequeue(pq);
        assert(dequeued_elem == elems[i]->value);
        printf(".");
        fflush(stdout);
        assert(pq->size == i);
    }

    printf("\npq->size == 0: PASSED\n");

    // Destroy
    pq_destroy(pq);
}

void* thread_func(void* arg) {
    priority_queue* pq = (priority_queue*)arg;
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        pq_element* elem = malloc(sizeof(pq_element));
        elem->value = (void*) &i;
        elem->priority = i;
        pq_enqueue(pq, elem);
        void* dequeued_elem = pq_dequeue(pq);
        assert(dequeued_elem == elem->value);
        free(elem);
    }
    return NULL;
}

void test_pq_thread_safety() {
    // Initialization
    priority_queue* pq = pq_init(NUM_THREADS * NUM_OPERATIONS);
    assert(pq != NULL);

    // Create threads
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, thread_func, pq);
    }

    // Wait for threads to finish
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Check if the queue is empty
    assert(pq->size == 0);

    // Destroy
    pq_destroy(pq);
}

int main() {
    test_pq_basic();
    printf("\n=======================\n");
    printf("All basic tests passed!\n");
    printf("=======================\n");

    test_pq_order();
    printf("\n=======================\n");
    printf("All order tests passed!\n");
    printf("=======================\n");

    test_pq_thread_safety();

    printf("\n===============================\n");
    printf("All thread safety tests passed!\n");
    printf("===============================\n");

    printf("All tests passed!\n");
    return 0;
}
