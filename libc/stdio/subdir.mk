# Formatting into a buffer: kernel (kprintf) and user programs
local_sources := \
snprintf.c \
vsnprintf.c

# Streams over file descriptors: user programs only
hosted_local_sources := \
FILE.c \
__stdio_exit.c \
__write_all.c \
__fwritex.c \
__fflush_one.c \
__fillbuf.c \
fflush.c \
ferror.c \
feof.c \
clearerr.c \
fgetc.c \
getc.c \
ungetc.c \
getchar.c \
fgets.c \
fputc.c \
fputs.c \
puts.c \
putchar.c \
fwrite.c \
vfprintf.c \
vprintf.c \
printf.c \
fprintf.c