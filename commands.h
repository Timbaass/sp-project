#ifndef COMMANDS_H
#define COMMANDS_H

void cli_print_usage(void);

int cmd_init(int argc, char **argv);
int cmd_create(int argc, char **argv);
int cmd_delete(int argc, char **argv);
int cmd_write(int argc, char **argv);
int cmd_read(int argc, char **argv);
int cmd_list(int argc, char **argv);
int cmd_status(int argc, char **argv);

#endif
