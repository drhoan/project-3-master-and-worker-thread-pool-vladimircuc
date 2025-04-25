#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// command-line args
int total_items, max_buf_size, num_workers, num_masters;

// ring buffer and indices
int *buffer;
int head = 0, tail = 0, count = 0;

// next item to produce
int item_to_produce = 0;

// synchronization
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_full  = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

// provided print functions
void print_produced(int num, int master) {
    printf("Produced %d by master %d\n", num, master);
}
void print_consumed(int num, int worker) {
    printf("Consumed %d by worker %d\n", num, worker);
}

// Producer (master) thread function
void *master_fn(void *arg) {
    int id = *(int*)arg;
    while (1) {
        pthread_mutex_lock(&mutex);

        // if all items produced, exit
        if (item_to_produce >= total_items) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // wait while buffer is full
        while (count == max_buf_size)
            pthread_cond_wait(&not_full, &mutex);

        // produce next item
        int item = item_to_produce++;
        buffer[tail] = item;
        tail = (tail + 1) % max_buf_size;
        count++;

        print_produced(item, id);

        // signal a consumer
        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// Consumer (worker) thread function
void *worker_fn(void *arg) {
    int id = *(int*)arg;
    while (1) {
        pthread_mutex_lock(&mutex);

        // wait while buffer is empty and producers still working
        while (count == 0 && item_to_produce < total_items)
            pthread_cond_wait(&not_empty, &mutex);

        // if no items left and production done, exit
        if (count == 0 && item_to_produce >= total_items) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // consume one item
        int item = buffer[head];
        head = (head + 1) % max_buf_size;
        count--;

        print_consumed(item, id);

        // signal a producer
        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr,
            "Usage: %s #total_items #max_buf_size #num_workers #num_masters\n",
            argv[0]);
        return 1;
    }

    total_items  = atoi(argv[1]);
    max_buf_size = atoi(argv[2]);
    num_workers  = atoi(argv[3]);
    num_masters  = atoi(argv[4]);

    buffer = malloc(sizeof(int) * max_buf_size);
    if (!buffer) {
        perror("malloc");
        return 1;
    }

    // spawn master threads
    pthread_t *masters = malloc(sizeof(pthread_t) * num_masters);
    int *master_ids   = malloc(sizeof(int)       * num_masters);
    for (int i = 0; i < num_masters; i++) {
        master_ids[i] = i;
        pthread_create(&masters[i], NULL, master_fn, &master_ids[i]);
    }

    // spawn worker threads
    pthread_t *workers = malloc(sizeof(pthread_t) * num_workers);
    int *worker_ids   = malloc(sizeof(int)        * num_workers);
    for (int i = 0; i < num_workers; i++) {
        worker_ids[i] = i;
        pthread_create(&workers[i], NULL, worker_fn, &worker_ids[i]);
    }

    // join masters
    for (int i = 0; i < num_masters; i++) {
        pthread_join(masters[i], NULL);
        printf("master %d joined\n", i);
    }

    // wake up all consumers in case they're waiting
    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&not_empty);
    pthread_mutex_unlock(&mutex);

    // join workers
    for (int i = 0; i < num_workers; i++) {
        pthread_join(workers[i], NULL);
        printf("worker %d joined\n", i);
    }

    // cleanup
    free(buffer);
    free(masters);
    free(master_ids);
    free(workers);
    free(worker_ids);

    return 0;
}
