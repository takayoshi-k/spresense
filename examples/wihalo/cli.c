

#include <string.h>
#include "cli.h"

char *cli_tok;
char cli_in[CLI_BUFSZ];
int cli_len;
char *cli_next_token, *cli_base_str;

int CliProcess(const CLI_TYPE *cli, char *cli_input, uint8_t sh)
{
	const CLI_TYPE *ptr = cli;

	cli_len = strlen(cli_input);
	cli_base_str = cli_input;

	char *ap = strtok(cli_input, " ");
	
	if (ap) {
		for (; ptr->Str; ptr++) {
			if (strcmp(ap, ptr->Str) == 0) {
				cli_tok = cli_input + strlen(ap) + 1;
				return (ptr->Function());
			}
		}
	}
	if (sh) CliProcessHelp(cli);
	return -1;
}

void CliProcessHelp(const CLI_TYPE *cli) {
	int i;
	printf("\nHelp information\n");
	for (i = 0; i < 255; i++) {
		if (!cli[i].Function) break;
		printf("%-10s : ", cli[i].Str);
		if (cli[i].Help) printf("%s\n", cli[i].Help);
		else printf("\n");
	}
	printf("\n");
}

char* CliGetNextToken()
{
	cli_next_token = strtok(NULL," ");
	return cli_next_token;
}

char* cli_get_stream (void) {
	char *str1;

	str1 = cli_next_token + strlen(cli_next_token) + 1;
	if(str1 - cli_base_str > cli_len) return NULL;
	return str1;

}


/*
AT+WIPADDR=”192.168.1.20”,”255.255.255.0”,”192.168.1.1”
*/