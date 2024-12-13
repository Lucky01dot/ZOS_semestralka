#ifndef FAT_TABLE_H
#define FAT_TABLE_H

#include <stdint.h>
#include <stdbool.h>

// File name: FatTable.h
// Description: Header file for a pseudo-FAT filesystem
// Author: Jan Čácha

// Special markers in the FAT table
#define FAT_UNUSED       (INT32_MAX - 1) // Indicates an unused cluster
#define FAT_FILE_END     (INT32_MAX - 2) // Marks the end of a file
#define FAT_BAD_CLUSTER  (INT32_MAX - 3) // Marks a bad cluster
#define MAX_ITEM_NAME_SIZE 256           // Maximum size for item names
#define MAX_CHILDREN 128                 // Maximum number of children per directory

// Structure describing the filesystem properties
typedef struct FSDescription {
    char signature[9];              // Filesystem author's signature, e.g., "novak"
    int32_t disk_size;              // Total size of the virtual filesystem
    int32_t cluster_size;           // Size of a single cluster (in bytes)
    int32_t cluster_count;          // Total number of clusters
    int32_t fat_count;              // Number of entries in each FAT table
    int32_t *fat1_start_address;    // Starting address of the FAT1 table
    int32_t *fat2_start_address;    // Starting address of the FAT2 table (if present)
    int32_t data_start_address;     // Starting address of the data blocks (root directory)
} FSDescription;

// Structure representing a directory or file item
typedef struct DirectoryItem {
    char item_name[MAX_ITEM_NAME_SIZE];  // Name of the item
    bool isFile;                         // True if it is a file, False if it is a directory
    int32_t size;                        // Size of the item (for files)
    int32_t start_cluster;               // Starting cluster of the item
    struct DirectoryItem *parent;        // Parent directory of the item
    struct DirectoryItem *children[MAX_CHILDREN];  // Array of child items (for directories)
    int child_count;                     // Number of child items
} DirectoryItem;

// Global variables representing the filesystem state
extern FSDescription fs_description; // Filesystem descriptor
extern int32_t *fat_table1;          // Pointer to the first FAT table
extern int32_t *fat_table2;          // Pointer to the second FAT table (if present)
extern DirectoryItem root_directory; // Root directory of the filesystem
extern DirectoryItem *current_directory; // Current working directory
extern char *fs_data;                // Pointer to the filesystem's data blocks (memory or disk)


// Filesystem initialization and state management
void initialize_filesystem(int32_t disk_size, int32_t cluster_size); // Initialize a new filesystem
void format_filesystem(const char *filename, int32_t disk_size, int32_t cluster_size); // Format the filesystem
void save_system_state(const char *filename);    // Save the current filesystem state to a file
void load_system_state(const char *filename);    // Load a saved filesystem state from a file
void process_command(const char *filename, char *command); // Process a command for the filesystem

// Cluster management
void allocate_clusters_for_directory(DirectoryItem *dir, int clusters_needed); // Allocate clusters for a directory
int32_t allocate_cluster();   // Allocate a single free cluster

// Filesystem operations
void mkdir(const char *path);   // Create a new directory
void rmdir(const char *path);   // Remove a directory
void rm(const char *name);      // Remove a file or directory
void cd(const char *path);      // Change the current working directory
void cp(const char *src_path, const char *dest_path); // Copy files or directories
void mv(const char *src_path, const char *dest_path); // Move or rename files/directories
void ls(const char *name);      // List the contents of a directory
void info(const char *name);    // Display information about a file or directory
void pwd();                     // Print the current working directory path
void check();                   // Check the filesystem's integrity
void bug(const char *name);     // Simulate a bug for testing
void incp(const char *source, const char *destination); // Copy data from an external file into the filesystem
void outcp(const char *source, const char *destination); // Copy data from the filesystem to an external file
void cat(const char *source);   // Display the contents of a file
void load(const char *filename, const char *source); // Load data into the filesystem from an external source

#endif // FAT_TABLE_H
