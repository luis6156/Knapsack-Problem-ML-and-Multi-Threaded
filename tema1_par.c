#include <stdio.h>
#include <stdlib.h>

#include "genetic_algorithm_par.h"

int main(int argc, char *argv[]) {
    int r;
    void *status;

    // array with all the objects that can be placed in the sack
    sack_object *objects = NULL;

    // number of objects
    int object_count = 0;

    // maximum weight that can be carried in the sack
    int sack_capacity = 0;

    // number of generations
    int generations_count = 0;

    // number of threads
    int P = 0;

    // read input and assign proper variables
    if (!read_input(&objects, &object_count, &sack_capacity, &generations_count,
                    &P, argc, argv)) {
        return 0;
    }

    // create current/next generation outside of the thread function
    // (calloc/malloc is not thread safe)
    individual *current_generation =
        (individual *)calloc(object_count, sizeof(individual));
    individual *next_generation =
        (individual *)calloc(object_count, sizeof(individual));

    // thread variables
    pthread_t threads[P];
    function_args args[P];

    // pthread barrier
    pthread_barrier_t barrier;

    // initialize barrier with P threads
    pthread_barrier_init(&barrier, NULL, P);

    // allocate current/next generation's chromosomes outside of the thread
    // function (calloc/malloc is not thread safe)
    for (int i = 0; i < object_count; ++i) {
        current_generation[i].chromosomes =
            (int *)calloc(object_count, sizeof(int));
        next_generation[i].chromosomes =
            (int *)calloc(object_count, sizeof(int));
    }

    // create each thread with the designated thread function
    for (int i = 0; i < P; ++i) {
		// assign the arguments for each thread
        args[i].barrier = &barrier;
        args[i].current_generation = current_generation;
        args[i].next_generation = next_generation;
        args[i].generations_count = generations_count;
        args[i].object_count = object_count;
        args[i].objects = objects;
        args[i].P = P;
        args[i].sack_capacity = sack_capacity;
        args[i].thread_id = i;

		// create thread with the designated function
        r = pthread_create(&threads[i], NULL, run_genetic_algorithm, &args[i]);
        if (r) {
            printf("Eroare la crearea thread-ului %d\n", i);
            exit(-1);
        }
    }

    // join threads
    for (int i = 0; i < P; ++i) {
        r = pthread_join(threads[i], &status);
        if (r) {
            printf("Eroare la asteptarea thread-ului %d\n", i);
            exit(-1);
        }
    }

    // destroy barrier
    if (pthread_barrier_destroy(&barrier) != 0) {
        printf("Eroare la dezalocarea barierei\n");
        exit(-1);
    }

    // free(barrier);
    free(objects);

    return 0;
}
