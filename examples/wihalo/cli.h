#ifndef cli__h
#define cli__h


#include <stdint.h>
#include <stdio.h>

#define Printf(...) {if(enable_debug) printf(__VA_ARGS__);}

#define CLI_BUFSZ 1024

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CLI_TYPE_ {
	const char *Str;
	const char *Help;
	int (*Function)(void);
} CLI_TYPE, *PCLI_TYPE;

int CliProcess(const CLI_TYPE *cli_list, char *cli_input, uint8_t show_help);
void CliProcessHelp(const CLI_TYPE *cli);
char* CliGetNextToken(void);

extern char *cli_tok;
extern char cli_in[CLI_BUFSZ];
extern uint8_t enable_debug;

char* cli_get_stream(void);

#ifdef __cplusplus
}
#endif

#endif
