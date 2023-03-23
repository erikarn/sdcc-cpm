#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpmbdos.h"
#include "cprintf.h"
#include "syslib/cpm_sysfunc.h"
#include "syslib/ansi_term.h"

void sys_init(void) {
	cpm_sysfunc_init();
	term_ANSIMode();
}

int main() {
	uint8_t ch;

	sys_init();
	term_ANSIClrScrn(ed_erase_all);

	cprintf("Adrian's Editor: Hi!\n");
#if 0
	while (1) {
		ch = cpm_rawio_read_wait();
		if (ch == 'a')
			break;
		cpm_putchar(ch);
	}
#endif

    int x;

    printf("Before CLS\n");
    printf("\x1b\x45");	 // ESC E -> Clear screen
    printf("\x1b\x48");  // ESC H -> Home
    printf("If this is the first thing you can read, CLS is OK.\n");

    /* Draw an X */
    for (x = 0; x < 11; x++) {
		// ESC Y r c -> gotoxy
		printf("\x1b\x59%c%c*",32 + 10 + x, 32 + 20 + x);
		printf("\x1b\x59%c%c*",32 + 20 - x, 32 + 20 + x);
    }


	return (EXIT_SUCCESS);
}


