#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

int regular_command(char** arglist);
int background_command(char** arglist, int i);
int piping_command(char** arglist, int i);
int arrow_command(char** arglist, int i);
void reset_handler();

void reset_handler()
{
    struct sigaction terminate; // https://unix.stackexchange.com/questions/351312/sigint-handler-runs-only-once
    terminate.sa_handler = SIG_DFL;
    sigemptyset(&(terminate.sa_mask));
    sigaddset(&(terminate.sa_mask), SIGINT);
    terminate.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &terminate, NULL) != 0)
    {
        fprintf(stderr, "SIGINT handler failed!\n");
        exit(1);
    }
}

int background_command(char** arglist, int i)
{
    arglist[i] = NULL;
    int pid = fork();
    switch(pid)
    {
        case -1:
            fprintf(stderr, "Error with fork!\n");
            exit(1);
        break;
        case 0:
            execvp(arglist[0], arglist);
            fprintf(stderr, "Error with executing command %s\n", arglist[0]);
            exit(1);
        break;
        default:
            return 1;
        break;
    }
}
int piping_command(char** arglist, int i)
{
    arglist[i] = NULL;
    int fd[2];
    if (pipe(fd) == -1)
    {
        fprintf(stderr, "Error with creating pipe!\n");
        exit(1);
    }
    int pid1 = fork();
    if (pid1 < 0)
    {
        fprintf(stderr, "Error with fork!\n");
        exit(1);
    }
    else if (pid1 == 0)
    {   // first child
        reset_handler();
        close(fd[0]); // first child doesn't need to read
        if (dup2(fd[1], STDOUT_FILENO) == -1)
        {
            fprintf(stderr, "Error with dup2!\n");
        }
        close(fd[1]);
        execvp(arglist[0], arglist);
        fprintf(stderr, "Error executing command %s\n", arglist[0]);
        exit(1);
    }        
    else
    { // father
        close(fd[1]); // no need for writing
        int pid2 = fork();
        if (pid2 < 0)
        {
            fprintf(stderr, "Error\n");
            exit(1);
        }
        else if (pid2 == 0)    // second child
        {  
            reset_handler();
            if(dup2(fd[0], STDIN_FILENO) == -1)
            {
                fprintf(stderr, "Error with dup2!\n");
                return 0;
            };
            close(fd[0]);
            execvp(arglist[i+1], arglist + i + 1); 
            fprintf(stderr, "Error executing command %s\n", arglist[i+1]);
            exit(1);
        }
        else
        {
            close(fd[0]);
            if ((waitpid(pid1, NULL, 0) == -1) && (errno != ECHILD) && (errno != EINTR))
            {
                fprintf(stderr, "Wait failed!");
                return 0;
            }
            if ((waitpid(pid2, NULL, 0) == -1) && (errno != ECHILD) && (errno != EINTR))
            {
                fprintf(stderr, "Wait failed!");
                return 0;
            }
            return 1;
        }    
    }
    
}
int arrow_command(char** arglist, int i)
{
    arglist[i] = NULL;
    int file = open(arglist[i+1], O_RDWR | O_CREAT, 0777);
    if (file == -1)
    {
        fprintf(stderr, "Error with file: %s", arglist[i+1]);
        exit(1);
    }
    int fd[2];
    if (pipe(fd) == -1)
    {
        fprintf(stderr, "Error\n");
        exit(1);
    }
    int pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "Error\n");
        exit(1);
    }
    else if (pid == 0)
    {
        reset_handler();
        if (dup2(file, STDOUT_FILENO) == -1)
        {
            fprintf(stderr, "Error with dup2!\n");
            return 0;
        }
        close(file);
        close(fd[0]);
        close(fd[1]);
        execvp(arglist[0], arglist);
        fprintf(stderr, "Error executing command %s\n", arglist[0]);
        exit(1);
    }
    else
    {
        if ((waitpid(pid, NULL, 0) == -1) && (errno != ECHILD) && (errno != EINTR))
        {
            fprintf(stderr, "Wait failed!");
            return 0;
        }
        close(file);
        close(fd[0]);
        close(fd[1]);
        return 1;
    }

}

int regular_command(char** arglist)
{
    int pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "Error\n");
        exit(1);
    }
    else if (pid == 0)
    {
        reset_handler();
        execvp(arglist[0], arglist);
        fprintf(stderr, "Error executing command %s\n", arglist[0]);
        exit(1);
    }
    else
    {
        if ((waitpid(pid, NULL, 0) == -1) && (errno != ECHILD) && (errno != EINTR))
        {
            fprintf(stderr, "Wait failed!");
            return 0;
        }
        return 1;
    }
}
int prepare()
{
    struct sigaction ignore_signal;  // https://unix.stackexchange.com/questions/351312/sigint-handler-runs-only-once
    ignore_signal.sa_handler = SIG_IGN;
    sigemptyset(&(ignore_signal.sa_mask));
    sigaddset(&(ignore_signal.sa_mask), SIGINT);
    sigaction(SIGCHLD, &ignore_signal, NULL);
    ignore_signal.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &ignore_signal, NULL) != 0)
    {
        fprintf(stderr, "SIGINT handler failed!\n");
        exit(1);
    }
    if (sigaction(SIGCHLD, &ignore_signal, NULL) != 0)
    {
        fprintf(stderr, "SIGCHLD handler failed!\n");
        exit(1);  
    }
    return 0;
}

int process_arglist(int count, char** arglist)
{
    char* tmp;
    for (int i = 0; i < count; i++)
    {
        tmp = arglist[i];
        if (*tmp == '|')
        {
            return piping_command(arglist, i);
        }
        else if (*tmp == '>')
        {
            return arrow_command(arglist, i);
        }
        else if (*tmp == '&')
        {
            return background_command(arglist, i); 
        }
    }
    return regular_command(arglist);
    
}
int finalize()
{
    return 0;
}