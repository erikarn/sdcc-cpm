;--------------------------------------------------------------------------
;  cpm0.s - Generic cpm0.s for a Z80 CP/M-80 v2.2 Application
;  Copyright (C) 2011, Douglas Goodall All Rights Reserved.
;--------------------------------------------------------------------------

	.globl	_main
	.globl	_zzz_progend
	.area	_CODE

	.ds	0x0100
init:
	;; Define an adequate stack
	;;
	;; XXX TODO: load the TPA into a variable here
	;; and use /that/ as the stack; then set /our/ usable
	;; TPA top to be 256 bytes below it.
	ld	sp,#stktop

	;; Load BDOS address; subtract one, that's our TPA end.
	ld	hl, (0x0006)
	dec hl
	ld	(#_free_tpa_end), hl

	;; and TPA start - later on I may move the stack here
	;; and subtract.
	ld	hl, #_zzz_progend
	ld	(#_free_tpa_start), hl

	;; Initialise global variables
	call    gsinit

	;; Call the C main routine
	call	_main

	ld	c,#0
	call	5

	;; Ordering of segments for the linker.
	.area	_TPA

	;; Pre-declare all the areas that we (hopefully) will
	;; link.  Verify that in the generated program .noi file
	;; that _ZZZ and zzz_progend are the end of the used
	;; memory range.
	.area	_HOME
	.area	_CODE
	.area	_INITIALIZER
	.area	_GSINIT
	.area	_GSFINAL
	.area	_DATA
	.area	_INITIALIZED
	.area	_BSEG
	.area	_BSS
	.area	_HEAP
	.area	_HEAP_END
	.area	_STACK
	;; Must be the final area so we can get the bounds of
	;; our segments.
	.area	_ZZZ

	;; Declare a 256 byte stack above the default heap.
	.area	_STACK
	.ds	256
stktop:

	;; Declare the start of gsinit::, other functions will
	;; be added to this as CALL instructions to get called
	;; during setup.
        .area   _GSINIT
gsinit::

        .area   _GSFINAL
        ret
	.db	0xe5

	.area	_DATA

;; Beginning of free TPA, after the _ZZZ progend label.

_free_tpa_start::
	.dw	0x00

;; End of TPA, before our stack, any CP/M extensions, BDOS, etc.
_free_tpa_end::
	.dw	0x00

;;;;;;;;;;;;;;;;
; eof - cpm0.s ;
;;;;;;;;;;;;;;;;

