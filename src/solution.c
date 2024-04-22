#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libcoro.h"
/**
 * You can compile and run this code using the commands:
 *
 * $> gcc solution.c libcoro.c
 * $> ./a.out
 */

struct my_context {
	char *name;
    	char* filename;
    	int second_start;
    	int second_finish;
    	int nano_second_start;
    	int nano_second_finish;
    	int second_all;
	int nano_second_all;
    	
};

static struct my_context *
my_context_new(const char *name, const char *filename)
{
	struct my_context *ctx = malloc(sizeof(*ctx));
	ctx->name = strdup(name);
	ctx->filename = strdup(filename);
	return ctx;
}

static void
my_context_delete(struct my_context *ctx)
{
	free(ctx->name);
	free(ctx->filename);
	free(ctx);
}

/**
 * A function, called from inside of coroutines recursively. Just to demonstrate
 * the example. You can split your code into multiple functions, that usually
 * helps to keep the individual code blocks simple.
 */

/**
 * Coroutine body. This code is executed by all the coroutines. Here you
 * implement your solution, sort each individual file.
 */

void 
start_timer_count(struct my_context *ctx) 
{
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	ctx->second_start = time.tv_sec;
	ctx->nano_second_start = time.tv_nsec;
}

void 
stop_timer_count(struct my_context *ctx) 
{
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	ctx->second_finish = time.tv_sec;
	ctx->nano_second_finish = time.tv_nsec;
}

void 
count_time(struct my_context *ctx) 
{
	ctx->second_all += ctx->second_finish - ctx->second_start;
	if (ctx->nano_second_finish - ctx->nano_second_start < 0) {
		ctx->nano_second_all += 1000000000 + ctx->nano_second_finish - ctx->nano_second_start;
		ctx->second_all--;
	} 
	else {
		ctx->nano_second_all += ctx->nano_second_finish - ctx->nano_second_start;
	}
}

void merge_sort(int* array, int left, int right, struct my_context *ctx) {
    if (left == right) {
        return;
    }

    else {
        int middle = (left + right) / 2;
        merge_sort(array, left, middle, ctx);
        merge_sort(array, middle + 1, right, ctx);
        
        unsigned int l_bound = left;
        unsigned int r_bound = middle + 1;
        int* temp = (int*)malloc(sizeof(int) * (right - left + 1));
        for (unsigned int step = 0; step < right - left + 1; step++) {

            if ((r_bound > right) || ((l_bound <= middle) && (array[l_bound] < array[r_bound]))) {
                temp[step] = array[l_bound];
                l_bound++;
            }

            else {
                temp[step] = array[r_bound];
                r_bound++;
            }
        }

        for (unsigned int step = 0; step < right - left + 1; step++) {
            array[left + step] = temp[step];
        }
        free(temp);
        
        stop_timer_count(ctx);
	count_time(ctx);
	coro_yield();
	start_timer_count(ctx);
    }
}

void get_array_from_file(const char* filename, struct my_context *ctx) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening file.\n");
        return;
    }
    
    int* array = NULL;
    int count = 0;
    int number = 0;
    while (fscanf(file, "%d", &number) == 1) {
        array = (int*)realloc(array, (count + 1) * sizeof(int));
        array[count++] = number;
    }
    fclose(file);
   
    merge_sort(array, 0, count - 1, ctx);
    
    file = fopen(filename, "w");
    for(unsigned int i = 0; i < count; i++) {
     fprintf(file, "%d ", array[i]);
    }
    fclose(file);
    
    free(array);
}

void merge_sort_finish(int* array, int left, int right) {
    if (left == right) {
        return;
    }

    else {
        int middle = (left + right) / 2;
        merge_sort_finish(array, left, middle);
        merge_sort_finish(array, middle + 1, right);
        
        unsigned int l_bound = left;
        unsigned int r_bound = middle + 1;
        int* temp = (int*)malloc(sizeof(int) * (right - left + 1));
        for (unsigned int step = 0; step < right - left + 1; step++) {

            if ((r_bound > right) || ((l_bound <= middle) && (array[l_bound] < array[r_bound]))) {
                temp[step] = array[l_bound];
                l_bound++;
            }

            else {
                temp[step] = array[r_bound];
                r_bound++;
            }
        }

        for (unsigned int step = 0; step < right - left + 1; step++) {
            array[left + step] = temp[step];
        }
        free(temp);
    }
}

void get_array_from_file_finish(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening file.\n");
        return;
    }
    
    int* array = NULL;
    int count = 0;
    int number = 0;
    while (fscanf(file, "%d", &number) == 1) {
        array = (int*)realloc(array, (count + 1) * sizeof(int));
        array[count++] = number;
    }
    fclose(file);
   
    merge_sort_finish(array, 0, count - 1);
    
    file = fopen(filename, "w");
    for(unsigned int i = 0; i < count; i++) {
     fprintf(file, "%d ", array[i]);
    }
    fclose(file);
    
    free(array);
}

static int
coroutine_func_f(void *context)
{
	/* IMPLEMENT SORTING OF INDIVIDUAL FILES HERE. */
	struct coro *this = coro_this();
	struct my_context *ctx = context;
	char *name = ctx->name;
    	char* filename = ctx->filename;
    	
    	start_timer_count(ctx);
    	
    	get_array_from_file(filename, ctx);
	
	stop_timer_count(ctx);
	count_time(ctx);

	printf("coroutine name is: %s \n %lld\ntime %d usec\n\n",
	 	ctx->name,
	    	coro_switch_count(this),
		ctx->second_all * 1000000 + ctx->nano_second_all / 1000
	);

	my_context_delete(ctx);
	
	/* This will be returned from coro_status(). */
	return 0;
}

int
main(int argc, char **argv)
{
	/* Initialize our coroutine global cooperative scheduler. */
	coro_sched_init();
	/* Start several coroutines. */
	
    	struct timespec total_start, total_end;
    	long long totalElapsedTime;
	clock_gettime(CLOCK_MONOTONIC, &total_start);
	
	for (int i = 1; i < argc; ++i) {
		/*
		 * The coroutines can take any 'void *' interpretation of which
		 * depends on what you want. Here as an example I give them
		 * some names.
		 */
		char name[16];
		sprintf(name, "coro_%d", i);
		/*
		 * I have to copy the name. Otherwise all the coroutines would
		 * have the same name when they finally start.
		 */
		coro_new(coroutine_func_f, my_context_new(name, argv[i]));
	}
	/* Wait for all the coroutines to end. */
	struct coro *c;
	while ((c = coro_sched_wait()) != NULL) {
		/*
		 * Each 'wait' returns a finished coroutine with which you can
		 * do anything you want. Like check its exit status, for
		 * example. Don't forget to free the coroutine afterwards.
		 */
		printf("Finished %d\n", coro_status(c));
		coro_delete(c);
	}
	/* All coroutines have finished. */
	
	/* IMPLEMENT MERGING OF THE SORTED ARRAYS HERE. */
	
    	FILE* target = fopen("result.txt", "w");
    	if (target == NULL) {
            printf("Error opening target file.\n");
            return 1;
    	}

    	for (unsigned int i = 1; i < argc; i++) {
             FILE* source = fopen(argv[i], "r");
             if (source == NULL) {
                 printf("Error opening source file %s.\n", argv[i]);
                 continue;
             }

             int c;
             while ((c = fgetc(source)) != EOF) {
                 fputc(c, target);
             }

             fclose(source);
        }

        fclose(target);
       	get_array_from_file_finish("result.txt");
       	
	clock_gettime(CLOCK_MONOTONIC, &total_end);

        totalElapsedTime = (total_end.tv_sec - total_start.tv_sec) * 1000000LL + (total_end.tv_nsec - total_start.tv_nsec) / 1000;
        printf("Total Elapsed time: %lld mcs\n", totalElapsedTime);
	return 0;
}
