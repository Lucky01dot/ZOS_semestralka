#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FatTable.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <filesystem_name>\n", argv[0]);
        return 1;
    }

    const char *filesystem_name = argv[1];
    char command[100];

    // Načti stav souborového systému
    load_system_state(filesystem_name);
    

    printf("Filesystem ready. Enter commands:\n");
    while (1) {
        printf("> ");
        if (!fgets(command, sizeof(command), stdin)) {
            printf("\n");
            break; // Konec programu
        }

        // Odstranění nového řádku
        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "exit") == 0) {
            printf("Exiting...\n");
            break;
        }

        process_command(filesystem_name, command);
    }

    // Uložení souborového systému při ukončení
    save_system_state(filesystem_name);

    return 0;
}
