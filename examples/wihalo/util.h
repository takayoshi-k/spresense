
#ifndef __utilh__
#define __utilh__


double get_system_time(void);
char*  strupr(char *str);


#define print_time() 	printf("<%lf> ", get_system_time());
#endif
