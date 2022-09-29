.globl _init
.section .init
.code64
_init:
	push %rbp
	movq %rsp, %rbp
	# gcc will nicely put the contents of crtbegin.o's .init section here.

.globl _fini
.section .fini
_fini:
	push %rbp
	movq %rsp, %rbp
	# gcc will nicely put the contents of crtbegin.o's .fini section here.