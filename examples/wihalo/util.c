

#include <time.h>
#include <ctype.h>

double get_system_time(void) 
{
    struct timespec __tm;
    double dtime;

    clock_gettime(CLOCK_MONOTONIC, &__tm);
    dtime = __tm.tv_sec + __tm.tv_nsec*1e-9;
    return (dtime);
}

char* strupr(char *str) 
{
    char *rest = str;
    while(*str) {
        *str++ = toupper(*str);
    }
    return rest;
}