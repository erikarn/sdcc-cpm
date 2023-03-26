#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cpmbdos.h"
#include "cprintf.h"
#include "syslib/cpm_sysfunc.h"
#include "syslib/pcw_term.h"

typedef enum {
	EDITOR_MODE_CMD = 0,
	EDITOR_MODE_EDIT = 1,
} editor_mode_t;

typedef enum {
	EDITOR_LINE_ENDING_CRLF = 0,
	EDITOR_LINE_ENDING_LF = 1,
} editor_line_ending_t;

#define	KEY_ESC				27
#define	KEY_LF				10
#define	KEY_CR				13
#define	KEY_DEL				127
#define	KEY_BS				8

static struct cur_editor_state {
	/* Current cursor x/y position */
	uint8_t cur_x;
	uint8_t cur_y;

	/* Top screen line */
	uint16_t top_screen_line;

	/* Start column for rendering */
	uint8_t left_screen_col;

	/*
	 * Viewport width/height - eg needing to know
	 * if it's 80x24 or 80x31 (or something else!)
	 */
	uint8_t port_w, port_h;

	uint8_t editor_mode;
	uint8_t editor_line_ending;
	bool do_exit;
} cur_state;

#define	TMP_BUF_LEN		16
char tmp_buf[TMP_BUF_LEN];

/* ***************************************************************** */

/*
 * Editor state.
 */
void
editor_init(void)
{
	cur_state.cur_x = 0;
	cur_state.cur_y = 0;
	cur_state.top_screen_line = 0;
	cur_state.left_screen_col = 0;
	cur_state.port_w = 90;
	cur_state.port_h = 31;

	cur_state.editor_mode = EDITOR_MODE_CMD;
	cur_state.editor_line_ending = EDITOR_LINE_ENDING_CRLF;
	cur_state.do_exit = false;
}

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
 * Initialise an initial text array, 128 lines, no content.
 */
bool
text_array_init(void)
{
	text_array = text_line_array_create(0, 128);
	if (text_array == NULL) {
		return false;
	}
	return true;
}

/**
 * Find the line with the given line number.
 *
 * If the line exists, return the line entry itself.
 * If the line doesn't exist, return NULL.
 */
struct text_line *
text_line_lookup(uint16_t line)
{
	struct text_line_array_entry *e = text_array;

	while (e != NULL) {
		if (line >= e->line_start && line < e->line_start + e->num_lines) {
			return (e->lines[line - e->line_start]);
		}
		e = e->next;
	}
	return NULL;
}

/*
 * Find or create the given line number.
 *
 * For now, we only support a single text line array entry.
 * I'll look at adding more support later.
 */
struct text_line *
text_line_fetch(uint16_t line, uint8_t size)
{
	struct text_line *l;
	struct text_line_array_entry *e = text_array;

	while (e != NULL) {
		if (line >= e->line_start && line < e->line_start + e->num_lines) {
			/* Found the array entry, line lookup */
			l = (e->lines[line - e->line_start]);
			if (l != NULL) {
				return l;
			}
			/* No line, try to create one */
			l = text_line_alloc(size);
			if (l == NULL) {
				return NULL;
			}
			e->lines[line - e->line_start] = l;
			return l;
		}
		e = e->next;
	}

	/* Couldn't do it, bail! */
	return NULL;
}

/* ***************************************************************** */

/*
 * Repaint the given line.
 *
 * Take left_screen_col into account.
 */
void
repaint_line(uint8_t y)
{
	struct text_line *l;
	uint8_t sx, lx;

	TERM_pcw_move_cursor(0, y);
	l = text_line_lookup(cur_state.top_screen_line + y);
	if (l == NULL) {
		TERM_pcw_write_char('~');
		return;
	}

	/* Iterate over the line itself, print characters */
	sx = cur_state.left_screen_col;
	for (lx = 0; lx < cur_state.cur_x; lx++, sx++) {
		if (sx >= l->len) {
			break;
		}
		/* XXX TODO: hella stupid for now, will make it less stupid later */
		TERM_pcw_write_char(l->buf[sx]);
	}
}

static void
set_viewport_normal(void)
{
	TERM_pcw_set_viewport(0, 0, cur_state.port_h - 1, cur_state.port_w);
	TERM_pcw_move_cursor(cur_state.cur_x, cur_state.cur_y);
}

static void
set_viewport_cmd(void)
{
	TERM_pcw_set_viewport(cur_state.port_h - 1, 0, cur_state.port_h,
	     cur_state.port_w);
	TERM_pcw_move_cursor(0, 0);
}

void
repaint_screen(void)
{
	uint16_t y;

	TERM_pcw_clr_home();
	TERM_pcw_set_wrapping(1);

	set_viewport_normal();

	for (y = 0; y < cur_state.port_h; y++) {
		repaint_line(y);
	}
	TERM_pcw_move_cursor(cur_state.cur_x, cur_state.cur_y);
}

/* ***************************************************************** */

void sys_init(void) {
	cpm_sysfunc_init();
	TERM_pcw_init();
}

static void
handle_colon_cmd(void)
{
	char p = 0, ch;

	set_viewport_cmd();

	TERM_pcw_write_char(':');

	memset(tmp_buf, 0, sizeof(tmp_buf));

	/*
	 * XXX TODO: this is basically a single line editor cmd input loop
	 */
	while (1) {
		ch = cpm_rawio_read_wait();
		if (ch == KEY_ESC) {
			goto exit;
		}
		if (ch == KEY_CR) {
			goto done;
		}
		if (ch == KEY_DEL) {
			if (p > 0) {
				/* XXX */
				cpm_putchar(KEY_BS);
				p--;
			} else {
				TERM_pcw_beep();
			}
		}
		if (ch >= 32 && ch < 127) {
			if (p < TMP_BUF_LEN) {
				tmp_buf[p++] = ch;
				/* XXX */
				cpm_putchar(ch);
			} else {
				TERM_pcw_beep();
			}
		}
	}

done:
	/* Handle commands */
	if (p == 2 && tmp_buf[0] == 'q' && tmp_buf[1] == '!') {
		cur_state.do_exit = true;
	}
	/* fallthrough */

exit:
	/* Restore normal editor window */
	set_viewport_normal();
	/* XXX TODO: repaint bottom line? */
}

static void
handle_keypress_cmd(char c)
{
	switch (c) {
	case 'h':
	case 'j':
	case 'k':
	case 'l':
		break;
	case ':':
		handle_colon_cmd();
		break;
	default:
		TERM_pcw_beep();
		break;
	}
}

int main() {
	uint8_t ch;

	sys_init();
	TERM_pcw_clr_home();

	text_array_init();
	editor_init();

	repaint_screen();

	while (cur_state.do_exit == false) {
		ch = cpm_rawio_read_wait();

		switch (cur_state.editor_mode) {
		case EDITOR_MODE_CMD:
			handle_keypress_cmd(ch);
			break;
		case EDITOR_MODE_EDIT:
		default:
			TERM_pcw_beep();
			break;
		}
		//TERM_pcw_write_char(ch);
	}

	/* Exiting, see if we need to save our file once more */

	return (EXIT_SUCCESS);
}


