#include "../library/src/serial_cmd.h"

int main(void)
{
    system_init();
    while (1)
    {
        system_loop();
    }
    return 0;
    
}