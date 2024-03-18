#include "parser.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_ARGS 100
#define MAX_COMMANDS 100
#define MAX_TOKEN_LEN 100


static void
execute_command_line(const struct command_line *line)
{
	/* REPLACE THIS CODE WITH ACTUAL COMMAND EXECUTION */

	assert(line != NULL);
	printf("================================\n");
	printf("Command line:\n");
	printf("Is background: %d\n", (int)line->is_background);
	printf("Output: ");
	if (line->out_type == OUTPUT_TYPE_STDOUT) {
		printf("stdout\n");
	} else if (line->out_type == OUTPUT_TYPE_FILE_NEW) {
		printf("new file - \"%s\"\n", line->out_file);
	} else if (line->out_type == OUTPUT_TYPE_FILE_APPEND) {
		printf("append file - \"%s\"\n", line->out_file);
	} else {
		assert(false);
	}
	printf("Expressions:\n");
	const struct expr *e = line->head;
	while (e != NULL) {
		if (e->type == EXPR_TYPE_COMMAND) {
			printf("\tCommand: %s", e->cmd.exe);
			for (uint32_t i = 0; i < e->cmd.arg_count; ++i) {
				printf(" %s", e->cmd.args[i]);
			}
			execvp(e->cmd.args);
			execvp(e->cmd.exe);
			printf("\n");
		} else if (e->type == EXPR_TYPE_PIPE) {
			printf("\tPIPE\n");
			
			int *pipes = (int*)malloc(2 * sizeof(int));
			int prev_pipe = 0;
			
			for (uint32_t i = 0; i < e->cmd.arg_count; ++i) {
				pipe(pipes);
				if (!fork()) {
				    dup2(prev_pipe, 0);
				    
				    if (i != e->cmd.arg_count - 1) {
				        dup2(pipes[1], 1);
				    }
				    close(pipes[0]);
				    
				    if (strcmp(e->cmd.args[i], "cd") == 0) {
        				chdir(e->cmd.args[i + 1]);
        			    } 
        			    else if (strcmp(e->cmd.args[i], "exit") == 0) {
        				      if (i == 1) {
        				      	  free(pipes);
            					  exit(0);
            				      }
    				    } 
    				    else {
        				execvp(e->cmd.args[0], e->cmd.args);
    				    }	
				}
				else {
                			wait(NULL);
                			close(pipes[1]);
                			prev_pipe = pipes[0];
            			}
			}
			free(pipes);
		
		} else if (e->type == EXPR_TYPE_AND) {
			printf("\tAND\n");
		} else if (e->type == EXPR_TYPE_OR) {
			printf("\tOR\n");
		} else {
			assert(false);
		}
		e = e->next;
	}
}

int
main(void)
{
	const size_t buf_size = 1024;
	char buf[buf_size];
	int rc;
	struct parser *p = parser_new();
	while ((rc = read(STDIN_FILENO, buf, buf_size)) > 0) {
		parser_feed(p, buf, rc);
		struct command_line *line = NULL;
		while (true) {
			enum parser_error err = parser_pop_next(p, &line);
			if (err == PARSER_ERR_NONE && line == NULL)
				break;
			if (err != PARSER_ERR_NONE) {
				printf("Error: %d\n", (int)err);
				continue;
			}
			execute_command_line(line);
			printf("TESTTESTTESY\n");
			command_line_delete(line);
		}
	}
	parser_delete(p);
	return 0;
}
