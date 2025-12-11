// Test program for mutex implementation
// Demonstrates shared counter problem with and without mutex protection

#include "../src/uthreads.h"

// Shared counter
int shared_counter = 0;

// Mutex to protect the counter
mutex_t counter_mutex;

// Flag to control whether to use mutex
int use_mutex = 1;

// Number of increments per thread
#define INCREMENTS_PER_THREAD 1000

// Thread function: increment the shared counter
void* increment_counter(void *arg) {
    int thread_num = *(int*)arg;

    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
        if (use_mutex) {
            mutex_lock(&counter_mutex);
        }

        // Critical section - increment counter
        int temp = shared_counter;
        // Yield to encourage race conditions if mutex is not used
        thread_yield();
        shared_counter = temp + 1;

        if (use_mutex) {
            mutex_unlock(&counter_mutex);
        }
    }

    return 0;
}

// Test without mutex (demonstrates race condition)
void test_without_mutex(void) {
    printf("=== Test WITHOUT Mutex ===\n");

    use_mutex = 0;
    shared_counter = 0;

    int num_threads = 3;
    int tids[3];
    int thread_args[3];

    // Create threads
    for (int i = 0; i < num_threads; i++) {
        thread_args[i] = i;
        tids[i] = thread_create(increment_counter, &thread_args[i]);
        printf("Created thread %d (TID: %d)\n", i, tids[i]);
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        thread_join(tids[i]);
        printf("Thread %d completed\n", tids[i]);
    }

    printf("Expected counter value: %d\n", num_threads * INCREMENTS_PER_THREAD);
    printf("Actual counter value: %d\n", shared_counter);

    if (shared_counter != num_threads * INCREMENTS_PER_THREAD) {
        printf("RACE CONDITION DETECTED! Counter is incorrect.\n");
    } else {
        printf("Counter is correct (got lucky without mutex)\n");
    }
    printf("\n");
}

// Test with mutex (correct behavior)
void test_with_mutex(void) {
    printf("=== Test WITH Mutex ===\n");

    use_mutex = 1;
    shared_counter = 0;
    mutex_init(&counter_mutex);

    int num_threads = 3;
    int tids[3];
    int thread_args[3];

    // Create threads
    for (int i = 0; i < num_threads; i++) {
        thread_args[i] = i;
        tids[i] = thread_create(increment_counter, &thread_args[i]);
        printf("Created thread %d (TID: %d)\n", i, tids[i]);
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        thread_join(tids[i]);
        printf("Thread %d completed\n", tids[i]);
    }

    printf("Expected counter value: %d\n", num_threads * INCREMENTS_PER_THREAD);
    printf("Actual counter value: %d\n", shared_counter);

    if (shared_counter == num_threads * INCREMENTS_PER_THREAD) {
        printf("SUCCESS! Counter is correct with mutex protection.\n");
    } else {
        printf("FAILURE! Counter is incorrect even with mutex.\n");
    }
    printf("\n");
}

int main(void) {
    printf("Shared Counter Test\n");
    printf("===================\n\n");

    // Initialize threading system
    thread_init();

    // Run test without mutex first (to show the problem)
    test_without_mutex();

    // Run test with mutex (to show the solution)
    test_with_mutex();

    printf("All tests completed.\n");

    exit();
}
