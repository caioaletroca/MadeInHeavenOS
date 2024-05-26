#ifndef _FILE_H_
#define _FILE_H_

struct _FILE {
    int buffer_size;     // Buffer size
    char *buffer;       // Buffer pointer
    char *pointer;      // Next Character position pointer
};

#endif