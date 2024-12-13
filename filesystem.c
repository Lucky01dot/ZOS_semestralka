#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FatTable.h"

FSDescription fs_description;
int32_t *fat_table1 = NULL;
int32_t *fat_table2 = NULL;
DirectoryItem root_directory;
DirectoryItem *current_directory = NULL; // Pointer to the current directory
char *fs_data;  // Pointer to filesystem data (this should represent actual data on disk or memory)

// Initializes the filesystem with the given disk size and cluster size
void initialize_filesystem(int32_t disk_size, int32_t cluster_size) {
    // Validate input parameters
    if (disk_size <= 0 || cluster_size <= 0 || cluster_size > disk_size) {
        fprintf(stderr, "Error: Invalid parameters. Disk size: %d, cluster size: %d\n", disk_size, cluster_size);
        exit(EXIT_FAILURE);
    }

    // Set basic filesystem information
    strncpy(fs_description.signature, "cacha", sizeof(fs_description.signature) - 1);
    fs_description.signature[sizeof(fs_description.signature) - 1] = '\0'; // Ensure null termination
    fs_description.disk_size = disk_size;
    fs_description.cluster_size = cluster_size;
    fs_description.cluster_count = disk_size / cluster_size;
    fs_description.fat_count = fs_description.cluster_count;

    // Allocate memory for the filesystem data
    fs_data = (char *)malloc(fs_description.cluster_count * fs_description.cluster_size);

    // Allocate memory for FAT tables
    fat_table1 = (int32_t *)malloc(fs_description.fat_count * sizeof(int32_t));
    fat_table2 = (int32_t *)malloc(fs_description.fat_count * sizeof(int32_t));
    if (!fat_table1 || !fat_table2) {
        fprintf(stderr, "Error: Insufficient memory for FAT tables (%d entries).\n", fs_description.fat_count);
        exit(EXIT_FAILURE);
    }

    // Initialize FAT tables
    for (int32_t i = 0; i < fs_description.fat_count; i++) {
        fat_table1[i] = FAT_UNUSED;
        fat_table2[i] = FAT_UNUSED;
    }

    // Initialize root directory
    memset(&root_directory, 0, sizeof(DirectoryItem));
    strncpy(root_directory.item_name, "root", sizeof(root_directory.item_name) - 1);
    root_directory.item_name[sizeof(root_directory.item_name) - 1] = '\0'; // Ensure null termination
    root_directory.isFile = false;
    root_directory.size = 0;
    root_directory.start_cluster = 0;
    root_directory.parent = NULL;
    root_directory.child_count = 0;

    // Mark root directory cluster as end of file
    fat_table1[root_directory.start_cluster] = FAT_FILE_END;
    fat_table2[root_directory.start_cluster] = FAT_FILE_END;

    // Set the current directory to root
    current_directory = &root_directory;

    printf("Filesystem initialized:\n");
    printf("  Disk size: %d MB\n", disk_size / (1024 * 1024));
    printf("  Cluster size: %d B\n", cluster_size);
    printf("  Cluster count: %d\n", fs_description.cluster_count);
}

// Formats the filesystem and saves its initial state to a file
void format_filesystem(const char *filename, int32_t disk_size, int32_t cluster_size) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to create filesystem file");
        exit(EXIT_FAILURE);
    }

    initialize_filesystem(disk_size, cluster_size); // Initialize the filesystem
    save_system_state(filename); // Save the initialized state

    fclose(file);
    printf("FORMAT COMPLETE\n");
}

// Recursively saves a directory and its children to a file
void save_directory(FILE *file, DirectoryItem *directory) {
    // Save the current directory
    fwrite(directory, sizeof(DirectoryItem), 1, file);

    // Recursively save all child items
    for (int i = 0; i < directory->child_count; i++) {
        save_directory(file, directory->children[i]);
    }
}

// Saves the current state of the filesystem to a file
void save_system_state(const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to save filesystem state");
        return;
    }

    // Save FSDescription structure
    fwrite(&fs_description, sizeof(FSDescription), 1, file);

    // Save FAT tables
    fwrite(fat_table1, sizeof(int32_t), fs_description.fat_count, file);
    fwrite(fat_table2, sizeof(int32_t), fs_description.fat_count, file);

    // Save the root directory and its children
    save_directory(file, &root_directory);

    // Save the filesystem data
    fwrite(fs_data, 1, fs_description.disk_size, file);

    fclose(file);
    printf("Filesystem state saved to %s\n", filename);
}

// Recursively loads a directory and its children from a file
void load_directory(FILE *file, DirectoryItem *directory, DirectoryItem *parent) {
    // Load the current directory
    fread(directory, sizeof(DirectoryItem), 1, file);
    directory->parent = parent;  // Restore the parent relationship

    // Recursively load all child items
    for (int i = 0; i < directory->child_count; i++) {
        directory->children[i] = (DirectoryItem *)malloc(sizeof(DirectoryItem));
        if (!directory->children[i]) {
            fprintf(stderr, "Memory allocation failed for child directory.\n");
            exit(EXIT_FAILURE);
        }
        load_directory(file, directory->children[i], directory);  // Pass current directory as parent
    }
}

// Loads the filesystem state from a file
void load_system_state(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Filesystem file not found. Use 'format' to initialize.\n");
        file = fopen(filename, "wb"); // Create an empty file
        if (!file) {
            perror("Failed to create empty filesystem file");
            exit(EXIT_FAILURE);
        }
        fclose(file);
        return;
    }

    // Load FSDescription structure
    fread(&fs_description, sizeof(FSDescription), 1, file);

    // Allocate memory for FAT tables
    fat_table1 = (int32_t *)malloc(fs_description.fat_count * sizeof(int32_t));
    fat_table2 = (int32_t *)malloc(fs_description.fat_count * sizeof(int32_t));
    if (!fat_table1 || !fat_table2) {
        fprintf(stderr, "Memory allocation failed for FAT tables.\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Load FAT tables
    fread(fat_table1, sizeof(int32_t), fs_description.fat_count, file);
    fread(fat_table2, sizeof(int32_t), fs_description.fat_count, file);

    // Allocate memory for virtual disk data
    fs_data = malloc(fs_description.disk_size);
    if (!fs_data) {
        fprintf(stderr, "Memory allocation failed for fs_data.\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Load the root directory and its children
    load_directory(file, &root_directory, NULL);

    // Load the filesystem data
    fread(fs_data, 1, fs_description.disk_size, file);

    // Set the current directory to root
    current_directory = &root_directory;

    fclose(file);
    printf("Filesystem state loaded from %s\n", filename);
}