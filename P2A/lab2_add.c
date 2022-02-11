#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>

long long counter;
int opt_yield, opt_m, opt_s, opt_c;
pthread_mutex_t mutex_lock;
int spin_lock;

void add(long long *pointer, long long value) 
{
    if (opt_m) {
        pthread_mutex_lock(&mutex_lock);
        long long sum = *pointer + value;
        if (opt_yield) sched_yield();
        *pointer = sum;
        pthread_mutex_unlock(&mutex_lock);
    }
    else if (opt_s)
    {
        while (__sync_lock_test_and_set(&spin_lock, 1));
        long long sum = *pointer + value;
        if (opt_yield) sched_yield();
        *pointer = sum;
        __sync_lock_release(&spin_lock);
    }
    else if (opt_c)
    {
        long long prev, sum;
        do 
        {
            prev = *pointer;
            sum = prev + value;
            if (opt_yield) sched_yield();
        } while (__sync_val_compare_and_swap(pointer, prev, sum) != prev);
    }
    else {
        long long sum = *pointer + value;
        if (opt_yield) sched_yield();
        *pointer = sum;
    }
}

void* thread_worker(void* p)
{
    int iterations =*((int*) p);
    int i;
    for (i = 0; i < iterations; ++i) add(&counter, 1);
    for (i = 0; i < iterations; ++i) add(&counter, -1);
    return NULL;
}

int main(int argc, char* argv[]) 
{
    int num_threads = 1, num_iterations = 1;
    counter = 0;
    opt_yield = 0, opt_m = 0, opt_c = 0, opt_s = 0;
    int option_index;
    char c;
    static struct option long_options[] =
    {
        {"threads", optional_argument, 0, 't'},
        {"iterations", optional_argument, 0, 'i'},
        {"yield", no_argument, 0, 'y'},
        {"sync", required_argument, 0, 's'},
        {0, 0, 0, 0}
    };
    while (1)
    {
        option_index = 0;
        c = getopt_long(argc, argv, "", long_options, &option_index);
        if (c == -1) break;
        switch (c)
        {
            case 't':
                if (optarg) num_threads = atoi(optarg);
                break;
            case 'i':
                if (optarg) num_iterations = atoi(optarg);
                break;
            case 'y':
                opt_yield = 1;
                break;
            case 's':
                if (strlen(optarg) > 1)
                {
                    fprintf(stderr, "Invalid --sync argument, accepted arguments are m, c, s\n");
                    exit(1);
                }
                if (optarg[0] == 's') opt_s = 1;
                else if (optarg[0] == 'c') opt_c = 1;
                else if (optarg[0] == 'm') opt_m = 1;
                else 
                {
                    fprintf(stderr, "Invalid --sync argument, accepted arguments are m, c, or s\n");
                    exit(1);
                }
                break;
            default:
                fprintf(stderr, "Accepted options are: lab2-add [--threads=optional_arg] [--iterations=optional_arg] [--yield]\n");
                exit(1);
        }
    }

    struct timespec start, end;
    long long total_runtime, num_ops, time_per_op;
    int i;

    char name[20] = "add";
    if (opt_yield) strcat(name, "-yield");
    if (opt_m) strcat(name, "-m");
    else if (opt_s) strcat(name, "-s");
    else if (opt_c) strcat(name, "-c");
    else strcat(name, "-none");

    pthread_t* threads = (pthread_t*) malloc(num_threads * sizeof(pthread_t));
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0)
    {
        fprintf(stderr, "Error getting start time: %s\n", strerror(errno));
        exit(1);
    }

    if (opt_m) pthread_mutex_init(&mutex_lock, NULL);
    if (opt_s) spin_lock = 0;

    for (i = 0; i < num_threads; ++i)
    {
        if (pthread_create(&threads[i], NULL, thread_worker, (void*) &num_iterations) != 0)
        {
            fprintf(stderr, "Error creating thread: %s\n", strerror(errno));
            exit(1);
        }
    }
    for (i = 0; i < num_threads; ++i) 
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            fprintf(stderr, "Error joining thread: %s\n", strerror(errno));
            exit(1);
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end))
    {
        fprintf(stderr, "Error getting end time: %s\n", strerror(errno));
        exit(1);
    }
    total_runtime = 1000000000 * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
    num_ops = num_threads * num_iterations * 2;
    time_per_op = total_runtime/num_ops;
    printf("%s,%d,%d,%lld,%lld,%lld,%lld\n", name, num_threads, num_iterations, num_ops, total_runtime, time_per_op, counter);
    exit(0);
}
