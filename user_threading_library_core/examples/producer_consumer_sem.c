// Producer-Consumer Problem using Semaphores
// Part 3.1.1 - Classic bounded buffer problem

#include "../src/uthreads.h"

#define BUFFER_SIZE 5
#define ITEMS_PER_PRODUCER 10
#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 2
#define TOTAL_ITEMS (NUM_PRODUCERS * ITEMS_PER_PRODUCER)

// Bounded buffer
int buffer[BUFFER_SIZE];
int in = 0;   // Write position
int out = 0;  // Read position

// Synchronization primitives
sem_t empty;  // Tracks empty slots
sem_t full;   // Tracks filled slots
mutex_t buffer_mutex; // Protects buffer access

// Track total items produced and consumed
int total_produced = 0;
int total_consumed = 0;
mutex_t stats_mutex;

// Producer thread function
void* producer(void *arg) {
    int producer_id = *(int*)arg;

    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
        // Create an item (just a number for this demo)
        int item = producer_id * 100 + i;

        // Wait for empty slot
        sem_wait(&empty);

        // Acquire buffer lock
        mutex_lock(&buffer_mutex);

        // Add item to buffer
        buffer[in] = item;
        printf("Producer %d: produced item %d (buffer pos %d)\n",
               producer_id, item, in);
        in = (in + 1) % BUFFER_SIZE;

        // Update stats
        mutex_lock(&stats_mutex);
        total_produced++;
        mutex_unlock(&stats_mutex);

        // Release buffer lock
        mutex_unlock(&buffer_mutex);

        // Signal that buffer has a filled slot
        sem_post(&full);

        // Yield to allow other threads to run
        thread_yield();
    }

    printf("Producer %d: finished producing %d items\n",
           producer_id, ITEMS_PER_PRODUCER);
    return 0;
}

// Consumer thread function
void* consumer(void *arg) {
    int consumer_id = *(int*)arg;
    int items_consumed = 0;

    while (1) {
        // Check if all items have been consumed
        mutex_lock(&stats_mutex);
        int consumed = total_consumed;
        mutex_unlock(&stats_mutex);

        if (consumed >= TOTAL_ITEMS) {
            break;
        }

        // Wait for filled slot
        sem_wait(&full);

        // Acquire buffer lock
        mutex_lock(&buffer_mutex);

        // Check again after acquiring lock
        mutex_lock(&stats_mutex);
        if (total_consumed >= TOTAL_ITEMS) {
            mutex_unlock(&stats_mutex);
            mutex_unlock(&buffer_mutex);
            sem_post(&full); // Put back the semaphore
            break;
        }
        mutex_unlock(&stats_mutex);

        // Remove item from buffer
        int item = buffer[out];
        printf("Consumer %d: consumed item %d (buffer pos %d)\n",
               consumer_id, item, out);
        out = (out + 1) % BUFFER_SIZE;

        // Update stats
        mutex_lock(&stats_mutex);
        total_consumed++;
        items_consumed++;
        mutex_unlock(&stats_mutex);

        // Release buffer lock
        mutex_unlock(&buffer_mutex);

        // Signal that buffer has an empty slot
        sem_post(&empty);

        // Yield to allow other threads to run
        thread_yield();
    }

    printf("Consumer %d: finished consuming %d items\n",
           consumer_id, items_consumed);
    return 0;
}

int main(void) {
    printf("Producer-Consumer Problem with Semaphores\n");
    printf("=========================================\n");
    printf("Buffer size: %d\n", BUFFER_SIZE);
    printf("Producers: %d (each produces %d items)\n",
           NUM_PRODUCERS, ITEMS_PER_PRODUCER);
    printf("Consumers: %d\n", NUM_CONSUMERS);
    printf("Total items: %d\n\n", TOTAL_ITEMS);

    // Initialize threading system
    thread_init();

    // Initialize synchronization primitives
    sem_init(&empty, BUFFER_SIZE);  // Initially, all slots are empty
    sem_init(&full, 0);              // Initially, no slots are full
    mutex_init(&buffer_mutex);
    mutex_init(&stats_mutex);

    int producer_tids[NUM_PRODUCERS];
    int producer_args[NUM_PRODUCERS];
    int consumer_tids[NUM_CONSUMERS];
    int consumer_args[NUM_CONSUMERS];

    // Create producer threads
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producer_args[i] = i + 1;
        producer_tids[i] = thread_create(producer, &producer_args[i]);
        printf("Created Producer %d (TID: %d)\n", i + 1, producer_tids[i]);
    }

    // Create consumer threads
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumer_args[i] = i + 1;
        consumer_tids[i] = thread_create(consumer, &consumer_args[i]);
        printf("Created Consumer %d (TID: %d)\n", i + 1, consumer_tids[i]);
    }

    printf("\nStarting production and consumption...\n\n");

    // Wait for all producers to complete
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        thread_join(producer_tids[i]);
    }

    // Wait for all consumers to complete
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        thread_join(consumer_tids[i]);
    }

    printf("\n=== Final Results ===\n");
    printf("Total produced: %d\n", total_produced);
    printf("Total consumed: %d\n", total_consumed);

    if (total_consumed == TOTAL_ITEMS) {
        printf("SUCCESS! All items processed correctly.\n");
    } else {
        printf("ERROR! Item count mismatch.\n");
    }

    exit();
}
