#include "main.h"
#include <string.h>
#include <stdio.h>

bool Main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printf("usage: pour [options] commands\n");
            return false;
        } else {
            fprintf(stderr, "error: invalid command line argument \"%s\".\n", argv[i]);
            return false;
        }
    }

    return true;
}
