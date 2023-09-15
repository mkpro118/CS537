/**
 * Replicate a simplified version of linux's `groff` command.
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

#define WGROFF_FAILURE 1  /* Exit status for failure */
#define WGROFF_SUCCESS 0  /* Exit status for success */

#define DATE_STR_LENGTH 10       /* Length of a date string */
#define YEAR_STR_LENGTH 4        /* Length of a year string */
#define MONTH_STR_LENGTH 2       /* Length of a month string */
#define DAY_STR_LENGTH 2         /* Length of a day string */
#define MAX_FILENAME_LENGTH 100  /* Maximum length of the name of a file */
#define MAX_STR_LENGTH 256       /* Maximum length of a str or char[] */
#define MAX_COMMAND_LENGTH 20    /* Maximum length of a str or char[] */
#define OUTPUT_LENGTH 80         /* Maximum length of a str or char[] */

#define IF_FORMAT_FAILED(x) if (x < 0)  /* Descriptive to avoid mistakes */
#define IS_NULL(x) if (NULL == x)       /* Descriptive to avoid mistakes */
#define IS_NOT_NULL(x) if (NULL != x)   /* Descriptive to avoid mistakes */

#define _FALSE_ 0  /* Integer constant for False value */
#define _TRUE_ 1   /* Integer constant for True value */

#define TH_ARG_COUNT 5  /* Number of arguments in the first line */
#define SH_ARG_COUNT 1  /* Number of arguments in the section line */

#define NULL_TERMINATOR '\0'  /* String terminator character */
#define COMMENT '#'           /* Infile comment character */
#define SPACE ' '             /* Space character */

#define INFILE_CSI_TH ".TH"  /* Infile control sequence for title header */
#define INFILE_CSI_SH ".SH"  /* Infile control sequence for section header */

#define INFILE_CSI_BOLD "/fB"       /* Infile control sequence for bold */
#define INFILE_CSI_BOLD_L 3         /* Control sequence length for bold */
#define INFILE_CSI_ITALIC "/fI"     /* Infile control sequence for italic */
#define INFILE_CSI_ITALIC_L 3       /* Control sequence length for italic */
#define INFILE_CSI_UNDERLINE "/fU"  /* Infile control sequence for underline */
#define INFILE_CSI_UNDERLINE_L 3    /* Control sequence length for underline */
#define INFILE_CSI_NORMAL "/fP"     /* Infile control sequence for reset */
#define INFILE_CSI_NORMAL_L 3       /* Control sequence length for reset */
#define INFILE_CSI_SLASH "//"       /* Infile control sequence for slash */
#define INFILE_CSI_SLASH_L 2        /* Control sequence length for slash */

#define ANSI_CSI_BOLD "/033[1m"       /* ANSI control sequence for bold */
#define ANSI_CSI_ITALIC "/033[1m"     /* ANSI control sequence for italic */
#define ANSI_CSI_UNDERLINE "/033[1m"  /* ANSI control sequence for underline */
#define ANSI_CSI_NORMAL "/033[1m"     /* ANSI control sequence for reset */
#define ANSI_CSI_SLASH "/"            /* ANSI control sequence for slash */

#define INDENT "       "  /* 7 spaces for indentation */

////////////////////////////////////////////////////////////////////////////////
/////////////////////////        Format Strings        /////////////////////////

// If the program was invoked incorrectly
SCCP INVALID_USE = "Usage: ./wgroff <file_name>\n";

// If no arguments were provided
SCCP NO_ARG = "Improper number of arguments\nUsage: ./wgroff `<file>`\n";

// If string formatting failed
SCCP ERROR_IN_FORMAT = "Couldn't write to formatted string in func `%s`\n";

// If file isn't formatted correctly
SCCP INVALID_FORMAT = "Improper formatting on line `%i`\n";

// If fopen failed
SCCP ERROR_IN_FOPEN = "File `%s` was found, but could not be opened to read\n";

// If fopen failed
SCCP ERROR_IN_FOPEN_W = "File `%s` could not be opened to write\n";

// If file doesn't exists
SCCP FILE_NOT_FOUND = "File doesn't exist\n";

// Output file
SCCP OUT_FILE = "%s.%i";

// Title header format
SCCP TH_FORMAT = INFILE_CSI_TH " %s %i %s-%s-%s\n";

// Section header format
SCCP SH_FORMAT = INFILE_CSI_SH " %s\n";

// First line of output format
SCCP OUTFILE_FIRST_LINE = "%s(%i)%s%s(%i)\n";

// Last line of output format
SCCP OUTFILE_LAST_LINE = "%s(%i)%s%s-%s-%s%s%s(%i)\n";

// Section header in output
SCCP OUTFILE_SECTION_HEADER = "\n" ANSI_CSI_BOLD "%s" ANSI_CSI_NORMAL "\n";

/////////////////////////      End Format Strings      /////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/////////////////////////       Print Functions        /////////////////////////

// Alias hack to prevent [-Werror=format-security]
// Link: https://security.stackexchange.com/a/251757
int (*_PRINTF_)(const char*, ...) = printf;

/////////////////////////     End Print Functions      /////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/////////////////////////            WGROFF            /////////////////////////

/**
 * Recursively write format strings and line content to the outfile
 *
 * This function uses a recursive strategy to print the contents,
 * by moving the char pointer of start of the line forward by the length of the
 * infile format string.
 *
 * Given the constraints of the length of the infile (80 chars max),
 * this function should, not make more than 40 recursive calls,
 * worst case being a line of 80 '/' characters.
 *
 * @param outfile The file to write to
 * @param line    The text to write to the outfile
 * @param indent  Whether or not to indent the line
 */
void write_line(FILE* outfile, char* line, int indent) {
    char* pos;
    IS_NOT_NULL((pos = strstr(line, INFILE_CSI_BOLD))) {
        // Write any text before the occurence of format string
        *pos = NULL_TERMINATOR;  // Terminate string at format occurence
        fprintf(outfile, "%s", line);

        // Write ANSI Bold format string to file
        fprintf(outfile, ANSI_CSI_BOLD);

        // Move the char* up `INFILE_CSI_BOLD_L` spaces and continue printing
        write_line(outfile, line + INFILE_CSI_BOLD_L, _FALSE_);
    } else IS_NOT_NULL((pos = strstr(line, INFILE_CSI_ITALIC))) {
        // Write any text before the occurence of format string
        *pos = NULL_TERMINATOR;  // Terminate string at format occurence
        fprintf(outfile, "%s", line);

        // Write ANSI Italic format string to file
        fprintf(outfile, ANSI_CSI_ITALIC);

        // Move the char* up `INFILE_CSI_ITALIC_L` spaces and continue printing
        write_line(outfile, line + INFILE_CSI_ITALIC_L, _FALSE_);
    } else IS_NOT_NULL((pos = strstr(line, INFILE_CSI_UNDERLINE))) {
        // Write any text before the occurence of format string
        *pos = NULL_TERMINATOR;  // Terminate string at format occurence
        fprintf(outfile, "%s", line);

        // Write ANSI Underline format string to file
        fprintf(outfile, ANSI_CSI_UNDERLINE);

        // Move the char* up `INFILE_CSI_UNDERLINE_L` spaces and continue printing
        write_line(outfile, line + INFILE_CSI_UNDERLINE_L, _FALSE_);
    } else IS_NOT_NULL((pos = strstr(line, INFILE_CSI_NORMAL))) {
        // Write any text before the occurence of format string
        *pos = NULL_TERMINATOR;  // Terminate string at format occurence
        fprintf(outfile, "%s", line);

        // Write ANSI Normal format string to file
        fprintf(outfile, ANSI_CSI_NORMAL);

        // Move the char* up `INFILE_CSI_NORMAL_L` spaces and continue printing
        write_line(outfile, line + INFILE_CSI_NORMAL_L, _FALSE_);
    } else IS_NOT_NULL((pos = strstr(line, INFILE_CSI_SLASH))) {
        // Write any text before the occurence of format string
        *pos = NULL_TERMINATOR;  // Terminate string at format occurence
        fprintf(outfile, "%s", line);

        // Write ANSI Slash format string to file
        fprintf(outfile, ANSI_CSI_SLASH);

        // Move the char* up `INFILE_CSI_SLASH_L` spaces and continue printing
        write_line(outfile, line + INFILE_CSI_SLASH_L, _FALSE_);
    } else {
        // This is the base case

        // Indent text
        if (_TRUE_ == indent) {
            fprintf(outfile, INDENT);
        }

        // Print the line!
        fprintf(outfile, "%s", line);
    }
}


/**
 * Write the file line of the outfile
 * @param outfile The file to write to
 * @param command The name of command
 * @param section The section
 */
void write_first_line(FILE* outfile, char* command, int section) {
    int cmd_len = strlen(command);
    // for "(<section>)"
    cmd_len += 3;

    int n_spaces = OUTPUT_LENGTH - (2 * cmd_len);

    // +1 for String terminator
    char* spaces = malloc(sizeof(char) * (n_spaces + 1));

    for (int i = 0; i <= n_spaces; i++) {
        spaces[i] = SPACE;
    }
    spaces[n_spaces] = NULL_TERMINATOR;

    fprintf(outfile, OUTFILE_FIRST_LINE, command, section, spaces, command, section);
    free(spaces);
}

/**
 * Write the last line of the outfile
 * @param outfile The file to write to
 * @param command The name of the command
 * @param section The section
 * @param year    Year from the infile
 * @param month   Month from the infile
 * @param day     Day from the infile
 */
void write_last_line(FILE* outfile, char* command, int section, char* year,
                     char* month, char* day) {
    int cmd_len = strlen(command);
    // for "(<section>)"
    cmd_len += 3;

    // +2 for the '-' separator
    int date_len = strlen(year) + strlen(month) + strlen(day) + 2;

    int n_spaces = OUTPUT_LENGTH - (date_len + (2 * cmd_len));

    // Split the padding to use before and after the date
    n_spaces /= 2;

    // +1 for String terminator
    char* spaces = malloc(sizeof(char) * (n_spaces + 1));

    for (int i = 0; i <= n_spaces; i++) { spaces[i] = SPACE; }

    spaces[n_spaces] = NULL_TERMINATOR;

    fprintf(outfile, OUTFILE_LAST_LINE, command, section, spaces,
            year, month, day, spaces, command, section);
    free(spaces);
}

/**
 * Write a section header
 * @param outfile      The file to write to
 * @param section_name The section header
 */
void write_section_header(FILE* outfile, char* section_name) {
    fprintf(outfile, OUTFILE_SECTION_HEADER, section_name);
}

void parse_file(FILE* handle) {
    char buffer[MAX_STR_LENGTH];

    int line_count = 1;

    char command[MAX_STR_LENGTH];
    int section;
    char year[YEAR_STR_LENGTH];
    char month[MONTH_STR_LENGTH];
    char day[DAY_STR_LENGTH];

    // First line is handled separately
    IS_NULL(fgets(buffer, MAX_STR_LENGTH, handle)) {
        _PRINTF_(INVALID_FORMAT, line_count);

        // Invalid formatting is still a success
        exit(WGROFF_SUCCESS);
    }

    // Parse first line
    if (TH_ARG_COUNT != sscanf(buffer, TH_FORMAT, command, &section,
                               year, month, day)) {
        _PRINTF_(INVALID_FORMAT, line_count);

        // Invalid formatting is still a success
        exit(WGROFF_SUCCESS);
    }

    if (section < 1 || section > 9) {
        _PRINTF_(INVALID_FORMAT, line_count);

        // Invalid formatting is still a success
        exit(WGROFF_SUCCESS);
    }

    // Validate date
    if (strlen(year) != YEAR_STR_LENGTH || strlen(month) != MONTH_STR_LENGTH
            || strlen(day) != DAY_STR_LENGTH) {
        _PRINTF_(INVALID_FORMAT, line_count);
    }

    // +3 for extension and string terminator
    int filename_size = strlen(command) + 3;
    char* out_filename = malloc(sizeof(char) * filename_size);

    IF_FORMAT_FAILED(sprintf(out_filename, OUT_FILE, command, section)) {
        fprintf(stderr, ERROR_IN_FORMAT, "parse_file");
        exit(WGROFF_FAILURE);
    }

    FILE* outfile = fopen(out_filename, "w");

    free(out_filename);

    IS_NULL(outfile) {
        fprintf(stderr, ERROR_IN_FOPEN_W, out_filename);
        exit(WGROFF_FAILURE);
    }

    write_first_line(outfile, command, section);

    while (NULL != fgets(buffer, MAX_STR_LENGTH, handle)) {
        line_count++;
        if (strstr(buffer, INFILE_CSI_SH)) {
            char section_name[MAX_STR_LENGTH];

            // Parse first line
            if (TH_ARG_COUNT != sscanf(buffer, SH_FORMAT, section_name)) {
                _PRINTF_(INVALID_FORMAT, line_count);

                // Invalid formatting is still a success
                exit(WGROFF_SUCCESS);
            }

            write_section_header(outfile, section_name);
        } else {
            write_line(outfile, buffer, _TRUE_);
        }
    }

    write_last_line(outfile, command, section, year, month, day);
}

void run_wgroff(char* filename) {
    // Does the file exist?
    if (access(filename, F_OK)) {
        _PRINTF_(FILE_NOT_FOUND);
        // 404 is still a success
        exit(WGROFF_SUCCESS);
    }

    FILE* handle = NULL;

    // Can we open the file?
    IS_NULL((handle = fopen(filename, "r"))) {
        _PRINTF_(ERROR_IN_FOPEN, filename);
        exit(WGROFF_FAILURE);
    }

    parse_file(handle);
}

/////////////////////////          End WGROFF          /////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/////////////////////////             MAIN             /////////////////////////

int main(int argc, char *argv[]) {
    char* filename = NULL;

    switch (argc - 1) {
        case 0: // Usage was: ./wgroff
            _PRINTF_(NO_ARG);
            break;
        case 1: // Usage was: ./wgroff <file_name>
            filename = argv[1];

            run_wgroff(filename);
            break;
        default: // Usage was: ./wgroff <arg1> <arg2> ... <argc-1>
            _PRINTF_(INVALID_USE);
            break;
    }

    return WGROFF_SUCCESS;
}

/////////////////////////           END MAIN           /////////////////////////
////////////////////////////////////////////////////////////////////////////////
