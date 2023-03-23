#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpmbdos.h"
#include "cprintf.h"
#include "syslib/cpm_sysfunc.h"
#include "syslib/pcw_term.h"

void sys_init(void) {
	cpm_sysfunc_init();
	TERM_pcw_init();
}

int main() {
	uint8_t ch;
	uint8_t x;

	sys_init();
	TERM_pcw_clr_home();

	cprintf("Adrian's Editor: Hi!\n");

	/* Draw an X */
	for (x = 0; x < 11; x++) {
		TERM_pcw_move_cursor(5 + x, 5 + x);
		TERM_pcw_write_char('*');
		TERM_pcw_move_cursor(15 - x, 5 + x);
		TERM_pcw_write_char('*');
	}

	while (1) {
		ch = cpm_rawio_read_wait();
		if (ch == 'a')
			break;
		TERM_pcw_write_char(ch);
	}

	return (EXIT_SUCCESS);
}


