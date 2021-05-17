global _init
section .init
bits 64
_init:
	push rbp
	mov rsp, rbp
	; gcc will nicely put the contents of crtbegin.o's .init section here.

global _fini
section .fini
_fini:
	push rbp
	mov rsp, rbp
	; gcc will nicely put the contents of crtbegin.o's .fini section here.