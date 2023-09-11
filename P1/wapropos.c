/**
 * Replicate a simplified version of linux's `apropos` command.
 *
 * @author Mrigank Kumar
 */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WAPROPOS_FAILURE 1  /* Exit status for failure */
#define WAPROPOS_SUCCESS 0  /* Exit status for success */

#define MIN_SECTION 1  /* Minimum section value */
#define MAX_SECTION 9  /* Maximum section value */

#define SCCP static const char*  /* To reduce line length */
/* (SCCP = Static Const Char Pointer) */

#define MAX_STR_LENGTH 256  /* Maximum length of a str or char[] */
#define MAX_FILENAME_LENGTH 100  /* Maximum length of the name of a file */

#define IS_NULL(x) if (NULL == x)      /* Descriptive to avoid mistakes */
#define IS_NOT_NULL(x) if (NULL != x)  /* Descriptive to avoid mistakes */
#define IF_FORMAT_FAILED(x) if (x < 0)    /* Descriptive to avoid mistakes */

#define _TRUE_ 1
#define _FALSE_ 0

#define DESCRIPTION "DESCRIPTION\n"
#define NAME "NAME\n"
#define FILE_EXTENSION_SEPARATOR '.'
#define NULL_TERMINATOR '\0'


////////////////////////////////////////////////////////////////////////////////
/////////////////////////        Format Strings        /////////////////////////

// If the program was invoked incorrectly
SCCP INVALID_USE = "Usage: ./wapropos <keyword>\n";

// If no arguments were provided
SCCP NO_ARG = "wapropos what?\n";

// If the page was not found in all man_pages sub-directories
SCCP KEYWORD_NOT_FOUND = "nothing appropriate\n";

// If string formatting failed
SCCP ERROR_IN_FORMAT = "Couldn't write to formatted string in func `%s`\n";

// If fopen failed
SCCP ERROR_IN_FOPEN = "File `%s` was found, but could not be opened to read\n";

// If opendir failed
SCCP ERROR_IN_OPENDIR = "Error in opening directory `%s`\n";

// Output wapropos
SCCP WAPROPOS_OUTPUT = "%s (%i) - %s\n";

/* Cross platform compatibility (not required, but I don't think it hurts)*/
/* This was inspired by multiple answers on StackOverflow */
/* Link: https://stackoverflow.com/q/12971499/10812282 */
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
SCCP BASE_FILEPATH = ".\\man_pages\\man%i\\";
#else
SCCP BASE_FILEPATH = "./man_pages/man%i/";
#endif

/////////////////////////      End Format Strings      /////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/////////////////////////       Print Functions        /////////////////////////

// Alias hack to prevent [-Werror=format-security]
// Link: https://security.stackexchange.com/a/251757
int (*_PRINTF_)(const char*, ...) = printf;

void print_apropos(FILE* handle, char* filename, char* name, int section) {
    int i = 0;
    for (; filename[i] != FILE_EXTENSION_SEPARATOR; i++);

    filename[i] = NULL_TERMINATOR;

    _PRINTF_(WAPROPOS_OUTPUT, filename, section, name);
}

/////////////////////////     End Print Functions      /////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/////////////////////////        Search Function       /////////////////////////

static inline void join_file_to_base(char* str, char* base, char* filename) {
    IF_FORMAT_FAILED(sprintf(str, "%s%s", base, filename)) {
        fprintf(stderr, ERROR_IN_FORMAT, "join_file_to_base");
        exit(WAPROPOS_FAILURE);
    }
}

char* contains_keyword(FILE* handle, char* keyword) {
    char buffer[MAX_STR_LENGTH];
    int to_check = _FALSE_;

    char* name = NULL;

    while (NULL != fgets(buffer, MAX_STR_LENGTH, handle)) {
        // Names are supposed to be one liners, so check them immediately
        if (strcmp(buffer, NAME) == 0) {
            name = malloc(sizeof(char) * MAX_STR_LENGTH);

            IS_NULL(name) {
                fprintf(stderr, "malloc failed in function `contains_keyword`");
                exit(WAPROPOS_FAILURE);
            }

            if(NULL != fgets(name, MAX_STR_LENGTH, handle)) {
                fprintf(stderr, "%s\n", "Name section not found. Aborting");
                exit(WAPROPOS_FAILURE);
            }

            if (strstr(buffer, keyword) != NULL) { return name; }
        }
        // Descriptions are longer, check them in an different (outer) loop.
        else if (strcmp(buffer, DESCRIPTION)) {
            to_check = _TRUE_;
            break;
        }
    }

    // If we never found a description section and didn't return early.
    if (_FALSE_ == to_check) { return NULL; }

    // Only checking the description section now
    // If there's a section after description, this code will check that too,
    // so a potential bug here.
    while (NULL != fgets(buffer, MAX_STR_LENGTH, handle)) {
        if (strstr(buffer, keyword)) { return name; }
    }

    return NULL;
}

int search_keyword(char* keyword) {
    // For traversing directories
    DIR* dir;
    struct dirent* entry;

    // For file operations
    FILE* handle;

    // Count number of instances found
    int count = 0;

    // Look through every man_pages section, i.e man`i` for i in [1, 9]
    for (int section = MIN_SECTION; section <= MAX_SECTION; section++) {
        // Store the name of the file
        char filename[MAX_FILENAME_LENGTH];

        // Store the name of the base directory
        char base[MAX_FILENAME_LENGTH];

        // Couldn't get a base_path from sprintf
        IF_FORMAT_FAILED(sprintf(base, BASE_FILEPATH, section)) {
            fprintf(stderr, ERROR_IN_FORMAT, "search_keyword");
            exit(WAPROPOS_FAILURE);
        }

        // Open the base directory
        dir = opendir(base);

        // NULL check
        IS_NULL(dir) {
            fprintf(stderr, ERROR_IN_OPENDIR, base);
            exit(WAPROPOS_FAILURE);
        }

        // While there are entries in this directory,
        while (NULL != (entry = readdir(dir))) {
            // base_dir + filename
            printf("entry->d_name = %s", entry->d_name);
            join_file_to_base(filename, base, entry->d_name);
            printf("filename = %s", filename);

            // If we don't have read access, we can't read!
            if (access(filename, R_OK)) continue;

            // Get a file handle
            handle = fopen(filename, "r");

            // NULL check
            IS_NULL(handle) {
                fprintf(stderr, ERROR_IN_FOPEN, filename);
                exit(WAPROPOS_FAILURE);
            }

            // If the file contains the keyword, this var will contain
            // the `name_one_liner`, the one-line content of the NAME portion
            char* name = contains_keyword(handle, keyword);

            IS_NOT_NULL(name) {
                print_apropos(handle, filename, name, section);
                count++;
            }

            // Close the file handle
            fclose(handle);
        }

        // Close the directory
        closedir(dir);
    }

    return count;
}

/////////////////////////      End Search Function     /////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/////////////////////////             MAIN             /////////////////////////

int main(int argc, char* argv[]) {
    char* keyword = NULL;
    switch (argc - 1) {
        case 0:
            _PRINTF_(NO_ARG);
            break;
        case 1:
            keyword = argv[1];

            if(0 == search_keyword(keyword)) {
                _PRINTF_(KEYWORD_NOT_FOUND);
            }

            break;
        default:
            _PRINTF_(INVALID_USE);
            break;
    }
    return WAPROPOS_SUCCESS;
}

/////////////////////////           END MAIN           /////////////////////////
////////////////////////////////////////////////////////////////////////////////
