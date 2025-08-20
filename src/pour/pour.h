#ifndef POUR_POUR_H
#define POUR_POUR_H

#include <common/common.h>

bool Pour_Run(const char* package, int argc, char** argv);
bool Pour_Install(const char* package);

bool Pour_Main(int argc, char** argv);

#endif
