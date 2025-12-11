// Producer-Consumer Problem using Channels
// Part 3.1.2 - Demonstrates message-passing concurrency

#include "../src/uthreads.h"

#define ITEMS_PER_PRODUCER 10
#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 2
#define TOTAL_ITEMS (NUM_PRODUCERS * ITEMS_PER_PRODUCER)
#define CHANNEL_CAPACITY 5

// Shared channel
channel_t *item_channel;

// Track statistics
int total_produced = 0;
int total_consumed = 0;
mutex_t stats_mutex;

// Producer thread function
void* producer(void *arg) {
    int producer_id = *(int*)arg;

    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
        // Create an item
        int item = producer_id * 100 + i;

        // Send item through channel (blocks if channel is full)
        int result = channel_send(item_channel, (void*)(long)item);

        if (result == 0) {
            printf("Producer %d: produced item %d\n", producer_id, item);

            // Update stats
            mutex_lock(&stats_mutex);
            total_produced++;
            mutex_unlock(&stats_mutex);
        } else {
            printf("Producer %d: channel closed, stopping\n", producer_id);
            break;
        }

        // Yield to allow other threads to run
        thread_yield();
    }

    printf("Producer %d: finished producing\n", producer_id);
    return 0;
}

// Consumer thread function
void* consumer(void *arg) {
    int consumer_id = *(int*)arg;
    int items_consumed = 0;

    while (1) {
        void *data;
        int result = channel_recv(item_channel, &data);

        if (result == -1) {
            // Channel is closed and empty
            printf("Consumer %d: channel closed, stopping\n", consumer_id);
            break;
        }

        int item = (int)(long)data;
        printf("Consumer %d: consumed item %d\n", consumer_id, item);

        items_consumed++;

        // Update stats
        mutex_lock(&stats_mutex);
        total_consumed++;
        mutex_unlock(&stats_mutex);

        // Yield to allow other threads to run
        thread_yield();
    }

    printf("Consumer %d: finished consuming %d items\n",
           consumer_id, items_consumed);
    return 0;
}

int main(void) {
    printf("Producer-Consumer Problem with Channels\n");
    printf("=======================================\n");
    printf("Channel capacity: %d\n", CHANNEL_CAPACITY);
    printf("Producers: %d (each produces %d items)\n",
           NUM_PRODUCERS, ITEMS_PER_PRODUCER);
    printf("Consumers: %d\n", NUM_CONSUMERS);
    printf("Total items: %d\n\n", TOTAL_ITEMS);

    // Initialize threading system
    thread_init();

    // Create channel
    item_channel = channel_create(CHANNEL_CAPACITY);
    if (item_channel == 0) {
        printf("Failed to create channel\n");
        exit();
    }

    // Initialize stats mutex
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

    printf("\nAll producers finished. Closing channel...\n");

    // Close channel to signal consumers that no more items will arrive
    channel_close(item_channel);

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
