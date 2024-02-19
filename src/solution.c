#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libcoro.h"
#include "sort.h"
/**
 * You can compile and run this code using the commands:
 *
 * $> gcc solution.c libcoro.c
 * $> ./a.out
 */

struct my_context {
	char *name;
    	char* filename;
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
static void
other_function(const char *name, int depth)
{
	printf("%s: entered function, depth = %d\n", name, depth);
	coro_yield();
	if (depth < 3)
		other_function(name, depth + 1);
}

/**
 * Coroutine body. This code is executed by all the coroutines. Here you
 * implement your solution, sort each individual file.
 */

static int
coroutine_func_f(void *context)
{
	/* IMPLEMENT SORTING OF INDIVIDUAL FILES HERE. */
	struct timespec start, end;
	long long elapsedTime;
	clock_gettime(CLOCK_MONOTONIC, &start);
	
	struct coro *this = coro_this();
	struct my_context *ctx = context;
	char *name = ctx->name;
    	char* filename = ctx->filename;
    	
    	get_array_from_file(filename);
	
	printf("Started coroutine %s\n", name);
	printf("%s: switch count %lld\n", name, coro_switch_count(this));
	printf("%s: yield\n", name);
	
	clock_gettime(CLOCK_MONOTONIC, &end);
    	elapsedTime = (end.tv_sec - start.tv_sec) * 1000000LL + (end.tv_nsec - start.tv_nsec) / 1000;
    	printf("Elapsed time: %lld mcs\n", elapsedTime);
    	
	coro_yield();
	
	other_function(name, 1);
	printf("%s: switch count after other function %lld\n", name,
	       coro_switch_count(this));

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
    	struct timespec total_start, total_end;
    	long long totalElapsedTime;
    
    	clock_gettime(CLOCK_MONOTONIC, &total_start);	
	
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
       	get_array_from_file("result.txt");
       	
       	clock_gettime(CLOCK_MONOTONIC, &total_end);
       	totalElapsedTime = (total_end.tv_sec - total_start.tv_sec) * 1000000LL +(total_end.tv_nsec - total_start.tv_nsec) / 1000;
    
    	printf("Total elapsed time for the program: %lld mcs\n", totalElapsedTime);
	return 0;
}
