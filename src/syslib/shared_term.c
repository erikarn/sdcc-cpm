
#include <stdlib.h>

#include "cpm_sysfunc.h"
#include "common_datatypes.h"
#include "shared_term.h"

#define		ESC_CHAR		27

void
term_sendCommand(char *cmd)
{
	int idx = 0;

	cpm_putchar(ESC_CHAR);
	while (cmd[idx] != '\0') {
		cpm_putchar(cmd[idx]);
		idx++;
	}
}
