#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <string.h>







void execute_command(char *command) {
    char *args[1000];
    int i = 0;
    char *token = strtok(command, " ");
    
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    
    args[i] = NULL;
    
    if (strcmp(args[0], "cd") == 0) {
        chdir(args[1]);
    } else if (strcmp(args[0], "exit") == 0) {
        if (i == 1) {
            exit(0);
        }
    } else {
        execvp(args[0], args);
    }
}

int main() {
    char input[1024];
    char *commands[100000];
    int command_count = 0;
    
    while (1) {
        printf("> ");
        fgets(input, sizeof(input), stdin);
        
        if (input[strlen(input) - 1] == '\n') {
            input[strlen(input) - 1] = '\0';
        }
        
        char *token = strtok(input, "|");
        
        while (token != NULL) {
            commands[command_count++] = token;
            token = strtok(NULL, "|");
        }
        
        int pipes[2];
        int prev_pipe = 0;
        
        for (int i = 0; i < command_count; i++) {
            pipe(pipes);
            
            if (!fork()) {
                dup2(prev_pipe, 0);
                
                if (i != command_count - 1) {
                    dup2(pipes[1], 1);
                }
                
                close(pipes[0]);
                
                execute_command(commands[i]);
            } else {
                wait(NULL);
                close(pipes[1]);
                prev_pipe = pipes[0];
            }
        }
        
        command_count = 0;
    }
    
    return 0;
}
