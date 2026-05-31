#include "commands.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *command;

    if (argc < 2) {
        cli_print_usage();
        return 1;
    }

    command = argv[1];
    if (strcmp(command, "init") == 0) {
        return cmd_init(argc, argv);
    }
    if (strcmp(command, "create") == 0) {
        return cmd_create(argc, argv);
    }
    if (strcmp(command, "delete") == 0 || strcmp(command, "rm") == 0) {
        return cmd_delete(argc, argv);
    }
    if (strcmp(command, "write") == 0) {
        return cmd_write(argc, argv);
    }
    if (strcmp(command, "read") == 0 || strcmp(command, "cat") == 0) {
        return cmd_read(argc, argv);
    }
    if (strcmp(command, "list") == 0 || strcmp(command, "ls") == 0) {
        return cmd_list(argc, argv);
    }
    if (strcmp(command, "status") == 0 || strcmp(command, "stat") == 0) {
        return cmd_status(argc, argv);
    }

    fprintf(stderr, "Hata: bilinmeyen komut: %s\n", command);
    cli_print_usage();
    return 1;
}
