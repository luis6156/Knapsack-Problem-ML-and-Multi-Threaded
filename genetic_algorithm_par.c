#include "genetic_algorithm_par.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define min(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a < _b ? _a : _b;      \
    })

int read_input(sack_object **objects, int *object_count, int *sack_capacity,
               int *generations_count, int *P, int argc, char *argv[]) {
    FILE *fp;

    if (argc < 4) {
        fprintf(stderr,
                "Usage:\n\t./tema1 in_file generations_count nr_threads\n");
        return 0;
    }

    fp = fopen(argv[1], "r");
    if (fp == NULL) {
        return 0;
    }

    if (fscanf(fp, "%d %d", object_count, sack_capacity) < 2) {
        fclose(fp);
        return 0;
    }

    if (*object_count % 10) {
        fclose(fp);
        return 0;
    }

    sack_object *tmp_objects =
        (sack_object *)calloc(*object_count, sizeof(sack_object));

    for (int i = 0; i < *object_count; ++i) {
        if (fscanf(fp, "%d %d", &tmp_objects[i].profit,
                   &tmp_objects[i].weight) < 2) {
            free(objects);
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);

    *generations_count = (int)strtol(argv[2], NULL, 10);

    if (*generations_count == 0) {
        free(tmp_objects);

        return 0;
    }

    *objects = tmp_objects;

    *P = (int)strtol(argv[3], NULL, 10);

    if (*P <= 0) {
        return 0;
    }

    return 1;
}

void print_objects(const sack_object *objects, int object_count) {
    for (int i = 0; i < object_count; ++i) {
        printf("%d %d\n", objects[i].weight, objects[i].profit);
    }
}

void print_generation(const individual *generation, int limit) {
    for (int i = 0; i < limit; ++i) {
        for (int j = 0; j < generation[i].chromosome_length; ++j) {
            printf("%d ", generation[i].chromosomes[j]);
        }

        printf("\n%d - %d\n", i, generation[i].fitness);
    }
}

void print_best_fitness(const individual *generation) {
    printf("%d\n", generation[0].fitness);
}

void compute_fitness_function(const sack_object *objects,
                              individual *generation, int object_count,
                              int sack_capacity, int start, int end) {
    int weight;
    int profit;

    for (int i = start; i < end; ++i) {
        weight = 0;
        profit = 0;
        generation[i].nr_objects = 0;

        for (int j = 0; j < generation[i].chromosome_length; ++j) {
            if (generation[i].chromosomes[j]) {
                weight += objects[j].weight;
                profit += objects[j].profit;
                // compute the value for "nr_object" inside the fitness function
                // as it will be used for comparison in the sorting algorithm
                ++generation[i].nr_objects;
            }
        }

        generation[i].fitness = (weight <= sack_capacity) ? profit : 0;
    }
}

int cmpfunc(const void *a, const void *b) {
    individual *first = (individual *)a;
    individual *second = (individual *)b;

    int res = second->fitness - first->fitness;  // decreasing by fitness
    if (res == 0) {
        // use the already stored "nr_objects" parameter insted of recomputing
        // the value everytime
        res = first->nr_objects - second->nr_objects;

        if (res == 0) {
            return second->index - first->index;
        }
    }

    return res;
}

void mutate_bit_string_1(const individual *ind, int generation_index) {
    int i, mutation_size;
    int step = 1 + generation_index % (ind->chromosome_length - 2);

    if (ind->index % 2 == 0) {
        // for even-indexed individuals, mutate the first 40% chromosomes by a
        // given step
        mutation_size = ind->chromosome_length * 4 / 10;
        for (i = 0; i < mutation_size; i += step) {
            ind->chromosomes[i] = 1 - ind->chromosomes[i];
        }
    } else {
        // for even-indexed individuals, mutate the last 80% chromosomes by a
        // given step
        mutation_size = ind->chromosome_length * 8 / 10;
        for (i = ind->chromosome_length - mutation_size;
             i < ind->chromosome_length; i += step) {
            ind->chromosomes[i] = 1 - ind->chromosomes[i];
        }
    }
}

void mutate_bit_string_2(const individual *ind, int generation_index) {
    int step = 1 + generation_index % (ind->chromosome_length - 2);

    // mutate all chromosomes by a given step
    for (int i = 0; i < ind->chromosome_length; i += step) {
        ind->chromosomes[i] = 1 - ind->chromosomes[i];
    }
}

void crossover(individual *parent1, individual *child1, int generation_index) {
    individual *parent2 = parent1 + 1;
    individual *child2 = child1 + 1;
    int count = 1 + generation_index % parent1->chromosome_length;

    memcpy(child1->chromosomes, parent1->chromosomes, count * sizeof(int));
    memcpy(child1->chromosomes + count, parent2->chromosomes + count,
           (parent1->chromosome_length - count) * sizeof(int));

    memcpy(child2->chromosomes, parent2->chromosomes, count * sizeof(int));
    memcpy(child2->chromosomes + count, parent1->chromosomes + count,
           (parent1->chromosome_length - count) * sizeof(int));
}

void copy_individual(const individual *from, const individual *to) {
    memcpy(to->chromosomes, from->chromosomes,
           from->chromosome_length * sizeof(int));
}

void free_generation(individual *generation) {
    int i;

    for (i = 0; i < generation->chromosome_length; ++i) {
        free(generation[i].chromosomes);
        generation[i].chromosomes = NULL;
        generation[i].fitness = 0;
    }
}

void *run_genetic_algorithm(void *args) {
    int count, cursor, r;
    int start_count;
    int end_count;

    // convert function struct to local variables for readability
    int object_count = (*(function_args *)args).object_count;
    int generations_count = (*(function_args *)args).generations_count;
    int sack_capacity = (*(function_args *)args).sack_capacity;
    int P = (*(function_args *)args).P;
    int thread_id = (*(function_args *)args).thread_id;
    sack_object *objects = (*(function_args *)args).objects;
    individual *current_generation =
        (*(function_args *)args).current_generation;
    individual *next_generation = (*(function_args *)args).next_generation;
    pthread_barrier_t *barrier = (*(function_args *)args).barrier;

    individual *tmp = NULL;

    // compute start and end indices for threads from "0" to "object_count"
    int start_object_count = thread_id * (double)object_count / P;
    int end_object_count =
        min((thread_id + 1) * (double)object_count / P, object_count);

    // set initial generation (composed of object_count individuals
    // with a single item in the sack);
    // parallelized "for" loop
    for (int i = start_object_count; i < end_object_count; ++i) {
        current_generation[i].fitness = 0;
        current_generation[i].chromosomes[i] = 1;
        current_generation[i].index = i;
        current_generation[i].chromosome_length = object_count;
        current_generation[i].nr_objects = 0;

        next_generation[i].fitness = 0;
        next_generation[i].index = i;
        next_generation[i].chromosome_length = object_count;
    }

    // wait for the current and next generation arrays to be initialised by all
    // threads
    r = pthread_barrier_wait(barrier);
    if (r != PTHREAD_BARRIER_SERIAL_THREAD && r != 0) {
        printf("Eroare la asteptarea la bariera\n");
        exit(-1);
    }

    // iterate for each generation
    for (int k = 0; k < generations_count; ++k) {
        cursor = 0;

        // compute fitness and sort by it;
        // pass the start and end indices for the threads from "0" to
        // "object_count"
        compute_fitness_function(objects, current_generation, object_count,
                                 sack_capacity, start_object_count,
                                 end_object_count);

        // wait for the fitness to be computed by all threads
        r = pthread_barrier_wait(barrier);
        if (r != PTHREAD_BARRIER_SERIAL_THREAD && r != 0) {
            printf("Eroare la asteptarea la bariera\n");
            exit(-1);
        }

        // let only one tread perform the sorting algorithm
        if (thread_id == 0) {
            qsort(current_generation, object_count, sizeof(individual),
                  cmpfunc);
        }

        // wait for all the threads to reach this point to make sure the sorting
        // has been done before any other operation
        r = pthread_barrier_wait(barrier);
        if (r != PTHREAD_BARRIER_SERIAL_THREAD && r != 0) {
            printf("Eroare la asteptarea la bariera\n");
            exit(-1);
        }

        // keep first 30% children (elite children selection)
        count = object_count * 3 / 10;

        // compute start and end indices for the threads from "0" to "count"
        start_count = thread_id * (double)count / P;
        end_count = min((thread_id + 1) * (double)count / P, count);

        // parallelized "for" loop
        for (int i = start_count; i < end_count; ++i) {
            copy_individual(current_generation + i, next_generation + i);
        }
        cursor = count;

        // wait for all the threads to copy the first 30% children
        r = pthread_barrier_wait(barrier);
        if (r != PTHREAD_BARRIER_SERIAL_THREAD && r != 0) {
            printf("Eroare la asteptarea la bariera\n");
            exit(-1);
        }

        // mutate first 20% children with the first version of bit string
        // mutation
        count = object_count * 2 / 10;

        // compute start and end indices for the threads from "0" to "count"
        start_count = thread_id * (double)count / P;
        end_count = min((thread_id + 1) * (double)count / P, count);

        // parallelized "for" loop
        for (int i = start_count; i < end_count; ++i) {
            copy_individual(current_generation + i,
                            next_generation + cursor + i);
            mutate_bit_string_1(next_generation + cursor + i, k);
        }
        cursor += count;

        // wait for all the threads to mutate the children
        r = pthread_barrier_wait(barrier);
        if (r != PTHREAD_BARRIER_SERIAL_THREAD && r != 0) {
            printf("Eroare la asteptarea la bariera\n");
            exit(-1);
        }

        // mutate next 20% children with the second version of bit string
        // mutation
        count = object_count * 2 / 10;

        // compute start and end indices for the threads from "0" to "count"
        start_count = thread_id * (double)count / P;
        end_count = min((thread_id + 1) * (double)count / P, count);

        // parallelized "for" loop
        for (int i = start_count; i < end_count; ++i) {
            copy_individual(current_generation + i + count,
                            next_generation + cursor + i);
            mutate_bit_string_2(next_generation + cursor + i, k);
        }
        cursor += count;

        // wait for all the threads to mutate the children
        r = pthread_barrier_wait(barrier);
        if (r != PTHREAD_BARRIER_SERIAL_THREAD && r != 0) {
            printf("Eroare la asteptarea la bariera\n");
            exit(-1);
        }

        // crossover first 30% parents with one-point crossover
        // (if there is an odd number of parents, the last one is kept as
        // such)
        count = object_count * 3 / 10;
        if (count % 2 == 1) {
            // let only one thread do the copy
            if (thread_id == 0) {
                copy_individual(current_generation + object_count - 1,
                                next_generation + cursor + count - 1);
            }
            // let all threads decrement the count
            count--;
        }

        // wait for all the threads to reach this point to make sure the copy
        // and the "count" decrement has been done before any other operation
        r = pthread_barrier_wait(barrier);
        if (r != PTHREAD_BARRIER_SERIAL_THREAD && r != 0) {
            printf("Eroare la asteptarea la bariera\n");
            exit(-1);
        }

        // compute start and end indices for the threads from "0" to "count"
        start_count = thread_id * (double)count / P;
        end_count = min((thread_id + 1) * (double)count / P, count);
        // if the start index in uneven make it even, since the step is 2 in the
        // "for" loop
        if (start_count % 2 == 1) {
            ++start_count;
        }

        // parallelized "for" loop
        for (int i = start_count; i < end_count; i += 2) {
            crossover(current_generation + i, next_generation + cursor + i, k);
        }

        // wait for all the threads to do the crossover
        r = pthread_barrier_wait(barrier);
        if (r != PTHREAD_BARRIER_SERIAL_THREAD && r != 0) {
            printf("Eroare la asteptarea la bariera\n");
            exit(-1);
        }

        // switch to new generation (let all threads to this step since "tmp" is
        // a local variable letting only one threads do this operation will not
        // synchronize the generations between threads)
        tmp = current_generation;
        current_generation = next_generation;
        next_generation = tmp;

        // wait for all the threads to do the switch
        r = pthread_barrier_wait(barrier);
        if (r != PTHREAD_BARRIER_SERIAL_THREAD && r != 0) {
            printf("Eroare la asteptarea la bariera\n");
            exit(-1);
        }

        // parallelized "for" loop from "0" to "object_count"
        for (int i = start_object_count; i < end_object_count; ++i) {
            current_generation[i].index = i;
        }

        // let only one thread to the print
        if (thread_id == 0) {
            if (k % 5 == 0) {
                print_best_fitness(current_generation);
            }
        }
    }

    // wait for all the threads to finish the "for" loop
    r = pthread_barrier_wait(barrier);
    if (r != PTHREAD_BARRIER_SERIAL_THREAD && r != 0) {
        printf("Eroare la asteptarea la bariera\n");
        exit(-1);
    }

    // compute fitness and sort by it;
    // pass the start and end indices for the threads from "0" to
    // "object_count"
    compute_fitness_function(objects, current_generation, object_count,
                             sack_capacity, start_object_count,
                             end_object_count);

    // wait for all the threads to compute the fitness
    r = pthread_barrier_wait(barrier);
    if (r != PTHREAD_BARRIER_SERIAL_THREAD && r != 0) {
        printf("Eroare la asteptarea la bariera\n");
        exit(-1);
    }

    // let only one thread do the sorting algorithm, the print operation and
    // deallocate heap memory
    if (thread_id == 0) {
        qsort(current_generation, object_count, sizeof(individual), cmpfunc);

        print_best_fitness(current_generation);

        // free resources for old generation
        free_generation(current_generation);
        free_generation(next_generation);

        // free resources
        free(current_generation);
        free(next_generation);
    }

    // return null
    pthread_exit(NULL);
}