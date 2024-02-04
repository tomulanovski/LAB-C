#include <stdio.h>
#include "LineParser.c"
#include <linux/limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int debug = 0;

void pipecmd(cmdLine *lineFromInput) {
    
    int pd[2];
    if(pipe(pd)==-1) {
        perror("couldnt open a pipe. Exiting\n");
        exit(1);
    }
    pid_t child1PID = fork();
    if(child1PID == -1) {
        perror("error in fork1. Exiting\n");
        exit(1);
    }
    //child1 process
    if(child1PID==0) {
        // close(STDOUT_FILENO);
        dup2(pd[1],STDOUT_FILENO);
        close(pd[1]);
        execvp(lineFromInput->arguments[0],lineFromInput->arguments);
    }
    //parent process
    else {
        fprintf(stderr, "(parent_process>created child1 process with id: %d)\n", child1PID);
        close(pd[1]);
    }
    pid_t child2PID = fork();
    if(child2PID==-1) {
        perror("error in fork2. Exiting\n");
        exit(1);
    }
    //child2 process
    if(child2PID==0) {
        // close(STDIN_FILENO);
        dup2(pd[0],STDIN_FILENO);
        close(pd[0]);
        execvp(lineFromInput->next->arguments[0],lineFromInput->next->arguments);
    }
    else {
        fprintf(stderr, "(parent_process>created child2 process with id: %d)\n", child2PID);
        close(pd[0]);
    }
    waitpid(child1PID, NULL, 0);
    waitpid(child2PID, NULL, 0);
}


int commands(cmdLine *lineFromInput) {
      
    if (strcmp(lineFromInput->arguments[0], "cd") == 0) {
        // Handle 'cd' command
        if (chdir(lineFromInput->arguments[1]) == -1) {
            perror("Error in chdir");
            freeCmdLines(lineFromInput);
            exit(1);
        }
        return 1;
    }
    if (strcmp(lineFromInput->arguments[0], "wakeup") == 0) {
        // Handle 'wakeup' command
        pid_t pid = atoi(lineFromInput->arguments[1]);
        if (kill(pid, SIGCONT) == -1) {
            perror("Error in wakeup");
            freeCmdLines(lineFromInput);
            exit(1);
        }
        else {
            printf("wakeup succeded\n");
        }
        return 1;
    }

    if (strcmp(lineFromInput->arguments[0], "nuke") == 0) {
        // Handle 'nuke' command
        pid_t pid = atoi(lineFromInput->arguments[1]);
        if (kill(pid, SIGINT) == -1) {
            perror("Error in nuke\n");
            freeCmdLines(lineFromInput);
            exit(1);
        }
        else {
            printf("nuke succeded\n");
            
        }
        return 1;
    }
    return 0;
}


void execute(cmdLine *lineFromInput) {

    if(commands(lineFromInput)) return;
    pid_t PID = fork();
    if (PID==0) { // child
      
        if (lineFromInput->inputRedirect) {
        int inputFile = open(lineFromInput->inputRedirect, O_RDONLY);
        if (inputFile == -1) {
            perror("Error in open for input");
            freeCmdLines(lineFromInput);
            exit(1);
        }
        dup2(inputFile,STDIN_FILENO);
        close(inputFile);
    }
     if (lineFromInput->outputRedirect) {
        int outputFile = open(lineFromInput->outputRedirect, O_WRONLY|O_CREAT);
        if (outputFile == -1) {
            perror("Error in open for output");
            freeCmdLines(lineFromInput);
            exit(1);
        }
        dup2(outputFile,STDOUT_FILENO);
        close(outputFile);
        
    }
      if(lineFromInput->next) {
            if(lineFromInput->outputRedirect) {
                perror("cant do output redirect on left side of pipe. Exiting\n");
                freeCmdLines(lineFromInput);
                exit(1);
            }
            else if(lineFromInput->next->inputRedirect){
                perror("cant do input redirect on right side of pipe. Exiting\n");
                freeCmdLines(lineFromInput);
                exit(1);
            }
            if(lineFromInput->next->outputRedirect) {
                int outputFile = open(lineFromInput->next->outputRedirect, O_WRONLY|O_CREAT);
                if (outputFile == -1) {
                    perror("Error in open for output");
                    freeCmdLines(lineFromInput);
                    exit(1);
            }
            dup2(outputFile,STDOUT_FILENO);
            close(outputFile);
            }
            
            pipecmd(lineFromInput);
        }

     else if (execvp(lineFromInput->arguments[0], lineFromInput->arguments) == -1){
            perror("Error in execv");
            freeCmdLines(lineFromInput);
            exit(1);
        }
    
    }

    else if (PID > 0) { //parent
        
        if(debug) {
        fprintf(stderr, "PID: %d\n", PID);
        fprintf(stderr, "Executing command: %s\n", lineFromInput->arguments[0]);
        }

        if(lineFromInput->blocking) {
            int status;
            waitpid(PID, &status, 0);
        }
        else {
            
            fprintf(stderr, "Background process started with PID: %d\n", PID);
            return;  
        }

    }
    else { // error
            perror("Error in fork");
            freeCmdLines(lineFromInput);
            exit(1);
        }
    }



int main(int argc, char **argv) {
    while(1) {

        for (int i=0; i<argc;i++) {
            if (strcmp(argv[i],"-d") ==0 ) {
                debug = 1;
            }
        }

        char cwd[PATH_MAX];
        char input[2048];
        if (getcwd(cwd, PATH_MAX) != NULL) {
            printf("current working directory: %s\n", cwd);
        } else {
            perror("error in getcwd function");
        }
        printf("enter input\n");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            // Exit the loop on EOF
            printf("Exiting \n");
            exit(1);
        }

        cmdLine *lineFromInput = parseCmdLines(input);

         if (strcmp(input, "quit\n") == 0) {
            printf("Exiting myshell\n");
            break;  // Exit the infinite loop if the user enters "quit"
        }
        else {
            execute(lineFromInput);
            freeCmdLines(lineFromInput);
        }

    }
    return 0;
}



