#include "application_runtime.h"

#include <stddef.h>

int main(void)
{
    return application_runtime_run_until_idle(1U, NULL) ==
                   APPLICATION_RUNTIME_DRAIN_INVALID_ARGUMENT
               ? 0
               : 1;
}
