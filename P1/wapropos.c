/**
 * Replicate a simplified version of linux's `apropos` command.
 *
 * @author Mrigank Kumar
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SCCP static const char*  /* To reduce line length */
/* (SCCP = Static Const Char Pointer) */

#define WAPROPOS_FAILURE 1  /* Exit status for failure */
#define WAPROPOS_SUCCESS 0  /* Exit status for success */

#define MIN_SECTION 1  /* Minimum section value */
#define MAX_SECTION 9  /* Maximum section value */

#define MAX_FILENAME_LENGTH 100  /* Maximum length of the name of a file */
#define MAX_STR_LENGTH 256       /* Maximum length of a str or char[] */

#define IS_NULL(x) if (NULL == x)       /* Descriptive to avoid mistakes */
#define IS_NOT_NULL(x) if (NULL != x)   /* Descriptive to avoid mistakes */
#define IF_FORMAT_FAILED(x) if (x < 0)  /* Descriptive to avoid mistakes */

#define _FALSE_ 0  /* Integer constant for False value */
#define _TRUE_ 1   /* Integer constant for True value */

#define DESCRIPTION "DESCRIPTION"  /* To find the description section */
#define NAME "NAME"                /* To find the name section */

#define FILE_EXTENSION_SEPARATOR '.'  /* The file extension character */
#define NULL_TERMINATOR '\0'          /* String terminator character */
#define SPACE ' '                     /* The space character */

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
SCCP WAPROPOS_OUTPUT = "%s (%i) - %s";

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

/**
 * Prints the wapropos output to stdout
 *
 * NOTE:
 * This function is super hacky, and the technique is explained below
 * Hopefully it's understandable. Further explanation is inline.
 *
 * The idea is, the name section has the following format
 * "<spaces> <filename_without_extension> - <name_one_liner>"
 *
 * So we extract the name of the file by finding
 * the first non space character,
 * then finding the '-' character, and splitting the line at those
 * points by manually introducing string terminators. This allows
 * printf to stop at the spltting points and print in the correct format.
 *
 *
 * @param name    The name section from the file, contains the filename
 *                and the name_one_liner
 * @param section The section in which the keyword was found in
 */
void print_apropos(char* name, int section) {
    int i = 0; // `i` holds the address of the first non space character
    int j; // `j` holds the address of the start of the name_one_liner

    for (; name[i] == ' '; i++); // loop over spaces, stop at non spaces

    for (j = i; name[j] != '-'; j++); // for instead to initialize

    name[j - 1] = NULL_TERMINATOR; // split the string inplace

    // What the above parts do is,
    // let's say we have the name section as
    // "      example - an example program\n\0"
    //
    // The above code turns that into
    // "      example\0- an example program\n\0"
    //        ^        ^
    //        i        j

    // name + i => start of the filename
    // name + j + 2 => start of the one-liner
    _PRINTF_(WAPROPOS_OUTPUT, name + i, section, name + j + 2);
}

/////////////////////////     End Print Functions      /////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/////////////////////////        Search Function       /////////////////////////

/**
 * Joins the filename to the base directory
 *
 * @param str      To store the output
 * @param base     The base directory
 * @param filename The filename
 */
static inline void join_file_to_base(char* str, char* base, char* filename) {
    IF_FORMAT_FAILED(sprintf(str, "%s%s", base, filename)) {
        fprintf(stderr, ERROR_IN_FORMAT, "join_file_to_base");
        exit(WAPROPOS_FAILURE);
    }
}

/**
 * Checks whether the given file contains the keyword
 *
 * If it does, this function returns the one line name section
 *
 * @param  handle  The file to search for the keyword in
 * @param  keyword The keyword to search for
 * @return         The heap allocated char array containing the
 *                 name one-liner if the keyword was found.
 *                 NULL otherwise.
 */
char* contains_keyword(FILE* handle, char* keyword) {
    char buffer[MAX_STR_LENGTH];
    int to_check = _FALSE_;

    char* name = NULL;

    while (NULL != fgets(buffer, MAX_STR_LENGTH, handle)) {
        // The section headings do not have leading spaces
        if (buffer[0] == SPACE) { continue; }
        // Names are supposed to be one liners, so check them immediately
        if (strstr(buffer, NAME) != NULL) {
            name = malloc(sizeof(char) * MAX_STR_LENGTH);

            // malloc success check
            IS_NULL(name) {
                fprintf(stderr, "malloc failed in function `contains_keyword`");
                free(name);
                exit(WAPROPOS_FAILURE);
            }

            // The very next line after NAME should be the one-liner
            if(NULL == fgets(name, MAX_STR_LENGTH, handle)) {
                // THIS SHOULD IDEALLY NEVER EXECUTE
                fprintf(stderr, "%s\n", "Name section not found. Aborting");
                free(name);
                exit(WAPROPOS_FAILURE);
            }

            // If the NAME section contains the keyword,
            // skip checking the DESCRIPTION
            if (strstr(name, keyword) != NULL) { return name; }
        }
        // Descriptions are longer, check them in an different (outer) loop.
        else if (strstr(buffer, "DESCRIPTION") != NULL) {
            to_check = _TRUE_;
            break;
        }
    }

    // If we never found a description section,
    // and didn't return early from the NAME section
    if (_FALSE_ == to_check) { return NULL; }

    // Only checking the description section now
    // If there's a section after description, this code will check that too,
    // so a potential bug here.
    while (NULL != fgets(buffer, MAX_STR_LENGTH, handle)) {
        if (buffer[0] != SPACE) { break; }
        if (strstr(buffer, keyword)) { return name; }
    }

    // If `keyword` wasn't found, return NULL
    return NULL;
}

/**
 * Looks for the given keyword across all manual pages
 * Returns the number of files found containing the given keyword
 *
 * @param  keyword The keyword to search for
 * @return         The number of files containing the given keyword.
 */
int search_keyword(char* keyword) {
    // For traversing directories
    DIR* dir;

    // For directory entries
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

        // NULL check, ensure opendir worked
        IS_NULL(dir) {
            fprintf(stderr, ERROR_IN_OPENDIR, base);
            exit(WAPROPOS_FAILURE);
        }

        // While there are entries in this directory,
        while (NULL != (entry = readdir(dir))) {
            // base_dir + filename

            // Ignore hidden files, cwd and parent
            if (entry->d_name[0] == '.') continue;

            // Join the file to the base path
            join_file_to_base(filename, base, entry->d_name);

            // If we don't have read access, we can't read!
            if (access(filename, R_OK)) continue;

            // Get a file handle
            handle = fopen(filename, "r");

            // NULL check, esnure fopen worked
            IS_NULL(handle) {
                fprintf(stderr, ERROR_IN_FOPEN, filename);
                exit(WAPROPOS_FAILURE);
            }

            // If the file contains the keyword, this var will contain
            // the `name_one_liner`, the one-line content of the NAME portion
            char* name = contains_keyword(handle, keyword);

            // NULL means keyword was not found in this file
            IS_NOT_NULL(name) {
                // print in wapropos format
                print_apropos(name, section);

                // free memory allocated by `contains_keyword`
                free(name);
                count++; // increment number of files found with `keyword`
            }

            // Close the file handle
            fclose(handle);
        }

        // Close the directory
        closedir(dir);
    }

    // Self explanatory
    return count;
}

/////////////////////////      End Search Function     /////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/////////////////////////             MAIN             /////////////////////////

int main(int argc, char* argv[]) {
    // Arg parse
    switch (argc - 1) {
        case 0: // Usage was : ./wapropos
            _PRINTF_(NO_ARG);
            break;
        case 1: // Usage was: ./wapropos <keyword>
            // If no file contain the specified keyword
            if(0 == search_keyword(argv[1])) { _PRINTF_(KEYWORD_NOT_FOUND); }

            break;
        default: // Usage was: ./wapropos <arg1> <arg2> ... <argc-1>
            _PRINTF_(INVALID_USE);
            break;
    }

    // Program succeeded
    return WAPROPOS_SUCCESS;
}

/////////////////////////           END MAIN           /////////////////////////
////////////////////////////////////////////////////////////////////////////////
