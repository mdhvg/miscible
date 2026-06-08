#include "miscible.h"
#include "os/os_inc.h"

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    return mscbl_start(nShowCmd, NULL);
}
