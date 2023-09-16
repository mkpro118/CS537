# Project 1

- Name: Mrigank Kumar
- CS Login: mrigank
- WISC ID: 9083537424
- Email: mkumar42@wisc.edu

## Note
An alias is used for `printf` in all programs, defined as
```c
int (*_PRINTF_)(const char*, ...) = printf;
```

This is used to avoid `[-Werror=format-security]`, since the `-Werror` and `-Wall` flags are used, which disallow using variables as the format string for `printf`. Security is still maintained in all programs, as the format strings defined are not malicious, and have been carefully designed as `static const char*`, which prevents modification of the format strings.

## INTEGRITY STATEMENT
EXCEPT THE ALIAS SNIPPET ABOVE, AND THE LIBRARIES USED, ALL OF THE CODE IN THIS PROJECT WAS WRITTEN ENTIRELY BY ME. NO LARGE LANGUAGE MODEL OR OTHER ONLINE OR OFFLINE SOURCE WAS USED FOR THE DEVELOPMENT OF THE SOURCE CODE OF THIS PROJECT.

# Project Implementation and Description.

## Status
The implementation submitted on Friday, 15th September 2023, passes all tests defined in the `~cs537-1/tests/P1/` directory
- `~cs537-1/tests/P1/test-wman.csh`: Scores 7/7
- `~cs537-1/tests/P1/test-wapropos.csh`: Scores 3/3
- `~cs537-1/tests/P1/test-wgroff.csh`: Scores 14/14

## 1. `wman`

### List of libraries included
- `<stdio.h>`: Standard functions like `printf`, `fprintf`, `sprintf`, `fopen`, `fgets`, `fclose` etc.
- `<stdlib.h>`: Standard functions like `exit`, `atoi`
- `<string.h>`: String functions like `strlen`
- `<unistd.h>`: The `access` function and the `F_OK` and `R_OK` macros.

### List of macros defined
- `WMAN_FAILURE = 1`:  Exit status for failure
- `WMAN_SUCCESS = 0`:  Exit status for success
- `MIN_SECTION = 1`:  Minimum section value
- `MAX_SECTION = 9`:  Maximum section value
- `MAX_STR_LENGTH = 256`:  Maximum length of a string or `char[]`
- `SCCP`: Short for `static const char*`, which was used for defining format strings
- `IS_NULL(x)`: Descriptive macro to check if a value is `null`. Equivalent to `if (NULL == x)`
- `IS_NOT_NULL(x)`: Descriptive macro to check if a value is not `null`. Equivalent to `if (NULL != x)`
- `IF_FORMAT_FAILED(x)`: Descriptive macro to check if `sprintf` failed. Equivalent to `if (x < 0)`. `sprintf` returns a negative value is it fails.

### List of constants defined
- `INVALID_SECTION`: Error message if the section specified was incorrect
- `INVALID_USE`: Error message if the program was used incorrectly
- `NO_ARG`: Error message if no arguments were specified
- `PAGE_NOT_FOUND`: Error message if manual page was not found across any section.
- `PAGE_NOT_FOUND_IN_SECTION`: Error message if manual page was not found in the specified section
- `ERROR_IN_FORMAT`: Error message for when `sprintf` fails
- `ERROR_IN_FOPEN`: Error message for when `fopen` fails
- `FILEPATH`: Base filepath for the manual pages directory

### List of functions defined (in order of appearance)
```c
/**
 * To print the error message when a manual page was not found.
 */
void page_not_found_msg(char* str, const char* page);
```
```c
/**
 * To print the error message when a manual page was not found
 * in the specified section.
 */
void page_not_found_in_section_msg(char* str, const char* page, int section);
```
```c
/**
 * To get the filepath to the manual page in a given section.
 * It uses the above defined `FILEPATH` as a template, and fills it using
 * `sprintf` to make a filepath string, which is stored in `str`.
 * Assumes str has enough capacity to store the filename.
 */
void build_filepath(char* str, char* page, int section);
```
```c
/**
 * To search for the given manual page in a specific section,
 * i.e search specifically for `man_pages/man<section>/<page>.<section>`
 * If a match is found, returns that pointer to a corresponding FILE
 */
FILE* search_page_in_section(char* page, int section);
```
```c
/**
 * To search for the given manual page in all sub-directories of `man_pages/`.
 * Internally uses `search_page_in_section`.
 * If a match is found, returns that pointer to a corresponding FILE
 */
FILE* search_all_pages(char* page);
```
```c
/** To print the manual page specified by the given file `handle`.
 */
void print_file(FILE* handle);
```
```c
/**
 * Driver, checks CLAs and runs `search_all_pages` or `search_page_in_section`
 * appropriately, or displays error messages if program was used incorrectly.
 */
int main(int argc, char* argv[])
```


## 2. `wapropos`

### List of libraries included
- `<dirent.h>`: Functions and structs to traverse a directory, like `DIR`, `struct dirent`, `opendir`, `readdir`, `closedir`.
- `<stdio.h>`: Standard functions like `printf`, `fprintf`, `sprintf`, `fopen`, `fgets`, `fclose` etc.
- `<stdlib.h>`: Standard functions like `malloc`, `free`, `exit`, `atoi`
- `<string.h>`: String functions like `strlen`, `strstr`
- `<unistd.h>`: The `access` function and the `F_OK` and `R_OK` macros.

### List of macros defined
- `WAPROPOS_FAILURE = 1`:  Exit status for failure
- `WAPROPOS_SUCCESS = 0`:  Exit status for success
- `MIN_SECTION = 1`:  Minimum section value
- `MAX_SECTION = 9`:  Maximum section value
- `MAX_STR_LENGTH = 256`:  Maximum length of a string or `char[]`
- `SCCP`: Short for `static const char*`, which was used for defining format strings
- `IS_NULL(x)`: Descriptive macro to check if a value is `null`. Equivalent to `if (NULL == x)`
- `IS_NOT_NULL(x)`: Descriptive macro to check if a value is not `null`. Equivalent to `if (NULL != x)`
- `IF_FORMAT_FAILED(x)`: Descriptive macro to check if `sprintf` failed. Equivalent to `if (x < 0)`. `sprintf` returns a negative value is it fails.

### List of constants defined
- `INVALID_USE`: Error message if the program was used incorrectly
- `NO_ARG`: Error message if no arguments were specified
- `KEYWORD_NOT_FOUND`: Error message if the specified keyword was not found across any manual pages.
- `ERROR_IN_FORMAT`: Error message for when `sprintf` fails
- `ERROR_IN_FOPEN`: Error message for when `fopen` fails
- `ERROR_IN_OPENDIR`: Error message for when `opendir` fails
- `WAPROPOS_OUTPUT`: Template for program output
- `BASE_FILEPATH`: Base filepath for the manual pages directory

### List of functions defined (in order of appearance)
```c
/**
 * Prints the wapropos output to stdout
 */
void print_apropos(char* name, int section);
```
```c
/**
 * Build a complete filepath by joining the name of the file to the
 * above defined BASE_FILEPATH.
 * uses `sprintf` to make a filepath string, which is stored in `str`.
 * Assumes str has enough capacity to store the filename.
 */
void join_file_to_base(char* str, char* base, char* filename);
```
```c
/**
 * Checks if the given file contains the specified keyword.
 * While reading the file, this function stores what the name_one_liner
 * is, and if a match is found, returns this name_one_liner
 * as a heap allocated char array.
 * Otherwise returns NULL.
 */
char* contains_keyword(FILE* handle, char* keyword);
```
```c
/**
 * Searches for the specified keyword in the man_pages sub-directories
 * Returns the number of man_pages that contain the keyword.
 * A return value of 0 means the keyword was not found.
 */
int search_keyword(char* keyword);
```
```c
/**
 * Driver, checks CLAs and runs `search_keyword`.
 * If `search_keyword` returns 0, displays KEYWORD_NOT_FOUND message
 * Displays error messages if program was used incorrectly.
 */
int main(int argc, char* argv[])
```


## 3. `wgroff`

### List of libraries included
- `<ctype.h>`: To use the `toupper` function for character case conversion.
- `<stdio.h>`: Standard functions like `printf`, `fprintf`, `sprintf`, `fopen`, `fgets`, `fclose` etc.
- `<stdlib.h>`: Standard functions like `exit`, `atoi`
- `<string.h>`: String functions like `strlen`, `strstr`
- `<unistd.h>`: The `access` function and the `F_OK` and `R_OK` macros.

### List of macros defined
- `WAPROPOS_FAILURE = 1`:  Exit status for failure
- `WAPROPOS_SUCCESS = 0`:  Exit status for success
- `MIN_SECTION = 1`:  Minimum section value
- `MAX_SECTION = 9`:  Maximum section value
- `MAX_STR_LENGTH = 256`:  Maximum length of a string or `char[]`
- `SCCP`: Short for `static const char*`, which was used for defining format strings
- `DATE_STR_LENGTH = 10`:  Length of a date string
- `YEAR_STR_LENGTH = 4`:  Length of a year string
- `MONTH_STR_LENGTH = 2`:  Length of a month string
- `DAY_STR_LENGTH = 2`:  Length of a day string
- `MAX_FILENAME_LENGTH = 100`:  Maximum length of the name of a file
- `MAX_STR_LENGTH = 256`:  Maximum length of a string or `char[]`
- `MAX_COMMAND_LENGTH = 20`:  Maximum length of a string or `char[]`
- `OUTPUT_LENGTH = 80`:  Maximum length of a string or `char[]`
- `TH_ARG_COUNT = 3`:  Number of arguments in the first line
- `SH_ARG_COUNT = 1`:  Number of arguments in the section line
- `NULL_TERMINATOR = '\0'`:  String terminator character
- `COMMENT = '#'`:  Infile comment character
- `SPACE = ' '`:  Space character
- `INFILE_CSI_TH  = ".TH"`:  Infile control sequence for title header
- `INFILE_CSI_SH  = ".SH"`:  Infile control sequence for section header
- `INFILE_CSI_BOLD =  "/fB"`:  Infile control sequence for bold
- `INFILE_CSI_BOLD_L =  3`:  Control sequence length for bold
- `INFILE_CSI_ITALIC =  "/fI"`:  Infile control sequence for italic
- `INFILE_CSI_ITALIC_L =  3`:  Control sequence length for italic
- `INFILE_CSI_UNDERLINE =  "/fU"`:  Infile control sequence for underline
- `INFILE_CSI_UNDERLINE_L =  3`:  Control sequence length for underline
- `INFILE_CSI_NORMAL =  "/fP"`:  Infile control sequence for reset
- `INFILE_CSI_NORMAL_L =  3`:  Control sequence length for reset
- `INFILE_CSI_SLASH =  "//"`:  Infile control sequence for slash
- `INFILE_CSI_SLASH_L =  2`:  Control sequence length for slash
- `ANSI_CSI_BOLD = "\033[1m"`:       ANSI control sequence for bold
- `ANSI_CSI_BOLD_L = 4`:  ANSI control sequence length for bold
- `ANSI_CSI_ITALIC = "\033[3m"`:  ANSI control sequence for italic
- `ANSI_CSI_ITALIC_L = 4`:  ANSI control sequence length for italic
- `ANSI_CSI_UNDERLINE = "\033[4m"`:  ANSI control sequence for underline
- `ANSI_CSI_UNDERLINE_L = 4`:  ANSI control sequence length for underline
- `ANSI_CSI_NORMAL = "\033[0m"`:  ANSI control sequence for reset
- `ANSI_CSI_NORMAL_L = 4`:  ANSI control sequence length for reset
- `ANSI_CSI_SLASH = "/"`:  ANSI control sequence for slash
- `ANSI_CSI_SLASH_L = 1`:  ANSI control sequence for slash
- `INDENT = "       "`:  7 spaces for indentation
- `IS_NULL(x)`: Descriptive macro to check if a value is `null`. Equivalent to `if (NULL == x)`
- `IS_NOT_NULL(x)`: Descriptive macro to check if a value is not `null`. Equivalent to `if (NULL != x)`
- `IF_FORMAT_FAILED(x)`: Descriptive macro to check if `sprintf` failed. Equivalent to `if (x < 0)`. `sprintf` returns a negative value is it fails.

### List of constants defined
- `INVALID_USE`: Error message if the program was used incorrectly
- `NO_ARG`: Error message if no arguments were specified
- `ERROR_IN_FORMAT`: Error message for when `sprintf` fails
- `INVALID_FORMAT`: If the file is not formatted correctly
- `ERROR_IN_FOPEN`: Error message for when `fopen` fails
- `ERROR_IN_FOPEN_W`: Error message for when `fopen` fails in write mode
- `FILE_NOT_FOUND`: Error message for a the input file cannot be found
- `OUT_FILE`: Template for the output file
- `TH_FORMAT`: Expected format of the Title Header line
- `SH_FORMAT`: Expected format of the Section Header line
- `OUTFILE_FIRST_LINE`: Template for the first line of the output file
- `OUTFILE_LAST_LINE`: Template for the last line of the output file
- `OUTFILE_SECTION_HEADER`: Template for the section line of the output file
- `OUTFILE_INDENTED_LINE`: Template for the other lines, which are indented 7 spaces


### List of functions defined (in order of appearance)
```c
/**
 * Parses, formats and writes an indented line to the outfile
 */
void write_line(FILE* outfile, char* line);
```
```c
/**
 * Writes the formatted title header line to the outfile
 */
void write_first_line(FILE* outfile, char* command, int section);
```
```c
/**
 * Write the center aligned date as the last line of the file
 */
void write_last_line(FILE* outfile, char* date);
```
```c
/**
 * Reads the infile, performs preliminary parsing like checking for .TH
 * checking for .SH, checking for comments.
 *
 * This function creates the outfile, but delegates writing to one of
 *     - write_first_line
 *     - write_line
 *     - write_last_line
 */
void parse_file(FILE* handle);
```
```c
/**
 * Checks whether the specified infile exists.
 * If it does, opens the file in read mode.
 * Uses `parse_file` to read the file and create the outfile
 */
void run_wgroff(char* filename);
```
```c
/**
 * Driver, checks CLAs and runs `run_wgroff`.
 * Displays error messages if program was used incorrectly.
 */
int main(int argc, char *argv[]);
```
