#include "adapt.h"

// getch
#if defined(_WIN32) || defined(_WIN64)
    #include <conio.h>
#else
    #include <termios.h>
    #include <unistd.h>
    #include <stdio.h>

    int getch(void) {
        struct termios oldt, newt;
        int ch;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        ch = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return ch;
    }
#endif

// spawnlp
#if defined(_WIN32) || defined(_WIN64)
    #include <process.h>
#else
    #include <unistd.h>
    #include <sys/wait.h>
    #include <stdarg.h>
    #include <stdlib.h>

    int _spawnlp(int mode, const char *cmd, const char *arg0, ...) {
        char *args[256];
        int argc = 0;
        args[argc++] = (char *)arg0;

        va_list valist;
        va_start(valist, arg0);
        char *next_arg = va_arg(valist, char *);
        while (next_arg != NULL && argc < 255) {
            args[argc++] = next_arg;
            next_arg = va_arg(valist, char *);
        }
        args[argc] = NULL;
        va_end(valist);

        pid_t pid = fork();
        if (pid < 0) return -1;
        if (pid == 0) {
            execvp(cmd, args);
            exit(127);
        } else {
            if (mode == 0) { // _P_WAIT
                int status;
                waitpid(pid, &status, 0);
                return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            }
            return pid;
        }
    }
#endif
