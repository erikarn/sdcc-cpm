#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cpmbdos.h"
#include "cprintf.h"
#include "syslib/cpm_sysfunc.h"
#include "syslib/pcw_term.h"

static struct cur_editor_state {
	/* Current cursor x/y position */
	uint8_t cur_x;
	uint8_t cur_y;

	/* Top screen line */
	uint16_t top_screen_line;

	/*
	 * Viewport width/height - eg needing to know
	 * if it's 80x24 or 80x31 (or something else!)
	 */
	uint8_t port_w, port_h;
} cur_state;

/* ***************************************************************** */

/*
 * Line storage.
 *
 * Lines are stored and accessed through a hacked up hybrid
 * list/array thing to hopefully make it quick-ish to access
 * lines given an arbitrary line number.
 *
 * Each "line" in memory contains a two byte header - how many
 * characters the buffer can hold, how many characters are
 * currently in the line.
 */

struct text_line {
	uint8_t len;
	uint8_t size;
	char *buf;
};

struct text_line *
text_line_alloc(uint8_t size)
{
	struct text_line *l;

	l = calloc(1, sizeof(struct text_line) + (sizeof(char) * size));
	if (l == NULL) {
		return NULL;
	}
	l->buf = calloc(size, sizeof(char));
	if (l->buf == NULL) {
		free(l);
		return (NULL);
	}
	l->len = 0;
	l->size = size;
	return (l);
}

void
text_line_free(struct text_line *l)
{
	free(l->buf);
	free(l);
}

struct text_line_array_entry;
struct text_line_array_entry {
	struct text_line_array_entry *next;
	uint16_t line_start;
	uint8_t num_lines;
	struct text_line *lines[];
};

struct text_line_array_entry *text_array;

struct text_line_array_entry *
text_line_array_create(uint16_t line_start, uint8_t num_lines)
{
	struct text_line_array_entry *e;
	e = calloc(1, sizeof(struct text_line_array_entry) +
	    ((sizeof(struct text_line *) * num_lines)));
	if (e == NULL) {
		return (NULL);
	}

	e->line_start = line_start;
	e->num_lines = num_lines;

	return (e);
}

/**
 * Free the text line array and all the lines it contains.
 */
void
text_line_array_free(struct text_line_array_entry *e)
{
	uint8_t l;
	for (l = 0; l < e->num_lines; l++) {
		if (e->lines[l] != NULL)
			text_line_free(e->lines[l]);
	}
	free(e);
}

/*
 * Actual global text array ops.
 */

/**
 * Free the entire text array, set it to NULL.
 */
void
text_array_free(void)
{
	struct text_line_array_entry *e, *en;

	e = text_array;
	while (e != NULL) {
		en = e->next;
		text_line_array_free(e);
		e = en;
	}
	text_array = NULL;
}

/**
 * Initialise an initial text array, 32 lines, no content.
 */
bool
text_array_init(void)
{
	text_array = text_line_array_create(1, 32);
	if (text_array == NULL) {
		return false;
	}
	return true;
}

/**
 * Find the line with the given line number.
 *
 * If the line exists, return the line entry itself.
 */

/* ***************************************************************** */

void sys_init(void) {
	cpm_sysfunc_init();
	TERM_pcw_init();
}

int main() {
	uint8_t ch;
	uint8_t x;

	sys_init();
	TERM_pcw_clr_home();

	text_array_init();

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


