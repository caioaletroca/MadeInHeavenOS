.globl _init
.section .init
.code64
.type _init, @function
_init:
	push %rbp
	movq %rsp, %rbp
	# gcc will nicely put the contents of crtbegin.o's .init section here.
.size _init, . - _init

.globl _fini
.section .fini
.type _fini, @function
_fini:
	push %rbp
	movq %rsp, %rbp
	# gcc will nicely put the contents of crtbegin.o's .fini section here.
.size _fini, . - _fini