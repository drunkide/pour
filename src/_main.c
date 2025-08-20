#include <pour/main.h>
#include <common/script.h>

int main(int argc, char** argv)
{
    return Script_RunVM(argc, argv, Pour_Main);
}
