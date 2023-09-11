/**
 * Replicate a simplified version of linux's `apropos` command.
 *
 * @author Mrigank Kumar
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WAPROPOS_SUCCESS 0
#define WAPROPOS_FAILURE 1

#define SCCP static const char*


SCCP NO_ARG = "wapropos what?\n";

SCCP INVALID_USE = "Usage: ./wapropos <keyword>\n";

// Alias hack to prevent [-Werror=format-security]
// Link: https://security.stackexchange.com/a/251757
int (*_PRINTF_)(const char*, ...) = printf;


////////////////////////////////////////////////////////////////////////////////
/////////////////////////             MAIN             /////////////////////////

int main(int argc, char* argv[]) {
    FILE* handle;
    char* keyword = NULL;
    switch (argc - 1) {
        case 0:
            _PRINTF_(NO_ARG);
            return WAPROPOS_SUCCESS;
        case 1:
            keyword = argv[1];
            handle = search_keyword(keyword);
            break;
        default:
            _PRINTF_(INVALID_USE);
            return WAPROPOS_SUCCESS;
    }

    print_apropos(handle);

    return WAPROPOS_FAILURE;
}

/////////////////////////           END MAIN           /////////////////////////
////////////////////////////////////////////////////////////////////////////////
