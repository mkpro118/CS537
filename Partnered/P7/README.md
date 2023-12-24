# Project 7

- Names: Mrigank Kumar, Saanvi Malhotra
- CS Logins: mrigank, malhotra
- WISC IDs: 9083537424, 9083552423
- Emails: mkumar42@wisc.edu, smalhotra9@wisc.edu

## INTEGRITY STATEMENT
EXCEPT FOR THE GIVEN SOURCE CODE, ALL OF THE CODE IN THIS PROJECT WAS WRITTEN ENTIRELY BY US. NO LARGE LANGUAGE MODEL OR OTHER ONLINE OR OFFLINE SOURCE WAS USED FOR THE DEVELOPMENT OR MODIFICATION OF THE SOURCE CODE OF THIS PROJECT.

# Project Implementation and Description.

## Status
The implementation submitted on Friday, 8th December 2023, passes all tests defined in the `~cs537-1/tests/P7` directory
- `~cs537-1/tests/P7/start.py`: Scores 11/11 (Score: 11)

## Trivia
0. We did this project two different ways.
1. We did the project using the file operations, such as `fseek`, `fread`, `fwrite` etc. This implementation works perectly, but is slower than our second approach.
2. We did the project by using a file backed mmap. This implementation also works perfectly, and is much faster than our first implementation.
3. This `README.md` file is being written in the mounted file system.
4. The submitted version runs the mmap version. To run the file operations version, compile our code with the `-DWFS_NOMMAP` flag in the `CFLAGS`.
