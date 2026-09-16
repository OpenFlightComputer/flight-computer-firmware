#include "application_runtime.h"
#include "board.h"

int main(void)
{
    board_enter_usb_bootloader_if_requested();
    application_runtime_initialize();
    application_runtime_run();
    return 0;
}
