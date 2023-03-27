
#define	platform_sys_init()		cpm_sysfunc_init()
#define	platform_console_read_char()	cpm_rawio_read_wait()
#define	platform_console_write_char(a)	cpm_putchar(a)
