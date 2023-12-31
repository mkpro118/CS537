#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "safequeue.h"

#define NUM_THREADS 10
#define NUM_OPERATIONS 100000

#define assert(cond, a, b, f1, f2)  if (a cond b) {printf("Assertion Failed! " #a " ( " #f1 " ) " #cond " " #b " ( " #f2 " ) [Line: %d]\n", a, b, __LINE__); fflush(stdout); exit(1);}

#define assert_eq(a, b, f1, f2) assert(!=, a, b, f1, f2)
#define assert_ne(a, b, f1, f2) assert(==, a, b, f1, f2)
#define assert_gt(a, b, f1, f2) assert(<=, a, b, f1, f2)

#define LINE "========================================\n"

typedef struct {
    priority_queue* pq;
    int val;
} args;


int retvals[NUM_THREADS * NUM_OPERATIONS][2];
int n;

pthread_mutex_t retval_lock;


void test_pq_basic() {
    // Test Case 1: Initialization
    printf("\n" LINE);
    printf("Testing create_queue\n");
    priority_queue* pq = create_queue(10);
    assert_ne(pq, NULL, "%p", "%p");
    printf("pq != NULL:                     | PASSED\n");
    assert_eq(pq->capacity, 10, "%d", "%d");
    printf("pq->capacity == 10:             | PASSED\n");
    assert_eq(pq->size, 0, "%d", "%d");
    printf("pq->size == 0:                  | PASSED\n");

    // Prepare a pq_element for testing
    pq_element* elem = malloc(sizeof(pq_element));
    int val = 100;
    elem->value = (void*) &val;
    elem->priority = 100;

    // Test Case 2: Enqueue
    printf(LINE);
    printf("Testing add_work\n");
    int enqueue_result = add_work(pq, elem);
    assert_eq(enqueue_result, 0, "%d", "%d");
    printf("enqueue_result == 0:            | PASSED\n");
    assert_eq(pq->size, 1, "%d", "%d");
    printf("pq->size == 1:                  | PASSED\n");

    // Test Case 3: Enqueue when Full
    printf(LINE);
    printf("Testing add_work after full\n");
    for (int i = 0; i < 9; i++) {
        pq_element* new_elem = malloc(sizeof(pq_element));
        new_elem->value = (void*) &i;
        new_elem->priority = i;
        enqueue_result = add_work(pq, new_elem);
        assert_eq(enqueue_result, 0, "%d", "%d");
    }

    pq_element* overflow_elem = malloc(sizeof(pq_element));
    int overflow_value = 10;
    overflow_elem->value = (void*) &overflow_value;
    overflow_elem->priority = 10;
    enqueue_result = add_work(pq, overflow_elem);
    assert_eq(enqueue_result, -1, "%d", "%d");
    printf("enqueue_result == -1:           | PASSED\n");
    assert_eq(pq->size, 10, "%d", "%d");
    printf("pq->size == 10:                 | PASSED\n");

    // Test Case 4: Dequeue
    printf(LINE);
    printf("Testing get_work_nonblocking\n");
    int* dequeued_elem = (int*) get_work_nonblocking(pq);
    assert_eq(*dequeued_elem, *((int*)(elem->value)), "%d", "%d");
    printf("*dequeued_elem == *elem->value: | PASSED\n");
    assert_eq(pq->size, 9, "%d", "%d");
    printf("pq->size == 9:                  | PASSED\n");

    printf(LINE);

    // Test Case 5: Dequeue when Empty
    printf("Testing get_work_nonblocking when empty\n");
    while (pq->size != 0) {
        dequeued_elem = get_work_nonblocking(pq);
        assert_ne(dequeued_elem, NULL, "%p", "%p");
    }

    dequeued_elem = get_work_nonblocking(pq);
    assert_eq(dequeued_elem, NULL, "%p", "%p");
    printf("dequeued_elem == NULL:          | PASSED\n");
    assert_eq(pq->size, 0, "%d", "%d");
    printf("pq->size == 0:                  | PASSED\n");

    // Test Case 6: Destroy
    pq_destroy(pq);
}

void test_pq_order() {
    printf(LINE);
    printf("Testing PQ ordering ");
    fflush(stdout);
    // Initialization
    priority_queue* pq = create_queue(10);
    assert_ne(pq, NULL, "%p", "%p");
    assert_eq(pq->capacity, 10, "%d", "%d");
    assert_eq(pq->size, 0, "%d", "%d");

    // Prepare pq_elements for testing
    pq_element* elems[10];
    for (int i = 0; i < 10; i++) {
        elems[i] = malloc(sizeof(pq_element));
        elems[i]->value = (void*) &i;
        elems[i]->priority = i;
    }

    // Enqueue elements with increasing priority
    for (int i = 0; i < 10; i++) {
        int enqueue_result = add_work(pq, elems[i]);
        assert_eq(enqueue_result, 0, "%d", "%d");
        assert_eq(pq->size, i + 1, "%d", "%d");
    }

    // Dequeue elements and check if they are in decreasing order of priority
    for (int i = 9; i >= 0; i--) {
        void* dequeued_elem = get_work_nonblocking(pq);
        assert_eq(dequeued_elem, elems[i]->value, "%p", "%p");
        printf(".");
        fflush(stdout);
        assert_eq(pq->size, i, "%d", "%d");
    }
    printf("  | PASSED\n");

    printf("pq->size == 0:                  | PASSED\n");

    // Destroy
    pq_destroy(pq);
}

void* thread_enqueue(void* arg) {
    priority_queue* pq = (priority_queue*)(((args*) arg)->pq);

    int tid = ((args*) arg)->val;
    int val = tid * NUM_OPERATIONS;

    for (int i = val; i < val + NUM_OPERATIONS; i++) {
        int* x = malloc(sizeof(int));
        *x = i;
        pq_element* elem = malloc(sizeof(pq_element));
        elem->value = (void*) x;
        elem->priority = *x;

        assert_eq(add_work(pq, elem), 0, "%d", "%d");
    }
    return NULL;
}

void* thread_dequeue(void* arg) {
    priority_queue* pq = (priority_queue*)(((args*) arg)->pq);

    for (int i = 0; i < NUM_OPERATIONS; i++) {
        pthread_mutex_lock(&retval_lock);
        int idx = n++;
        int* x = (int*) get_work_nonblocking(pq);
        pthread_mutex_unlock(&retval_lock);

        retvals[idx][0] = *x;
        retvals[idx][1] = ((args*) arg)->val;
    }
    return NULL;
}


void test_pq_thread_safety() {
    // Initialization
    printf("Starting thread safety test...\n");
    priority_queue* pq = create_queue(NUM_THREADS * NUM_OPERATIONS);
    assert_ne(pq, NULL, "%p", "%p");

    // Create threads
    pthread_t threads[NUM_THREADS];
    args thread_args[NUM_THREADS];

    printf("\nCreating enqueue threads ");
    fflush(stdout);
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_args[i].pq = pq;
        thread_args[i].val = i;
        if (pthread_create(&threads[i], NULL, thread_enqueue, &thread_args[i])) {
            printf("FATAL ERROR: Couldn't create a Thread!\n");
            pq_destroy(pq);
            exit(1);
        }
        printf(".");
        fflush(stdout);
    }
    printf(" Done\n");

    // Wait for threads to finish
   printf("Joining  enqueue threads ");
   fflush(stdout);
   for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        printf(".");
        fflush(stdout);
    }

    printf(" Done\n");

    printf("\nCreating dequeue threads ");
    fflush(stdout);
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, thread_dequeue, &thread_args[i])) {
            printf("FATAL ERROR: Couldn't create a Thread!\n");
            pq_destroy(pq);
            exit(1);
        }
        printf(".");
        fflush(stdout);
    }
    printf(" Done\n");

    // Wait for threads to finish
    printf("Joining  dequeue threads ");
    fflush(stdout);
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        printf(".");
        fflush(stdout);
    }

    printf(" Done\n");

    for (int i = 0; i < NUM_THREADS * NUM_OPERATIONS - 1; i++) {
        if (retvals[i][0] <= retvals[i+1][0]) {
            printf("i = %d | retvals[i] = %d | retvals[i + 1] = %d\n", i, retvals[i][0], retvals[i+1][0]);
            for (int i = 0; i <NUM_OPERATIONS; i++) {
                printf("[%d: %d]", retvals[i][1], retvals[i][0]);
            }
			printf("\n");
        }
        assert_gt(retvals[i][0], retvals[i+1][0], "%d", "%d");
    }

    printf("Dequeue Priority order:         | PASSED\n");

    // Check if the queue is empty
    assert_eq(pq->size, 0, "%d", "%d");

    // Destroy
    pq_destroy(pq);
}

int main() {
    n = 0;
    pthread_mutex_init(&retval_lock, NULL);

    printf("\n\nStarting tests...\n\n");
    test_pq_basic();
    printf(LINE);
    printf("        All basic tests PASSED!         \n");
    printf(LINE);

    test_pq_order();
    printf(LINE);
    printf("  All single thread order tests PASSED! \n");
    printf(LINE);

    test_pq_thread_safety();

    printf(LINE);
    printf("     All thread safety tests passed!    \n");
    printf(LINE);

    printf(LINE);
    printf("            All tests passed!           \n");
    printf(LINE);

    pthread_mutex_destroy(&retval_lock);
    return 0;
}
