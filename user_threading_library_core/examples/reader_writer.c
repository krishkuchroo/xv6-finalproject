// Reader-Writer Lock with Writer Priority
// Part 3.2 - Prevents writer starvation

#include "../src/uthreads.h"

#define NUM_READERS 3
#define NUM_WRITERS 2
#define READS_PER_READER 5
#define WRITES_PER_WRITER 3

// Shared data
int shared_data = 0;

// Reader-Writer synchronization state
struct rwlock {
    int readers_active;      // Number of readers currently reading
    int writers_waiting;     // Number of writers waiting
    int writer_active;       // 1 if a writer is writing, 0 otherwise
    mutex_t lock;            // Protects the state variables
    cond_t readers_ok;       // Condition for readers to proceed
    cond_t writers_ok;       // Condition for writers to proceed
};

struct rwlock rwlock;

// Initialize reader-writer lock
void rwlock_init(struct rwlock *rw) {
    rw->readers_active = 0;
    rw->writers_waiting = 0;
    rw->writer_active = 0;
    mutex_init(&rw->lock);
    cond_init(&rw->readers_ok);
    cond_init(&rw->writers_ok);
}

// Acquire read lock
void reader_lock(struct rwlock *rw) {
    mutex_lock(&rw->lock);

    // Wait if a writer is active or writers are waiting (writer priority)
    while (rw->writer_active || rw->writers_waiting > 0) {
        cond_wait(&rw->readers_ok, &rw->lock);
    }

    // Increment reader count
    rw->readers_active++;

    mutex_unlock(&rw->lock);
}

// Release read lock
void reader_unlock(struct rwlock *rw) {
    mutex_lock(&rw->lock);

    // Decrement reader count
    rw->readers_active--;

    // If this was the last reader and writers are waiting, wake a writer
    if (rw->readers_active == 0 && rw->writers_waiting > 0) {
        cond_signal(&rw->writers_ok);
    }

    mutex_unlock(&rw->lock);
}

// Acquire write lock
void writer_lock(struct rwlock *rw) {
    mutex_lock(&rw->lock);

    // Indicate that a writer is waiting
    rw->writers_waiting++;

    // Wait while readers are active or another writer is writing
    while (rw->readers_active > 0 || rw->writer_active) {
        cond_wait(&rw->writers_ok, &rw->lock);
    }

    // We're no longer waiting
    rw->writers_waiting--;

    // Mark writer as active
    rw->writer_active = 1;

    mutex_unlock(&rw->lock);
}

// Release write lock
void writer_unlock(struct rwlock *rw) {
    mutex_lock(&rw->lock);

    // Mark writer as inactive
    rw->writer_active = 0;

    // If writers are waiting, wake one (writer priority)
    if (rw->writers_waiting > 0) {
        cond_signal(&rw->writers_ok);
    } else {
        // Otherwise, wake all readers
        cond_broadcast(&rw->readers_ok);
    }

    mutex_unlock(&rw->lock);
}

// Reader thread function
void* reader(void *arg) {
    int reader_id = *(int*)arg;

    for (int i = 0; i < READS_PER_READER; i++) {
        // Acquire read lock
        reader_lock(&rwlock);

        // Read the shared data
        int value = shared_data;
        printf("Reader %d: reading value = %d\n", reader_id, value);

        // Simulate reading time
        for (int j = 0; j < 100; j++) {
            thread_yield();
        }

        // Release read lock
        reader_unlock(&rwlock);

        // Yield between operations
        thread_yield();
    }

    printf("Reader %d: finished all reads\n", reader_id);
    return 0;
}

// Writer thread function
void* writer(void *arg) {
    int writer_id = *(int*)arg;

    for (int i = 0; i < WRITES_PER_WRITER; i++) {
        // Acquire write lock
        writer_lock(&rwlock);

        // Write new value to shared data
        shared_data++;
        printf("Writer %d: wrote new value = %d\n", writer_id, shared_data);

        // Simulate writing time
        for (int j = 0; j < 100; j++) {
            thread_yield();
        }

        // Release write lock
        writer_unlock(&rwlock);

        // Yield between operations
        thread_yield();
    }

    printf("Writer %d: finished all writes\n", writer_id);
    return 0;
}

int main(void) {
    printf("Reader-Writer Lock with Writer Priority\n");
    printf("=======================================\n");
    printf("Readers: %d (each performs %d reads)\n",
           NUM_READERS, READS_PER_READER);
    printf("Writers: %d (each performs %d writes)\n",
           NUM_WRITERS, WRITES_PER_WRITER);
    printf("Initial shared data: %d\n\n", shared_data);

    // Initialize threading system
    thread_init();

    // Initialize reader-writer lock
    rwlock_init(&rwlock);

    int reader_tids[NUM_READERS];
    int reader_args[NUM_READERS];
    int writer_tids[NUM_WRITERS];
    int writer_args[NUM_WRITERS];

    // Create reader threads
    for (int i = 0; i < NUM_READERS; i++) {
        reader_args[i] = i + 1;
        reader_tids[i] = thread_create(reader, &reader_args[i]);
        printf("Created Reader %d (TID: %d)\n", i + 1, reader_tids[i]);
    }

    // Create writer threads
    for (int i = 0; i < NUM_WRITERS; i++) {
        writer_args[i] = i + 1;
        writer_tids[i] = thread_create(writer, &writer_args[i]);
        printf("Created Writer %d (TID: %d)\n", i + 1, writer_tids[i]);
    }

    printf("\nStarting readers and writers...\n\n");

    // Wait for all readers to complete
    for (int i = 0; i < NUM_READERS; i++) {
        thread_join(reader_tids[i]);
    }

    // Wait for all writers to complete
    for (int i = 0; i < NUM_WRITERS; i++) {
        thread_join(writer_tids[i]);
    }

    printf("\n=== Final Results ===\n");
    printf("Final shared data value: %d\n", shared_data);
    printf("Expected value: %d\n", NUM_WRITERS * WRITES_PER_WRITER);

    if (shared_data == NUM_WRITERS * WRITES_PER_WRITER) {
        printf("SUCCESS! All writes completed correctly.\n");
    } else {
        printf("ERROR! Write count mismatch.\n");
    }

    exit();
}
