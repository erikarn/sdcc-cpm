

#include <stdlib.h>

#include "cpm_sysfunc.h"
#include "common_datatypes.h"
#include "shared_term.h"
#include "pcw_term.h"

#define	CHAR_BEL		0x07
#define	CHAR_ESC		27

void
TERM_pcw_init(void)
{
}

void
TERM_pcw_beep(void)
{
	cpm_putchar(CHAR_BEL);
}

void
TERM_pcw_status_disable(void)
{
	term_sendCommand("0");
}

void
TERM_pcw_status_enable(void)
{
	term_sendCommand("1");
}

void
TERM_pcw_set_charset(char set)
{
	char cmd[] = "2 ";
	cmd[1] = set;
	term_sendCommand(cmd);
}

void
TERM_pcw_cursor_move(cursor_move_dir_t dir)
{
	char cmd[] = "A";
	cmd[0] = cmd[0] + dir;
	term_sendCommand(cmd);
}

void
TERM_pcw_clr(void)
{
	term_sendCommand("E");
}

void
TERM_pcw_home(void)
{
	term_sendCommand("H");
}

void
TERM_pcw_clr_home(void)
{
	TERM_pcw_clr();
	TERM_pcw_home();
}

void
TERM_pcw_cursor_move_up_scroll(void)
{
	term_sendCommand("I");
}

void
TERM_pcw_erase_page_end(void)
{
	term_sendCommand("J");
}

void TERM_pcw_erase_line_end(void)
{
	term_sendCommand("K");
}

void TERM_pcw_erase_insert_line(void)
{
	term_sendCommand("L");
}

void TERM_pcw_erase_delete_line(void)
{
	term_sendCommand("M");
}

void TERM_pcw_erase_delete_char(void)
{
	term_sendCommand("N");
}

void
TERM_pcw_set_viewport(char top, char left,
    char height, char width)
{
	char cmd[] = "X    ";

	cmd[1] = top + 32;
	cmd[2] = left + 32;
	cmd[3] = height - 1 + 32;
	cmd[4] = width - 1 + 32;

	term_sendCommand(cmd);
}

void
TERM_pcw_move_cursor(char x, char y)
{
	char cmd[] = "Y  ";
	cmd[1] = y + 32;
	cmd[2] = x + 32;

	term_sendCommand(cmd);
}

void
TERM_pcw_viewport_clear_upto(void)
{
	term_sendCommand("d");
}

void
TERM_pcw_enable_cursor(void)
{
	term_sendCommand("e");
}

void
TERM_pcw_disable_cursor(void)
{
	term_sendCommand("f");
}

void TERM_pcw_save_cursor_pos(void)
{
	term_sendCommand("j");
}

void TERM_pcw_restore_cursor_pos(void)
{
	term_sendCommand("k");
}

void
TERM_pcw_clear_line(void)
{
	term_sendCommand("l");
}

void
TERM_pcw_clear_line_upto(void)
{
	term_sendCommand("o");
}

void
TERM_pcw_set_reverse_mode(char enable)
{
	term_sendCommand(enable ? "p" : "q");
}

void
TERM_pcw_set_underline(char enable)
{
	term_sendCommand(enable ? "r" : "u");
}

void
TERM_pcw_set_wrapping(char enable)
{
	term_sendCommand(enable ? "v" : "w");
}

void
TERM_pcw_write_char(char ch)
{
	if (ch < 32) {
		cpm_putchar(CHAR_ESC);
	}
	cpm_putchar(ch);
}
