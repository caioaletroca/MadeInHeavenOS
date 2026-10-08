# Formatting into a buffer: kernel (kprintf) and user programs
local_sources := \
vsnprintf.c

# Streams over file descriptors: user programs only
hosted_local_sources := \
FILE.c \
__stdio_exit.c \
__write_all.c \
__fwritex.c \
__fflush_one.c \
fflush.c \
ferror.c \
fputc.c \
fputs.c \
puts.c \
putchar.c \
fwrite.c \
vfprintf.c \
vprintf.c \
printf.c \
fprintf.c