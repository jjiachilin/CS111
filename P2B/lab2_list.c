#include "SortedList.h"
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

SortedList_t* listheads;
SortedListElement_t* pool;
int num_threads, num_iterations, num_lists;
int opt_m, opt_s, opt_yield;
pthread_mutex_t* mutex_locks;
int* spin_locks;
unsigned long* wait_time;

unsigned int hashkey(const char* s) // based on djb2 hash function by Dan Bernstein
{
    unsigned int hash = 13;
    int c;
    while ((c = *s++)) 
        hash = ((hash << 5) + hash) + c;
    return hash % num_lists;
}

void handler(int sig)
{
    if (sig == SIGSEGV)
    {
        fprintf(stderr, "Caught segfault!\n");
        exit(2);
    }
}

void* thread_worker(void* p)
{
    struct timespec start, end;
    int threadNum = *((int *) p);
    int startIndex = threadNum * num_iterations;
    int i;

    // insert elements
    for (i = startIndex; i < (threadNum + 1) * num_iterations; ++i)
    {
        // hash key to get index of sublist
        unsigned int hash = hashkey(pool[i].key);
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        if (opt_m) pthread_mutex_lock(mutex_locks + hash);
        clock_gettime(CLOCK_MONOTONIC, &end);
        if (opt_s) while (__sync_lock_test_and_set(spin_locks + hash, 1));
        
        wait_time[threadNum] += 1000000000 * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);

        SortedList_insert(listheads + hash, &pool[i]);
        
        if (opt_m) pthread_mutex_unlock(mutex_locks + hash);
        else if (opt_s) __sync_lock_release(spin_locks + hash);
    }

    // check full list length
    for (i = 0; i < num_lists; ++i ){
        clock_gettime(CLOCK_MONOTONIC, &start);
        if (opt_m) pthread_mutex_lock(mutex_locks + i);
        clock_gettime(CLOCK_MONOTONIC, &end);
        if (opt_s) while (__sync_lock_test_and_set(spin_locks + i, 1));
        wait_time[threadNum] += 1000000000 * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
    }
    
    int total_length = 0;
    int cur_length;
    for (i = 0; i < num_lists; ++i) {
        cur_length = SortedList_length(listheads + i);
        if (cur_length >= 0) total_length += cur_length; 
    }
    if (total_length < 0) 
    {
        fprintf(stderr, "Error, corrupted list\n");
        exit(2);
    }

    for (i = 0; i < num_lists; ++i) {
        if (opt_m) pthread_mutex_unlock(mutex_locks + i);
        else if (opt_s) __sync_lock_release(spin_locks + i);
    }
    

    // lookup and then delete
    for (i = startIndex; i < startIndex + num_iterations; ++i)
    {
        unsigned int hash = hashkey(pool[i].key);
        clock_gettime(CLOCK_MONOTONIC, &start);
        if (opt_m) pthread_mutex_lock(mutex_locks + hash);
        clock_gettime(CLOCK_MONOTONIC, &end);
        if (opt_s) while (__sync_lock_test_and_set(spin_locks + hash, 1));
        wait_time[threadNum] += 1000000000 * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);

        SortedListElement_t* e = SortedList_lookup(listheads + hash, pool[i].key);
        SortedList_delete(e);

        if (opt_m) pthread_mutex_unlock(mutex_locks + hash);
        else if (opt_s) __sync_lock_release(spin_locks + hash);
    }
    return NULL;
}

int main(int argc, char* argv[]) 
{
    num_threads = 1;
    num_iterations = 1;
    num_lists = 1;
    opt_m = 0;
    opt_s = 0;
    opt_yield = 0;
    int option_index;
    int i, j;
    char c;
    static struct option long_options[] =
    {
        {"threads", optional_argument, 0, 't'},
        {"iterations", optional_argument, 0, 'i'},
        {"yield", required_argument, 0, 'y'},
        {"sync", required_argument, 0, 's'},
        {"lists", optional_argument, 0, 'l'},
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
                for (i = 0; i < (int) strlen(optarg); ++i )
                {
                    if (optarg[i] == 'i') opt_yield |= INSERT_YIELD;
                    else if (optarg[i] == 'd') opt_yield |= DELETE_YIELD;
                    else if (optarg[i] == 'l') opt_yield |= LOOKUP_YIELD;
                    else
                    {
                        fprintf(stderr, "Invalid --yield argument, accepted arguments are i, d, l");
                        exit(1);
                    }
                }
                break;
            case 's':
                if (strlen(optarg) > 1)
                {
                    fprintf(stderr, "Invalid --sync argument, accepted arguments are m, s\n");
                    exit(1);
                }
                if (optarg[0] == 's') opt_s = 1;
                else if (optarg[0] == 'm') opt_m = 1;
                else 
                {
                    fprintf(stderr, "Invalid --sync argument, accepted arguments are m or s\n");
                    exit(1);
                }
                break;
            case 'l':
                if (optarg) num_lists = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Accepted options are: lab2-add [--threads=optional_arg] [--iterations=optional_arg] [--yield=idl]\n");
                exit(1);
        }
    }

    // initialize everything
    if (opt_m) 
    {
        mutex_locks = malloc(sizeof(pthread_mutex_t) * num_lists);
        for (i = 0; i < num_lists; ++i) 
            pthread_mutex_init(&mutex_locks[i], NULL);
    }
    else if (opt_s)
    {
        spin_locks = malloc(sizeof(int) * num_lists);
        for (i = 0; i < num_lists; ++i)
            spin_locks[i] = 0;
    }

    listheads = (SortedList_t *) malloc(sizeof(SortedList_t) * num_lists);
    for (i = 0; i < num_lists; ++i) {
        listheads[i].next = &listheads[i];
        listheads[i].prev = &listheads[i];
        listheads[i].key = NULL;
    }

    pool = (SortedListElement_t*) malloc(num_threads * num_iterations * sizeof(SortedListElement_t));
    static const char alphanumeric[] = "QWERTYUIOPASDFGHJKLZXCVBNMqwertyuiopasdfghjklzxcvbnm1234567890";
    for (i = 0; i < num_threads * num_iterations; ++i)
    {
        int len = rand() % 10;
        char* key = (char *) malloc((len + 1) * sizeof(char));
        for (j = 0; j < len; ++j)
        {
            key[j] = alphanumeric[rand() % strlen(alphanumeric)];
        }
        key[len] = '\0';
        pool[i].key = key;
    }

    signal(SIGSEGV, handler);
    wait_time = (unsigned long*) calloc(num_threads, sizeof(unsigned long));
    pthread_t* threads = (pthread_t*) malloc(num_threads * sizeof(pthread_t)); 
    int* ids = (int *) malloc(num_threads * sizeof(int));

    // get time
    struct timespec start, end;
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0)
    {
        fprintf(stderr, "Error getting start time: %s\n", strerror(errno));
        exit(1);
    }

    // start threads
    for (i = 0; i < num_threads; ++i) 
    {
        ids[i] = i;
        pthread_create(&threads[i], NULL, thread_worker, (void *) (&ids[i]));
    }

    // join threads
    for (i = 0; i < num_threads; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    // get end time
    clock_gettime(CLOCK_MONOTONIC, &end);

    // check length of list
    int list_len = 0;
    for (i = 0; i < num_lists; ++i)
        list_len += SortedList_length(listheads + i);
    if (list_len != 0)
    {
        fprintf(stderr, "Error, nonzero final list length\n");
        exit(1);
    }

    // print 
    char name[20] = "list-";
    if (opt_yield)
    {
        if (opt_yield & INSERT_YIELD) strcat(name, "i");
        if (opt_yield & DELETE_YIELD) strcat(name, "d");
        if (opt_yield & LOOKUP_YIELD) strcat(name, "l");
    }
    else strcat(name, "none");
    
    if (opt_m) strcat(name, "-m");
    else if (opt_s) strcat(name, "-s");
    else strcat(name, "-none");

    long long num_ops = num_threads * num_iterations * 3;
    long long total_runtime = 1000000000 * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
    long long time_per_op = total_runtime/num_ops;
    long total_wait_time = 0;
    if (opt_m || opt_s) 
    {
        for (i = 0; i < num_threads; ++i) 
            total_wait_time += wait_time[i];
    }

    long average_wait_time = total_wait_time/num_ops;
    printf("%s,%d,%d,%d,%lld,%lld,%lld,%ld\n", name, num_threads, num_iterations, num_lists, num_ops, total_runtime, time_per_op, average_wait_time);

    // free memory
    if (opt_m) 
        for (i = 0; i < num_lists; ++i) 
            pthread_mutex_destroy(&mutex_locks[i]);
    free(spin_locks);
    free(threads);
    free(pool);
    free(listheads);
    free(ids);
    free(wait_time);

    exit(0);
}
