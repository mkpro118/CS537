#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "wfs.h"

void print_usage() {
    printf("Usage: mkfs.wfs <img_file>\n");
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        print_usage();
        goto success;
    }

    const char* filename = argv[1];

    FILE* img_file = fopen(filename, "w"); 

    if (!img_file) {
        fprintf(stderr, "Failed to open file \"%s\"!\n", filename);
        goto fail;
    }

    struct wfs_sb sb;
    wfs_sb_init(&sb);

    size_t sb_size = sizeof(struct wfs_sb);
    size_t bytes_written;

    if ((bytes_written = fwrite(&sb, sb_size, 1, img_file)) != 1) {
        fprintf(stderr, "Failed to write to file \"%s\" (%% = %ld / %d)\n", filename, bytes_written, 1);
        goto fail;
    }

    fprintf(stderr, "Successfully initialized file \"%s\" for WFS\n", filename);

    success:
    return 0;

    fail:
    perror("FATAL ERROR: Couldn't intialize WFS.\n");
    return 1;
}
