#ifndef ADAPT_H
#define ADAPT_H

#if defined(_WIN32) || defined(_WIN64)
    #include <conio.h>
    #include <process.h>

    #define getch() _getch()
    #define _spawnlp _spawnlp
#else
    int getch(void);
    int _spawnlp(int mode, const char *cmd, const char *arg0, ...);
#endif

#endif
