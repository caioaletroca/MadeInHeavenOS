#include <stdio.h>
#include <stdlib.h>

// Called if a pure virtual function runs: only possible from a constructor or
// destructor of the abstract class itself. A bug: report it and stop.
extern "C" void __cxa_pure_virtual()
{
    fprintf(stderr, "pure virtual function called\n");
    abort();
}