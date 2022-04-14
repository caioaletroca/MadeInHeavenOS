#include <stdlib.h>

int atoi(const char *str) {
    int i;
    int sign = 1;
    int num = 0;

    // Discart white spaces
    for(i = 0; isspace(str[i]); i++);

    // Check for sign in the string
    if(str[i] == '+')
        i++;
    else if (str[i] == '-') {
        sign = -1;
        i++;
    }

    // Run over only the digits characters
    for(; isdigit(str[i]); i++) {
        num = (num << 3) + (num << 1);  // num *= 10
        num += (str[i] - '0');
    }

    if(sign == -1)
        num = -num;

    return num;
}