#ifndef _PCW_TERM_HEADER_
#define _PCW_TERM_HEADER_

#include "common_datatypes.h"

typedef enum {
	CURSOR_MOVE_UP = 0,
	CURSOR_MOVE_DOWN = 1,
	CURSOR_MOVE_LEFT = 2,
	CURSOR_MOVE_RIGHT = 3,
} cursor_move_dir_t;

extern	void TERM_pcw_init(void);
extern	void TERM_pcw_beep(void);

extern	void TERM_pcw_status_disable(void);
extern	void TERM_pcw_status_enable(void);
extern	void TERM_pcw_set_charset(char set);
extern	void TERM_pcw_cursor_move(cursor_move_dir_t dir);
extern	void TERM_pcw_clr(void);
extern	void TERM_pcw_home(void);
extern	void TERM_pcw_clr_home(void);
extern	void TERM_pcw_cursor_move_up_scroll(void);
extern	void TERM_pcw_erase_page_end(void);
extern	void TERM_pcw_erase_line_end(void);
extern	void TERM_pcw_erase_insert_line(void);
extern	void TERM_pcw_erase_delete_line(void);
extern	void TERM_pcw_erase_delete_char(void);
extern	void TERM_pcw_set_viewport(char top, char left,
	    char height, char width);
extern	void TERM_pcw_move_cursor(char x, char y);
extern	void TERM_pcw_viewport_clear_upto(void);
extern	void TERM_pcw_enable_cursor(void);
extern	void TERM_pcw_disable_cursor(void);
extern	void TERM_pcw_save_cursor_pos(void);
extern	void TERM_pcw_restore_cursor_pos(void);
extern	void TERM_pcw_clear_line(void);
extern	void TERM_pcw_clear_line_upto(void);
extern	void TERM_pcw_set_reverse_mode(char enable);
extern	void TERM_pcw_set_underline(char enable);
extern	void TERM_pcw_set_wrapping(char enable);
extern	void TERM_pcw_write_char(char ch);

#endif /* _PCW_TERM_HEADER_ */
