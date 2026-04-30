#include "miscible.h"
#include "os/os_inc.h"

// #if OS_WINDOWS
// int WinMain(
//     HINSTANCE hInstance,
//     HINSTANCE hPrevInstance,
//     LPSTR argv,
//     int argc)
// #elif OS_LINUX
int main(int argc, char **argv)
// #endif
{
    return mscbl_start(argc, argv);
}
