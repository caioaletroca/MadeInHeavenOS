# Formatting into a buffer: kernel (kprintf) and user programs
local_sources := \
vsnprintf.c

# Streams over file descriptors: user programs only
hosted_local_sources := \
FILE.c \
fputc.c \
fwrite.c \
vfprintf.c \
vprintf.c
