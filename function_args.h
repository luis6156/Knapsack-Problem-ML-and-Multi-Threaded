#ifndef FUNCTION_ARGS_H
#define FUNCTION_ARGS_H

#include <pthread.h>

#include "individual.h"
#include "sack_object.h"

// structure that holds the arguments for the thread function
typedef struct function_args {
    individual *current_generation;
    individual *next_generation;
    sack_object *objects;
    pthread_barrier_t *barrier;
    int P;
    int thread_id;
    int object_count;
    int generations_count;
    int sack_capacity;
} function_args;

#endif
