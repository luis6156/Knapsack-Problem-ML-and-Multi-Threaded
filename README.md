# Copyright Micu Florian-Luis 331CA 2021 - First Assignment

## Initial Problem
The algorithm at hand for solving the knapsack problem uses a genetic algorithm
that resolves around mutations and crossover operations between generations. 
The task is to use pthreads to parallelize the solution. Before adding the 
pthread code, we first have to find the main function that will serve as the
thread function. The main function that computes the solution for the problem 
is "run_genetic_algorithm", which will be used as the thread function.

## Parallelizing the algorithm
### Changing the algorithm to accept threads
We must first create P number of pthreads and assign the designed function as
well as make sure we join them after completion. As such, the "read_input"
function was changed to accept the "number of threads" parameter (P). Moreover,
the function signature was changed to return a void pointer and its parameters
was changed to void pointer. As a consequence, I created a structure that holds
all of my required parameters which will be passed in this function 
(function_args.h).

### Heap memory allocation problem
Inside the thread function, the current and next generation are allocated on 
the heap, therefore I added them in the main function as a preamble to the 
thread creation, since all the threads will use this as a "shared resource".
Moreover, heap allocation using "malloc" or "calloc" is not thread safe and 
could lead to data desynchronization between threads.  

### "for" loop parallelization
The algorithm functions using many "for" loops which can be parallelized, as
a thread could complete one part of the loop and another the other part. The
loops which were altered are the ones ranging from [0 - object_count] and
from [0 - count]. The loop ranging from [0 - generations_count] was not altered
as it needs the previous iteration to compute the current generation, thus if
a thread were to do one half, the second thread would not have the first half
to compute its current half. 

### Adding barriers
Since threads can work at different paces (one thread could be in front of
the other), we need to make sure the data that one thread needs is updated
before computing the next genetic material or other calculations. Therefore,
after parallelizing the "for" loops, we will need to add a barrier to make
sure the full array for the current/next generation was updated. The barrier 
is created, initialized and destroyed inside the main function with "P" threads
as it needs to wait for all the threads before proceeding to the next 
operation. After each problematic operation (sorting, computing fitness, 
copying, mutating, crossovers, updating count) the wait function was called on
the barrier.

### Further optimization
After running Gprof (performance analysis tool), I remarked that 87% of the
running time was spent in the comparison function used by "qsort". In this
function, the first sorting criteria is the fitness, the second is the object
count and the third is the index. The object count value is always recalculated
inside the compare function which is wasteful as the same object could be 
compared more than once, thus I added a field inside the "individual" structure
called "object_count" is calculated once before the sorting algorithm. 
Therefore, the value is cached and this improves speed vastly. This value is 
calculated inside the "compute_fitness" function as it already uses the 
necessary "for" loop to compute the object count. 

### Sorting algorithm possible optimization
The standard "qsort" algorithm could be changed with the parallelized 
"merge sort" or "shear sort" algorithm which have a similar general complexity
and an even better one as the standard complexity is divided by P threads.
However, the shear sort algorithm uses a square matrix therefore it requires a
square number of elements in the array and the merge sort algorithm requires a
power of 2 to be run. Our input does not match this criteria, however we could
add garbage values to complete the requirements and the performance penalty 
would be minimal (e.g. merge sort complexity O(logN), for input = 10000 ->
complexity ~ 13.28 and for input = 16384 or 2^14 -> complexity ~ 14). However,
the compare function optimization offers a major speedup (from 22s to 2s in one
test) and I considered this change to be enough to create a major difference
couples with the other parallelizations.

### Gprof data
As I have said in the previous paragraphs, the compare function occupied 87%
of the time (or 19s out of 22s) and the "compute_fitness" function occupied
2s out of 22s. Both of these two functions were addressed and optimized. The
remaining 1s is attributed to the rest of the functions. I attached an image
link to the Gprof data.

link: https://drive.google.com/file/d/1zBNq43eat8nMinged9I8smIrOWkkFxdv/view?usp=sharing
