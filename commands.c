#include <stdio.h>
#include <string.h>
#include "FatTable.h"

void handle_format_command(const char *filename, const char *size_str) {
    int32_t size_in_mb;
    if (sscanf(size_str, "%dMB", &size_in_mb) != 1 || size_in_mb <= 0) {
        printf("INVALID SIZE FORMAT\n");
        return;
    }

    int32_t disk_size = size_in_mb * 1024 * 1024;
    int32_t cluster_size = 4096; 

    format_filesystem(filename, disk_size, cluster_size);
}

void process_command(const char *filename, char *command) {
    if (strncmp(command, "format", 6) == 0) {
        char size_str[20];
        if (sscanf(command + 7, "%s", size_str) == 1) {
            handle_format_command(filename, size_str);
        } else {
            printf("INVALID COMMAND\n");
        }
    } else if (strncmp(command, "ls", 2) == 0) {
        if (!fat_table1) {
            printf("Filesystem not formatted. Use 'format' first.\n");
            return;
        }

        char dir_name[MAX_ITEM_NAME_SIZE];
        if (sscanf(command + 3, "%s", dir_name) == 1) {
            ls(dir_name); 
        } else {
            ls(NULL); 
        }
    } else if (strncmp(command, "mkdir", 5) == 0) {
        if (!fat_table1) {
            printf("Filesystem not formatted. Use 'format' first.\n");
            return;
        }
        char dir_name[MAX_ITEM_NAME_SIZE];
        if (sscanf(command + 6, "%s", dir_name) == 1) {
            mkdir(dir_name);
        } else {
            printf("INVALID COMMAND\n");
        }
    } else if (strncmp(command, "cd", 2) == 0) {
        if (!fat_table1) {
            printf("Filesystem not formatted. Use 'format' first.\n");
            return;
        }
        char path[MAX_ITEM_NAME_SIZE];
        if (sscanf(command + 3, "%s", path) == 1) {
            cd(path);
        } else {
            printf("INVALID COMMAND\n");
        }
    } else if (strcmp(command, "pwd") == 0) {
        if (!fat_table1) {
            printf("Filesystem not formatted. Use 'format' first.\n");
            return;
        }
        pwd();
    } else if (strncmp(command, "rmdir", 5) == 0) {
        if (!fat_table1) {
            printf("Filesystem not formatted. Use 'format' first.\n");
            return;
        }
        char dir_name[MAX_ITEM_NAME_SIZE];
        if (sscanf(command + 6, "%s", dir_name) == 1) {
            rmdir(dir_name);
        } else {
            printf("INVALID COMMAND\n");
        }
    } else if (strncmp(command, "rm", 2) == 0) {
        if (!fat_table1) {
            printf("Filesystem not formatted. Use 'format' first.\n");
            return;
        }
        char dir_name[MAX_ITEM_NAME_SIZE];
        if (sscanf(command + 3, "%s", dir_name) == 1) {
            rm(dir_name);
        } else {
            printf("INVALID COMMAND\n");
        }
    } else if (strncmp(command, "cp", 2) == 0) {
        if (!fat_table1) {
            printf("Filesystem not formatted. Use 'format' first.\n");
            return;
        }

        
        char src_path[MAX_ITEM_NAME_SIZE];
        char dest_path[MAX_ITEM_NAME_SIZE];
        int args_parsed = sscanf(command + 3, "%s %s", src_path, dest_path);

        if (args_parsed != 2) {
            printf("Invalid command syntax. Usage: cp <source> <destination>\n");
            return;
        }

        
        cp(src_path, dest_path);
    } else if (strncmp(command, "mv", 2) == 0) {
        if (!fat_table1) {
            printf("Filesystem not formatted. Use 'format' first.\n");
            return;
        }

        
        char src_path[MAX_ITEM_NAME_SIZE];
        char dest_path[MAX_ITEM_NAME_SIZE];
        int args_parsed = sscanf(command + 3, "%s %s", src_path, dest_path);

        if (args_parsed != 2) {
            printf("Invalid command syntax. Usage: cp <source> <destination>\n");
            return;
        }

        
        mv(src_path, dest_path);
    } else if (strncmp(command, "info", 4) == 0) {
        if (!fat_table1) {
            printf("Filesystem not formatted. Use 'format' first.\n");
            return;
        }

        
        if (strlen(command) <= 5 || command[4] != ' ') {
            printf("INVALID COMMAND\n");
            return;
        }

        
        char dir_name[MAX_ITEM_NAME_SIZE];
        strncpy(dir_name, command + 5, MAX_ITEM_NAME_SIZE - 1);
        dir_name[MAX_ITEM_NAME_SIZE - 1] = '\0'; 

        
        info(dir_name);
    } else if (strcmp(command, "check") == 0) {
        if (!fat_table1) {
            printf("Filesystem not formatted. Use 'format' first.\n");
            return;
        }
        check();
    } else if (strncmp(command, "bug", 3) == 0) {
        if (!fat_table1) {
            printf("Filesystem not formatted. Use 'format' first.\n");
            return;
        }

        
        char dir_name[MAX_ITEM_NAME_SIZE];
        if (sscanf(command + 3, "%s", dir_name) == 1) {
            bug(dir_name); 
        } 
    } else if (strncmp(command, "incp", 4) == 0) {
        if (!fat_table1) {
            printf("Filesystem not formatted. Use 'format' first.\n");
            return;
        }

        
        const char *args = command + 5; 
        while (*args == ' ') args++;   

        char src_path[MAX_ITEM_NAME_SIZE];
        char dest_path[MAX_ITEM_NAME_SIZE];

        
        int args_parsed = sscanf(args, "%255s %255s", src_path, dest_path);

        if (args_parsed != 2) {
            printf("Invalid command syntax. Usage: incp <source> <destination>\n");
            return;
        }

        
        incp(src_path, dest_path);
    } else if (strncmp(command, "outcp", 5) == 0) {
        if (!fat_table1) {
            printf("Filesystem not formatted. Use 'format' first.\n");
            return;
        }

        
        const char *args = command + 6; 
        while (*args == ' ') args++;   

        char src_path[MAX_ITEM_NAME_SIZE];
        char dest_path[MAX_ITEM_NAME_SIZE];

        
        int args_parsed = sscanf(args, "%255s %255s", src_path, dest_path);

        if (args_parsed != 2) {
            printf("Invalid command syntax. Usage: incp <source> <destination>\n");
            return;
        }

        
        outcp(src_path, dest_path);
    } else if (strncmp(command, "cat", 3) == 0) {
        if (!fat_table1) {
            printf("Filesystem not formatted. Use 'format' first.\n");
            return;
        }
        char path[MAX_ITEM_NAME_SIZE];
        if (sscanf(command + 4, "%s", path) == 1) {
            cat(path);
        } else {
            printf("INVALID COMMAND\n");
        }
    } else if (strncmp(command, "load", 4) == 0) {
        if (!fat_table1) {
            printf("Filesystem not formatted. Use 'format' first.\n");
            return;
        }

        
        const char *args = command + 5; 
        while (*args == ' ') args++;   

        char src_path[MAX_ITEM_NAME_SIZE];

        
        int args_parsed = sscanf(args, "%255s", src_path);

        if (args_parsed != 1) {
            printf("Invalid command syntax. Usage: load <source>\n");
            return;
        }

        
        load(filename, src_path);
    }else {
        printf("UNKNOWN COMMAND\n");
    }
}
