#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

struct task {
    int delay;
    char command[256];
};

typedef struct task task;

#define MIN(a, b) ((a < b) ? (a) : (b))

task read_task(FILE* file) {
    task res;
    fscanf(file, "%i", &res.delay);

    char* command = NULL;
    unsigned long n = 0;
    getline(&command, &n, file);
    memcpy(res.command, command, MIN(n, 256));
    free(command);

    return res;
}

#define INSTANTIATE_ARRAY_T(elem_t) \
struct dynamic_array_ ## elem_t { \
    elem_t* storage; \
    int capacity; \
    int size; \
};

#define ARRAY_T(elem_t) \
struct dynamic_array_ ## elem_t

#define ALLOC_ARRAY(elem_t) \
(ARRAY_T(elem_t)*)calloc(1, sizeof(ARRAY_T(elem_t)));

#define INIT_ARRAY(array) { \
    array->capacity = 1; \
    array->size = 0; \
    array->storage = calloc(array->capacity, sizeof(array->storage[0])); \
}

#define PUSH(array, a) { \
    if (array->size == array->capacity) { \
        array->capacity *= 2; \
        array->storage = realloc(array->storage, sizeof(array->storage[0]) * array->capacity); \
    } \
    array->storage[array->size] = a; \
    array->size += 1; \
}

INSTANTIATE_ARRAY_T(task);

typedef FILE* PFILE;
INSTANTIATE_ARRAY_T(PFILE);

int comparator(const void* a, const void* b) {
    return ((task*)a)->delay - ((task*)b)->delay;
}

int main(int argc, char** argv) {
    int start = time(NULL);

    if (argc < 2) {
        printf("usage: %s command_list\n", argv[0]);
        exit(1);
    }
    const char* filename = argv[1];
    FILE* file = fopen(filename, "r");

    ARRAY_T(task)* array = ALLOC_ARRAY(task);
    INIT_ARRAY(array);
    while (!feof(file)) {
        task t = read_task(file);
        PUSH(array, t);
    }

    qsort(array->storage, array->size, sizeof(task), comparator);

    ARRAY_T(PFILE)* pipes = ALLOC_ARRAY(PFILE);
    INIT_ARRAY(pipes);
    for (int i = 0; i < array->size; ++i) {
        int delay = array->storage[i].delay;
        while (time(NULL) - start < delay) { /* busy wait */ }
        FILE* pipe = popen(array->storage[i].command, "r");
        PUSH(pipes, pipe);
    }

    for (int i = 0; i < pipes->size; ++i) {
        pclose(pipes->storage[i]);
    }
}