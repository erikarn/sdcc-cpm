#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef	CPM
#include "syslib/cpm_sysfunc.h"
#include "syslib/pcw_term.h"
#include "cpm_defs.h"
#include "cpmbdos.h"
#else
#include <sys/types.h>
#include "unix_sysfunc.h"
#include "pcw_term.h"
#endif


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
#define	KEY_FF				12

/*
 * XXX TODO: not used yet
 */
#define	EDITOR_MAX_LINE_LEN		80
#define	EDITOR_MAX_LINE_COUNT		50

/* ******************************** */


#ifdef	CPM

extern uint16_t free_tpa_end;
extern uint16_t free_tpa_start;

#if 0
#include "cprintf.h"
void
get_tpa_range(void)
{
	uint16_t bdos_start;

	/* End is the start of BDOS */
	memcpy(&bdos_start, (void *) 0x0006, sizeof(uint16_t));

	cprintf("%s: (addr %x) end=%x\n", __func__,
	    free_tpa_start, free_tpa_end);
	cprintf("%s: BDOS start: %x\n", __func__, bdos_start);
}
#endif

/* XXX because this isn't a function in the SDCC lib, sigh */
struct header;
struct header {
	struct header *next, *next_free;
};

extern struct header * __sdcc_heap_free;

static void
add_tpa_to_sdcc_heap(void)
{
	struct header *s;

	s = (void *) free_tpa_start;
	s->next = (void *) free_tpa_end;
	s->next_free = (void *) __sdcc_heap_free;
	__sdcc_heap_free = s;
}

#endif


struct text_line;

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

	struct text_line *cur_line;
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

static inline uint16_t
get_cur_line_number(void)
{
	return (cur_state.top_screen_line + cur_state.cur_y);
}

static inline uint8_t
get_cur_col_number(void)
{
	return (cur_state.left_screen_col + cur_state.cur_x);
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

	l = calloc(1, sizeof(struct text_line));
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

bool
text_line_realloc(struct text_line *l, uint8_t size)
{
	char *b;
	b = realloc(l->buf, size);
	if (b == NULL) {
		return false;
	}
	l->buf = b;
	l->size = size;

	return true;
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
	for (lx = 0; lx < cur_state.port_w; lx++, sx++) {
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
	platform_sys_init();
	TERM_pcw_init();
}

/*
 * Handle the 'insert' / 'append' commands.
 */
static void
handle_insert_cmd(void)
{
	struct text_line *l;
	uint16_t ln;

	/*
	 * Is there a line at the current line number?
	 * If not, we should try to create one first.
	 */
	ln = get_cur_line_number();
	l = text_line_fetch(ln, 2);		// Short for now, can expand it later
	if (l == NULL) {
		TERM_pcw_beep();
		return;
	}

	/* Update our line cache */
	cur_state.cur_line = l;

	/*
	 * For now let's redraw the line, in case it's a '~' that
	 * we need to update.  Like, that's pretty over the top
	 * inefficiency wise, but I wanna working editor before I
	 * want a super efficiently fast editor.
	 */
	repaint_line(cur_state.cur_y);

	/* And now we're in "edit" mode */
	/* XXX make it an inline func for debugging */
	cur_state.editor_mode = EDITOR_MODE_EDIT;
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
		ch = platform_console_read_char();
		if (ch == KEY_ESC) {
			goto exit;
		}
		/* PCW: enter = KEY_CR; unix , enter = KEY_LF */
		if (ch == KEY_CR | ch == KEY_LF) {
			goto done;
		}
		if (ch == KEY_DEL) {
			if (p > 0) {
				/* XXX */
				platform_console_write_char(KEY_BS);
				p--;
			} else {
				TERM_pcw_beep();
			}
		}
		if (ch >= 32 && ch < 127) {
			if (p < TMP_BUF_LEN) {
				tmp_buf[p++] = ch;
				/* XXX */
				platform_console_write_char(ch);
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
handle_keypress_cursor_up(void)
{
	struct text_line *l;
	uint16_t ln;

	/* See if we can even go up */
	ln = get_cur_line_number();
	if (ln == 0) {
		goto error;
	}

	/*
	 * See if a line exists before we try
	 * updating the screen.
	 */
	l = text_line_lookup(ln - 1);
	if (l == NULL) {
		goto error;
	}

	/*
	 * Ok, a line exists.  Move the cursor up one,
	 * scroll the screen if possible, update the
	 * current text line.  Note that we clamp the
	 * cursor to the end of the new line.
	 */

	/* Can just move the cursor */
	if (cur_state.cur_y > 0) {
		cur_state.cur_y--;
		TERM_pcw_move_cursor(cur_state.cur_x, cur_state.cur_y);
		goto done;
	}

	/* Need to scroll up one */
	TERM_pcw_move_cursor(0, 0);
	TERM_pcw_cursor_move_up_scroll();

done:

	/* XXX TODO: clamp */

	cur_state.cur_line = l;
	TERM_pcw_move_cursor(cur_state.cur_x, cur_state.cur_y);
	return;

error:
	TERM_pcw_beep();
	return;
}

static void
handle_keypress_cursor_down(void)
{
	struct text_line *l;
	uint16_t ln;

	/*
	 * See if a line exists before we try
	 * updating the screen.
	 */
	ln = get_cur_line_number();
	l = text_line_lookup(ln + 1);
	if (l == NULL) {
		TERM_pcw_beep();
		return;
	}

	/*
	 * Ok, a line exists.  Move the cursor down one,
	 * scroll the screen if possible, update the
	 * current text line.
	 */
	/* XXX TODO */
}

static void
handle_keypress_cursor_left(void)
{
	if (cur_state.cur_line == NULL) {
		goto error;
	}
	if (cur_state.cur_x == 0) {
		/* Potentially shift the screen left and repaint */
		if (cur_state.left_screen_col > 0) {
			/* XXX Move it one at a time to be lazy */
			cur_state.left_screen_col--;
			repaint_screen();
			goto done;
		}
		/* Can't move any further left */
		goto error;
	}

	cur_state.cur_x--;
	TERM_pcw_move_cursor(cur_state.cur_x, cur_state.cur_y);
done:
	return;
error:
	TERM_pcw_beep();
	return;
}

static void
handle_keypress_cursor_right(void)
{
	if (cur_state.cur_line == NULL || cur_state.cur_line->len == 0) {
		goto error;
	}

	/*
	 * Only allow going to the end of the line, not a character over it!
	 */
	if ((cur_state.left_screen_col + cur_state.cur_x) >= (cur_state.cur_line->len - 1)) {
		goto error;
	}

	/*
	 * Check if we need to move the screen right.
	 * We only do this if we have characters left on the line.
	 */
	if (cur_state.cur_x >= cur_state.port_w) {
		if (cur_state.cur_x + cur_state.left_screen_col < cur_state.cur_line->len) {
			/* XXX Move it one at a time to be lazy */
			cur_state.left_screen_col ++;
			repaint_screen();
			goto done;
		} else {
			/* We're at the end of the line */
			goto error;
		}
	}

	cur_state.cur_x++;
	TERM_pcw_move_cursor(cur_state.cur_x, cur_state.cur_y);
done:
	return;

error:
	TERM_pcw_beep();
	return;
}

static void
handle_keypress_cmd(char c)
{
	switch (c) {
	case 'r':		/* should be ctrl-L instead XXX TODO */
		repaint_screen();
		break;
	case 'h':
		handle_keypress_cursor_left();
		break;
	case 'i':
		handle_insert_cmd();
		break;
	case 'j':
		handle_keypress_cursor_down();
		break;
	case 'k':
		handle_keypress_cursor_up();
		break;
	case 'l':
		handle_keypress_cursor_right();
		break;
	case ':':
		handle_colon_cmd();
		break;
	default:
		TERM_pcw_beep();
		break;
	}
}

static void
handle_keypress_edit_newline(char c)
{
	struct text_line *l;
	uint16_t cur_editor_line;

	(void) c;

	/* New line - start by going to the beginning of line */
	cur_state.cur_x = 0;
	cur_state.left_screen_col = 0;

	/* Are we at the end of our size? */
	/*
	 * XXX TODO: again, I should likely store this and calculate
	 * the cursor position..
	 */
	cur_editor_line = cur_state.cur_y + cur_state.top_screen_line;
	if (cur_editor_line >= EDITOR_MAX_LINE_COUNT) {
		TERM_pcw_beep();
		return;
	}

	/*
	 * Now we need to check if we have a next line, and
	 * if not we create one.  (Well a short one for now.)
	 */
	l = text_line_fetch(cur_editor_line + 1, 2);
	if (l == NULL) {
		TERM_pcw_beep();
		return;
	}

	/*
	 * Now, check to see if we need to scroll the screen down
	 * because we're at the bottom of the screen.
	 */
	cur_state.cur_y++;
	if (cur_state.cur_y > cur_state.port_h) {
		cur_state.cur_y--;
		cur_state.top_screen_line++;
		/* XXX repaint for now, will scroll later */
		repaint_screen();
	}

	/* Update cached line */
	cur_state.cur_line = l;

	/* Update cursor, display new now blanked line */
	repaint_line(cur_state.cur_y);
	TERM_pcw_move_cursor(0, cur_state.cur_y);
}

static void
handle_keypress_edit(char c)
{
	uint8_t cur_x_pos;

	if (c == KEY_ESC) {
		/* Drop out of editor mode */
		cur_state.editor_mode = EDITOR_MODE_CMD;
		return;
	}

	/* Definitely need a cached line already here! */
	if (cur_state.cur_line == NULL) {
		TERM_pcw_beep();
		return;
	}

	/*
	 * Depending upon our mode (insert, overwrite)
	 * we will need to potentially do some inserting
	 * and copying and such.
	 *
	 * But for now, to bring the editor up, let's just
	 * support typing keyboard characters and handle DEL,
	 * ignore CR/LF (so we don't change lines), and
	 * just debug the "grow the line buffer if we have
	 * to" logic.
	 */
	/* XXX for now, will handle DEL/BS and other stuff later */
	if (c < 32 || c > 126) {
		return;
	}

	/*
	 * See if we have enough space to add it to the line.
	 * If we don't then we're going to need to reallocate the
	 * line or check if we've hit the maximum line length.
	 */

	/*
	 * XXX I definitely don't like this; it could wrap in really
	 * terrible ways if I'm not careful!
	 *
	 * XXX I likely should change it to have cur_x be the
	 * actual line offset, and I subtract left_screen_col to
	 * get the /actual/ cursor start position.
	 */
	cur_x_pos = cur_state.cur_x + cur_state.left_screen_col;
	if (cur_x_pos >= EDITOR_MAX_LINE_LEN) {
		TERM_pcw_beep();
		return;
	}
	if (cur_x_pos >= cur_state.cur_line->size) {
		/*
		 * For now just go for a full line length; optimise smaller
		 * increments later.
		 */
		if (text_line_realloc(cur_state.cur_line, EDITOR_MAX_LINE_LEN) == false) {
			TERM_pcw_beep();
			return;
		}
	}

	/* It's big enough now, we can edit appropriately */
	cur_state.cur_line->buf[cur_x_pos] = c;
	TERM_pcw_write_char(c);

	/* Advance character */
	cur_x_pos++;

	/* Extend the line too if we need to */
	if (cur_x_pos > cur_state.cur_line->len) {
		cur_state.cur_line->len = cur_x_pos;
	}

	/* Advance cursor */
	cur_state.cur_x++;

	/* If we're at the end of the viewport then we may need to shift
	 * the viewport and repaint! */
	if (cur_state.cur_x >= cur_state.port_w) {
		cur_state.cur_x -= 4;
		cur_state.left_screen_col += 4;
		repaint_screen();
	}
}

int main() {
	uint8_t ch;

	sys_init();
	TERM_pcw_clr_home();

	text_array_init();
	editor_init();

#ifdef	CPM
	//get_tpa_range();
	add_tpa_to_sdcc_heap();
#endif

	repaint_screen();

	while (cur_state.do_exit == false) {
		ch = platform_console_read_char();

		switch (cur_state.editor_mode) {
		case EDITOR_MODE_CMD:
			handle_keypress_cmd(ch);
			break;
		case EDITOR_MODE_EDIT:
			if (ch == KEY_CR | ch == KEY_LF) {
				handle_keypress_edit_newline(ch);
				break;
			}
			handle_keypress_edit(ch);
			break;
		default:
			TERM_pcw_beep();
			break;
		}
		//TERM_pcw_write_char(ch);
	}

	/* Exiting, see if we need to save our file once more */

	return (EXIT_SUCCESS);
}


