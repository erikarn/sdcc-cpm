
;; TODO: which segment should I be attaching this after, so I can capture
;; the /actual/ end of the binary?

;; Declaring these seems to make _ZZZ show up at the end?

	.area	_HEAP
	.area	_HEAP_END

	.area   _ZZZ
_zzz_progend::
	.ds	1

