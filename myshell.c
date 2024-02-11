#include <stdio.h>
#include "LineParser.c"
#include <linux/limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0

int debug = 0;

 typedef struct process{
        cmdLine* cmd;                         /* the parsed command line*/
        pid_t pid; 		                  /* the process id that is running the command*/
        int status;                           /* status of the process: RUNNING/SUSPENDED/TERMINATED */
        struct process *next;	                  /* next process in chain */
    } process;


void addProcess(process** process_list, cmdLine* cmd, pid_t pid){
    process* new_process = (process*)malloc(sizeof(process));
    new_process->cmd = cmd;
    new_process->pid = pid;
    new_process->status = RUNNING;
    new_process->next = *process_list;
    *process_list = new_process;
    
}

void printProcessList(process** process_list){
    printf("PID\tCommand\t\tSTATUS\n");
    process* current = *process_list;
    while (current != NULL) {
        printf("%d\t%s", current->pid, current->cmd->arguments[0]);
        switch (current->status) {
            case TERMINATED:
                printf("\tTerminated\n");
                break;
            case RUNNING:
                printf("\tRunning\n");
                break;
            case SUSPENDED:
                printf("\tSuspended\n");
                break;
            default:
                printf("\tStatus is wrong\n");
                break;
        }
        current = current->next;
    }
}


void pipecmd(cmdLine *lineFromInput , process** process_list) {
    
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
        printf("fork in child1 pipe\n");
        close(STDOUT_FILENO);
        dup(pd[1]);
        close(pd[1]);
        close(pd[0]);
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
        if(execvp(lineFromInput->arguments[0],lineFromInput->arguments)==-1){
            perror("Error in execv");
            freeCmdLines(lineFromInput);
            exit(1);
        }
    }
    //parent process
    else {
        addProcess(process_list,lineFromInput,child1PID);
        fprintf(stderr, "(parent_process>created child1 process with id: %d)\n", child1PID);
        close(pd[1]);
        pid_t child2PID = fork();
        if(child2PID==-1) {
            perror("error in fork2. Exiting\n");
            exit(1);
        }
        //child2 process
        if(child2PID==0) {
            printf("fork in child2 pipe\n");
            close(STDIN_FILENO);
            dup(pd[0]);
            close(pd[0]);
            close(pd[1]);
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
            if(execvp(lineFromInput->next->arguments[0],lineFromInput->next->arguments)==-1){
                 perror("Error in execv");
                freeCmdLines(lineFromInput);
                exit(1);
            }
        }
        else {
            addProcess(process_list,lineFromInput,child2PID);
            fprintf(stderr, "(parent_process>created child2 process with id: %d)\n", child2PID);
            close(pd[0]);
            waitpid(child1PID, NULL, 0);
            waitpid(child2PID, NULL, 0);
        }
    }
}


int commands(cmdLine *lineFromInput, process** process_list) {
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
    if (strcmp(lineFromInput->arguments[0], "suspend") == 0) {
        // Handle 'suspend' command
        pid_t pid = atoi(lineFromInput->arguments[1]);
        if (kill(pid, SIGTSTP) == -1) {
            perror("Error in suspend");
            freeCmdLines(lineFromInput);
            exit(1);
        }
        else {
            printf("suspend succeded\n");
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


void execute(cmdLine *lineFromInput,process** process_list) {
    
    if(commands(lineFromInput, process_list)) return;
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
            
            pipecmd(lineFromInput , process_list);
        }
else {
    pid_t PID = fork();
    if( PID == -1) { // error
            perror("Error in fork");
            freeCmdLines(lineFromInput);
            exit(1);
        }
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

     else { 
         if (execvp(lineFromInput->arguments[0], lineFromInput->arguments) == -1){
            perror("Error in execv");
            freeCmdLines(lineFromInput);
            exit(1);
     }
        }
    
    }

    else { //parent

        addProcess(process_list,lineFromInput,PID);
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
    }
}



int main(int argc, char **argv) {
    process* process_list = NULL;
    char input[2048];
    char cwd[PATH_MAX];
    for (int i=0; i<argc;i++) {
        if (strcmp(argv[i],"-d") ==0 ) {
            debug = 1;
        }
    }
    while(1) {

        if (getcwd(cwd, PATH_MAX) != NULL) {
            printf("current working directory: %s\n", cwd);
        } else {
            perror("error in getcwd function");
        }
        printf("enter input\n");
        fgets(input, sizeof(input), stdin);

        cmdLine *lineFromInput = parseCmdLines(input);

        if (strcmp(input, "quit\n") == 0) {
            printf("Exiting myshell\n");
            break;  // Exit the infinite loop if the user enters "quit"
        }
        else if (strcmp(input, "procs\n")==0) {
            printProcessList(&process_list);
        }
        else {
            execute(lineFromInput , &process_list);
            freeCmdLines(lineFromInput);
        }

    }
    return 0;
}






}



