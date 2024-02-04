#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char *args1[] = {"ls", "-l", NULL};
char *args2[] = {"tail", "-n", "2", NULL};

int main(){
    int pd[2];
    if(pipe(pd)==-1) {
        perror("couldnt open a pipe. Exiting\n");
        exit(1);
    }
    fprintf(stderr, "(parent_process>forking child1...)\n");
    pid_t child1PID = fork();
    if(child1PID == -1) {
        perror("error in fork1. Exiting\n");
        exit(1);
    }
    //child1 process
    if(child1PID==0) {
        fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe...)\n");
        close(STDOUT_FILENO);
        dup(pd[1]);
        close(pd[1]);
        fprintf(stderr, "(child1>going to execute cmd: ls -l)\n");
        execvp(args1[0],args1);
    }
    //parent process
    else {
        fprintf(stderr, "(parent_process>created child1 process with id: %d)\n", child1PID);
        fprintf(stderr, "(parent_process>closing writeEnd...)\n");
        close(pd[1]);
    }
    fprintf(stderr, "(parent_process>forking child2...)\n");
    pid_t child2PID = fork();
    if(child2PID==-1) {
        perror("error in fork2. Exiting\n");
        exit(1);
    }
    //child2 process
    if(child2PID==0) {
        fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe...)\n");
        close(STDIN_FILENO);
        dup(pd[0]);
        close(pd[0]);
        fprintf(stderr, "(child2>going to execute cmd: tail -n 2)\n");
        execvp(args2[0],args2);
    }
    else {
        fprintf(stderr, "(parent_process>created child2 process with id: %d)\n", child2PID);
        fprintf(stderr, "(parent_process>closing readEnd...)\n");
        close(pd[0]);
    }
    fprintf(stderr, "(parent_process>waiting for children process...)\n");
    waitpid(child1PID, NULL, 0);
    waitpid(child2PID, NULL, 0);
    fprintf(stderr, "(parent_process>exiting after finishing...)\n");
    return 0;
}