// Basic threading test - tests thread creation, execution, and joining

#include "../src/uthreads.h"

// Simple thread function
void* simple_thread(void *arg) {
    int thread_num = *(int*)arg;

    printf("Thread %d: Hello from thread!\n", thread_num);

    // Yield a few times to demonstrate context switching
    for (int i = 0; i < 3; i++) {
        printf("Thread %d: Iteration %d\n", thread_num, i);
        thread_yield();
    }

    printf("Thread %d: Exiting\n", thread_num);
    return (void*)(long)(thread_num * 100);
}

int main(void) {
    printf("Basic Threading Test\n");
    printf("===================\n\n");

    // Initialize threading system
    thread_init();
    printf("Threading system initialized\n");
    printf("Main thread TID: %d\n\n", thread_self());

    // Create multiple threads
    int num_threads = 3;
    int tids[3];
    int thread_args[3];

    printf("Creating %d threads...\n", num_threads);
    for (int i = 0; i < num_threads; i++) {
        thread_args[i] = i + 1;
        tids[i] = thread_create(simple_thread, &thread_args[i]);
        printf("Created thread %d (TID: %d)\n", i + 1, tids[i]);
    }
    printf("\n");

    // Yield to let threads run
    printf("Main thread yielding to let threads run...\n\n");
    thread_yield();

    // Join all threads and collect return values
    printf("Main thread joining threads...\n");
    for (int i = 0; i < num_threads; i++) {
        void *retval = thread_join(tids[i]);
        printf("Joined thread %d, return value: %d\n",
               tids[i], (int)(long)retval);
    }

    printf("\nAll threads completed successfully!\n");

    exit();
}
