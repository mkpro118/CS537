/**
 * Replicate a simplified version of linux's `man` command.
 *
 * @author Mrigank Kumar
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WMAN_FAILURE 1  /* Exit status for failure */
#define WMAN_SUCCESS 0  /* Exit status for success */

#define MIN_SECTION 1  /* Minimum section value */
#define MAX_SECTION 9  /* Maximum section value */

#define SCCP static const char*  /* To reduce line length */
/* (SCCP = Static Const Char Pointer) */

#define MAX_STR_LENGTH 256  /* Maximum length of a str or char[] */

#define IS_NULL(x) if (NULL == x)      /* Descriptive to avoid mistakes */
#define IS_NOT_NULL(x) if (NULL != x)  /* Descriptive to avoid mistakes */
#define IF_FORMAT_FAILED(x) if (x < 0)    /* Descriptive to avoid mistakes */

////////////////////////////////////////////////////////////////////////////////
/////////////////////////        Format Strings        /////////////////////////

// If the section input is invalid
SCCP INVALID_SECTION = "invalid section\n";

// If the program was invoked incorrectly
SCCP INVALID_USE = "Usage: ./wman <page>  or  ./wman <section> <page>\n";

// If no arguments were provided
SCCP NO_ARG = "What manual page do you want?\nFor example, try 'wman wman'\n";

// If the page was not found in all man_pages sub-directories
SCCP PAGE_NOT_FOUND = "No manual entry for %s\n";

// If the page was not found in a given section
SCCP PAGE_NOT_FOUND_IN_SECTION = "No manual entry for %s in section %i\n";

// If string formatting failed
SCCP ERROR_IN_FORMAT = "Couldn't write to formatted string in func `%s`\n";

// If fopen failed
SCCP ERROR_IN_FOPEN = "File `%s` was found, but could not be opened to read\n";

/* Cross platform compatibility (not required, but I don't think it hurts)*/
/* This was inspired by multiple answers on StackOverflow */
/* Link: https://stackoverflow.com/q/12971499/10812282 */
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
SCCP FILEPATH = ".\\man_pages\\man%i\\%s.%i";
#else
SCCP FILEPATH = "./man_pages/man%i/%s.%i";
#endif

/////////////////////////      End Format Strings      /////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/////////////////////////   String Format Functions    /////////////////////////

// Alias hack to prevent [-Werror=format-security]
// Link: https://security.stackexchange.com/a/251757
int (*_PRINTF_)(const char*, ...) = printf;

/**
 * Write a formatted error message to `str` when the page is not found
 * in any of the man pages.
 *
 * In case of failure to format the string, exits the program with status
 * WMAN_FAILURE
 *
 * This function is inlined as it's only called once. Separated for readability.
 *
 * @param str  Where to store the formatted error message
 * @param page The page that was not found
 */
static inline void page_not_found_msg(char* str, const char* page) {
    IF_FORMAT_FAILED(sprintf(str, PAGE_NOT_FOUND, page)) {
        fprintf(stderr, ERROR_IN_FORMAT, "page_not_found_msg");
        exit(WMAN_FAILURE);
    }
}

/**
 * Write a formatted error message to `str` when the page is not found
 * in any the given section
 *
 * In case of failure to format the string, exits the program with status
 * WMAN_FAILURE
 *
 * This function is inlined as it's only called once. Separated for readability.
 *
 * @param str     Where to store the formatted error message
 * @param page    The page that was not found
 * @param section The section where the page was not found
 */
static inline void page_not_found_in_section_msg(char* str, const char* page,
                                          int section) {
    IF_FORMAT_FAILED(sprintf(str, PAGE_NOT_FOUND_IN_SECTION, page, section)) {
        fprintf(stderr, ERROR_IN_FORMAT, "page_not_found_in_section_msg");
        exit(WMAN_FAILURE);
    }
}

///////////////////////// End String Format Functions  /////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/////////////////////////       Search Functions       /////////////////////////


static inline void build_filepath(char* str, char* page, int section) {
    IF_FORMAT_FAILED(sprintf(str, FILEPATH, section, page, section)) {
        fprintf(stderr, ERROR_IN_FORMAT, "build_filepath");
        exit(WMAN_FAILURE);
    }
}

/**
 * Search through the sub folder specified by `section`
 * to find a manual entry for `page`
 *
 * @param  page The page to find a manual entry for
 * @return      A handle to the file if the manual is found. NULL otherwise
 */
FILE* search_page_in_section(char* page, int section) {
    char filename[MAX_STR_LENGTH];
    build_filepath(filename, page, section);

    if (access(filename, F_OK | R_OK)) { return NULL; }

    FILE* handle;

    IS_NULL((handle = fopen(filename, "r"))) {
        fprintf(stderr, ERROR_IN_FOPEN, filename);

        exit(WMAN_FAILURE);
    }

    return handle;
}

/**
 * Search through all sub folders to find a manual entry for `page`
 *
 * @param  page The page to find a manual entry for
 * @return      A handle to the file if the manual is found. NULL otherwise
 */
FILE* search_all_pages(char* page) {
    FILE* handle = NULL;

    for (int section = MIN_SECTION; section <= MAX_SECTION; section++) {
        handle = search_page_in_section(page, section);

        IS_NOT_NULL(handle) {
            return handle;
        }
    }

    return handle;
}

/////////////////////////     End Search Functions     /////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/////////////////////////          Print File          /////////////////////////

void print_file(FILE* handle) {
    char buffer[MAX_STR_LENGTH];

    while (NULL != fgets(buffer, MAX_STR_LENGTH, handle)) {
        printf("%s", buffer);
    }
}

/////////////////////////        End Print File        /////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/////////////////////////             MAIN             /////////////////////////

int main(int argc, char* argv[])
{
    // Define variables, initialize with invalid values to avoid flags
    // This allows a single check for input validity and result
    FILE* handle = NULL;  // Explicit null to avoid compiler warnings
    char* page = NULL;  // Same as above.
    int section = -1;

    // Arg parse
    switch (argc - 1) {
        case 0:  // Usage was: prompt> wman
            _PRINTF_(NO_ARG);
            return WMAN_SUCCESS;

        case 1:  // Usage was: prompt> wman <page>

            // Re-initialize local variable
            // No checks are performed to verify contents of `page`
            page = argv[1];

            // Re-initialize local variable with a file handle
            handle = search_all_pages(page);

            break;

        case 2: // Usage was: prompt> wman <section> <page>

            // Additional check for invalid section value
            // Link https://piazza.com/class/llnp9g1fwu33yx/post/30
            if (strlen(argv[1]) != 1) {
                _PRINTF_(INVALID_SECTION);
                return WMAN_FAILURE;
            }

            // stdlib.h -> atoi : convert str to int
            // This will overwrite the value of the local variable section
            section = atoi(argv[1]);


            // Range check, if invalid, print error and return
            if (section < MIN_SECTION || section > MAX_SECTION) {
                _PRINTF_(INVALID_SECTION);
                return WMAN_FAILURE;  // This is a failure as specified
            }

            // At this point, `section` is a valid section [1-9]
            // `section` itself acts as a flag for
            // whether or not section was specified

            // Re-initiliaze local variable
            // No checks are performed to verify contents of `page`
            page = argv[2];

            // Re-initialize local variable with a file handle
            handle = search_page_in_section(page, section);

            break;
        default: // Usage was: prompt> wman <arg1> <arg2> <arg3> ... <argc>

            // Print invalid usage message
            _PRINTF_(INVALID_USE);

            // Unsure whether this should be a failure or success
            return WMAN_SUCCESS;
    }

    // Check for file returned by `case 1` or `case 2`, other cases return early
    IS_NULL(handle) {
        // If the local variable is still `NULL`, no file with the given
        // specifications were found

        // Error messages are assumed to be < `MAX_STR_LENGTH` long
        char msg[MAX_STR_LENGTH];

        // If `section` is still `-1`, the section was NOT specified
        if (-1 == section) {
            // Populate local var `msg` with the formatted message
            page_not_found_msg(msg, page);
        }

        // Section was specified
        else {
            // Populate local var `msg` with the formatted message
            page_not_found_in_section_msg(msg, page, section);
        }

        // Print error to stdout
        _PRINTF_(msg);

        return WMAN_SUCCESS;  // No entry found is still a success
    }

    // A manual entry was found
    // The handle to the file is present in `handle`

    // Print the file's contents.
    print_file(handle);

    // Close the file handle
    fclose(handle);

    return WMAN_SUCCESS;  // Program succeeded
}

/////////////////////////           END MAIN           /////////////////////////
////////////////////////////////////////////////////////////////////////////////
