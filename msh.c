// The MIT License (MIT)
// 
// Copyright (c) 2024 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.


#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>   //for control options in redirection

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 32    //Max arguments a command can have



//funtion to handle error
void err_handler(){
      char error_message[30] = "An error has occurred\n";
      write(STDERR_FILENO, error_message, strlen(error_message)); //sends error message by 
                                                                  // writing to file descriptor
}



//Function to split the input string into tokens
char **parsing_command (char *command_string){
    char **token = (char**)malloc(MAX_NUM_ARGUMENTS *sizeof(char*)); //allocating memory for token
    int token_count = 0;  //keeps track of total arguments

    char *argument_pointer; //pointer to point to token parsed by strsep

    char *working_string = strdup(command_string); //keeps track of original value
                                                   //so we can deallocate correct amount at end

    //tokenizing the input with WHITESPACE used as the delimiter
    while(((argument_pointer = strsep(&working_string, WHITESPACE)) != NULL &&
            (token_count < 11))){  //since max parameters excluding command is 10 

                //duplicates each token to store in an array within limit of size
                token[token_count] = strndup(argument_pointer, MAX_COMMAND_SIZE);

                if (strlen(token[token_count]) == 0){ 
                    token[token_count] = NULL; //empty tokens are ingonered by setting them to NULL
                }
                token_count++;
            }    
    return token;     //returns a pointer to array of tokens                               
}



//function to handle redirection
void redirect_command(char **token){
    for(int i = 0; token[i] != NULL; i++){ //iteration to check if > operator present
        if(strcmp(token[i], ">") == 0){

            //opens the file for both read and write
            //create a file if doen't exits
            //if already file exists, clears its contents before writing
            //sets permission for the owner to read and write the file
            int fd = open(token[i+1], O_RDWR| O_CREAT| O_TRUNC, S_IRUSR|S_IWUSR);

            if(fd < 0){ 
                err_handler();
            }

            dup2(fd,1); //redirects output to file descriptor 1 (STDOUT_FILENO)
                        //so that output will be written to file instead of terminal
            close(fd);

            token[i] = NULL; //trims off the > output part of command
                             //so that only actual command is passed without > and filename
            break;
        }
    }
}



//Function to handle the three built-in commands
int handle_built_in (char **token){

    if (strcmp(token[0], "exit") == 0 || strcmp(token[0], "quit") == 0){
        exit(0); //exits program calling this
    }

    if (strcmp(token[0], "cd") == 0){

        //ensures if only one argument is given for the command
        if (token[1] == NULL || token[2] != NULL){
            err_handler();
        }
        else{
            if (chdir(token[1]) != 0){
            err_handler();
            }
        }
        return 1; //return 1 if built-in command handled
    }
    return 0; //return 0 if it wasn't built-in command to allow other commands to be executed
}




//Function to execute commands other than built-in commands
void execute_command (char** token){
    pid_t pid = fork(); //creates a new process(child), a duplicate of current parent process

    if (pid < 0){ //fork failure
        err_handler();
        return;
    }

    if (pid == 0){ //child process: responsible for executing command
        redirect_command(token); //checks to see if redirection needed

        //array of paths where commands are
        char *paths[] = {"/bin/", "/usr/bin/", "/usr/local/bin/", "./"};
        char path_executable[MAX_COMMAND_SIZE];
        int flag = 0; //indicates executable found or not

        for (int i = 0; i < 4; i++){ //to construct full path to executable using each path
            snprintf(path_executable, MAX_COMMAND_SIZE, "%s%s", paths[i], token[0]);

            if (access(path_executable, X_OK) == 0){ //checks if file exists and can be executed 
                flag = 1; //signals found
                break;
            }
        }

        if (flag){
            execv(path_executable, token); //replaces the current child process
                                           //with the new process to execute the command
        }
        else{
            err_handler();
        }
    }
    else{
        wait(NULL); //parent process waits for child to terminate
                    //and doesn't care of its exit status
    }
}





int main(int argc, char *argv[]){
    char *input_command = (char*)malloc(MAX_COMMAND_SIZE);
    FILE *infile = stdin; //sets to interactive mode where input read from terminal

    //if two argumetns->batch mode: filename provided to read from
    if (argc == 2){  
        infile = fopen(argv[1], "r");
        if(infile == NULL){
            err_handler();
            exit(1);
        }
    }
    if (argc > 2){
        err_handler();
        exit(1);
    }


    while(1){ //loops in interactive mode until exit command
        if (infile == stdin){
            printf("msh> ");
        }

        if(fgets(input_command, MAX_COMMAND_SIZE, infile) == NULL){
            if (feof(infile)){ //in batch mode: all read so exit shell
                exit(0);
            }
            err_handler();
            continue;   //if any other error in interactive mode, continue loop
        }

        char **token = parsing_command(input_command);

        if (token[0] != NULL){
            if(!handle_built_in(token)){ //if built-in already handled, ignores. else process 
                                         //the given command
                    execute_command(token);
            }
        }

        free(token);
    }
    free(input_command);
    return 0;
}
