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
#define HISTLEN 20 // history length

int debug = 0;
int newest = 0,oldest = 0 , sizeOfHistory = 0;
char* historyArr[HISTLEN];

 typedef struct process{
        cmdLine* cmd;                         /* the parsed command line*/
        pid_t pid; 		                  /* the process id that is running the command*/
        int status;                           /* status of the process: RUNNING/SUSPENDED/TERMINATED */
        struct process *next;	                  /* next process in chain */
    } process;

void freeCmdLinesOfLeft(cmdLine *pCmdLine)
{
    int i;
    if (!pCmdLine)
        return;

    FREE(pCmdLine->inputRedirect);
    FREE(pCmdLine->outputRedirect);
    for (i=0; i<pCmdLine->argCount; ++i)
        FREE(pCmdLine->arguments[i]);

    FREE(pCmdLine);
}


void freeHistory() {
    for (int i=0;i<sizeOfHistory;i++) {
        free(historyArr[i]);
    }
}

void addToHistory(char* command) {
    int full=0;
    if(sizeOfHistory==HISTLEN) {
        full = 1;
        free(historyArr[oldest]);
    }
    historyArr[newest] = strdup(command);
    if (full) oldest = (oldest+1) % HISTLEN;
    newest = (newest+1) % HISTLEN;
    sizeOfHistory = (sizeOfHistory < HISTLEN) ? (sizeOfHistory+1) : sizeOfHistory;
     
}

void printHistory() {
    if (sizeOfHistory==0) {
        printf("no history\n");
    }
    int curr = oldest;
    for (int i=0;i<sizeOfHistory;i++) {
        printf("%d:%s",i+1,historyArr[curr]);
        curr = (curr+1) % HISTLEN;
    }
}

void freeProcessList(process* process_list){
    
    while (process_list) {
        process* tmp = process_list ->next;
        if(process_list->cmd)
            freeCmdLines(process_list->cmd);
        if(process_list) 
            free(process_list);
        process_list = tmp;     
    }
}
void updateProcessStatus(process* process_list, int pid, int status) {
    process* current = process_list;
    while (current != NULL) {
        if (current->pid == pid) {
            current->status = status;
            break; // No need to continue searching
        }
        current = current->next;
    }
}
void updateProcessList(process **process_list) {
    process* current = *process_list;
    while (current != NULL) {
        int status;
        pid_t result = waitpid(current->pid, &status, WNOHANG);
        if (result == -1) {
            updateProcessStatus(current,current->pid,TERMINATED);
            current = current->next;
            continue;
        }
        else if (result!=0) {
            // Process has terminated
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                // Process exited normally or due to a signal
                updateProcessStatus(current,current->pid,TERMINATED);
            } else if (WIFSTOPPED(status)) {
                // Process was stopped by a signal
                updateProcessStatus(current,current->pid,SUSPENDED);
            } else if (WIFCONTINUED(status)) {
                // Process was resumed by a signal
                updateProcessStatus(current,current->pid,RUNNING);
            }
            // Move to the next process
    }
 
    current = current->next;
}
    }

void addProcess(process** process_list, cmdLine* cmd, pid_t pid){
    process* new_process = (process*)malloc(sizeof(process));
    new_process->cmd = cmd;
    new_process->pid = pid;
    new_process->status = RUNNING;
    new_process->next = *process_list;
    *process_list = new_process;
    
}

void printProcessList(process** process_list){
    updateProcessList(process_list);
    printf("PID\tCommand\t\t\tSTATUS\n");
    process* current = *process_list;
    process* prev = NULL;
    while (current != NULL) {
        printf("%d\t%s\t\t", current->pid, current->cmd->arguments[0]);
        switch (current->status) {
            case TERMINATED:
                printf("\tTerminated\n");
               if (prev == NULL) {
                    *process_list = current->next;
                } else {
                    prev->next = current->next;
                }
                process* tmp = current;
                current = prev ? prev->next : *process_list;
                if(!tmp->cmd->next) freeCmdLines(tmp->cmd);
                else freeCmdLinesOfLeft(tmp->cmd);
                free(tmp);
                break;
            case RUNNING:
                printf("\tRunning\n");
                prev = current;
                current = current->next;
                break;
            case SUSPENDED:
                printf("\tSuspended\n");
                prev = current;
                current = current->next;
                break;
            default:
                printf("\tStatus is wrong\n");
                prev = current;
                current = current->next;
                break;
        }
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
                int outputFile = open(lineFromInput->next->outputRedirect, O_WRONLY|O_CREAT | O_TRUNC);
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
            addProcess(process_list,lineFromInput->next,child2PID);
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
            updateProcessStatus(*process_list,pid,RUNNING);
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
            updateProcessStatus(*process_list,pid,SUSPENDED);
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
            updateProcessStatus(*process_list,pid,TERMINATED);
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
                freeCmdLines(lineFromInput->next);
                exit(1);
            }
            else if(lineFromInput->next->inputRedirect){
                perror("cant do input redirect on right side of pipe. Exiting\n");
                freeCmdLines(lineFromInput);
                freeCmdLines(lineFromInput->next);
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
        int outputFile = open(lineFromInput->outputRedirect, O_WRONLY|O_CREAT| O_TRUNC);
        if (outputFile == -1) {
            perror("Error in open for output");
            freeCmdLines(lineFromInput);
            exit(1);
        }
        dup2(outputFile,STDOUT_FILENO);
        close(outputFile);
        
    }

         if (execvp(lineFromInput->arguments[0], lineFromInput->arguments) == -1){
            perror("Error in execv");
            freeCmdLines(lineFromInput);
            exit(1);
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
    // for (int i=0;i<HISTLEN;i++) {
    //     historyArr[i] = NULL;
    // }

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
            freeCmdLines(lineFromInput);
            printf("Exiting myshell\n");
            break;  // Exit the infinite loop if the user enters "quit"
        }
        else if (strcmp(input, "procs\n")==0) {
            printHistory();
            printProcessList(&process_list);
            freeCmdLines(lineFromInput);
        }
        else if(strcmp(input,"history\n")==0)
        {
            printHistory();
            freeCmdLines(lineFromInput);
        }
        else if(strcmp(input,"!!\n")==0)
        {
            if(sizeOfHistory) {
            int index = newest==0? HISTLEN-1: newest-1;
            cmdLine *lastLineExecuted = parseCmdLines(historyArr[index]);
            execute(lastLineExecuted , &process_list);
            freeCmdLines(lineFromInput);
        }
        else {
            printf("no history\n");
        }
        }
        else if(strcmp(input,"!")==0)
        {
            int index = atoi(lineFromInput->arguments[0]+1);
            if( index >=0 && index < sizeOfHistory ) {
            cmdLine *nIndexHistory = parseCmdLines(historyArr[index]);
            execute(nIndexHistory,&process_list);
            freeCmdLines(lineFromInput);
            }
            else
             printf("invalid index\n");
              
        }
        
        else {
            addToHistory(input);
            execute(lineFromInput , &process_list);
            // freeCmdLines(lineFromInput);
        }

    }
    freeHistory();
    freeProcessList(process_list);
    return 0;
}



