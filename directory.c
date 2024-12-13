#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FatTable.h"

/* POMOCNÉ FUNKCE */


// Global error flag for process_command
bool process_error = false;
static int32_t cluster_references[MAX_CHILDREN]; // Pole pro sledování referencí clusterů

char *strdup(const char *str) {
    if (str == NULL) return NULL;
    size_t len = strlen(str) + 1;
    char *copy = malloc(len);
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}


void increment_cluster_reference(int32_t cluster) {
    cluster_references[cluster]++;
}

void decrement_cluster_reference(int32_t cluster) {
    if (cluster_references[cluster] > 0) {
        cluster_references[cluster]--;
    }
}

int get_cluster_reference_count(int32_t cluster) {
    return cluster_references[cluster];
}

void copy_cluster_data(int32_t src_cluster, int32_t dest_cluster) {
    // Validace vstupních clusterů
    if (src_cluster < 0 || src_cluster >= fs_description.cluster_count ||
        dest_cluster < 0 || dest_cluster >= fs_description.cluster_count) {
        fprintf(stderr, "Error: Invalid cluster index.\n");
        return;
    }

    // Zvýšení referenčního počtu na cílový cluster
    increment_cluster_reference(dest_cluster);

    // Data už jsou shodná, nic není třeba kopírovat
    printf("Cluster %d is now referenced for destination.\n", dest_cluster);
}



void get_parent_path(const char *path, char *parent_path) {
    if (!path || !parent_path) {
        strcpy(parent_path, "/");
        return;
    }

    char temp_path[MAX_ITEM_NAME_SIZE];
    strncpy(temp_path, path, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';

    size_t len = strlen(temp_path);
    if (len > 1 && temp_path[len - 1] == '/') {
        temp_path[len - 1] = '\0';
    }

    char *last_slash = strrchr(temp_path, '/');
    if (last_slash) {
        if (last_slash == temp_path) {
            strcpy(parent_path, "/");
        } else {
            *last_slash = '\0';
            strcpy(parent_path, temp_path);
        }
    } else {
        strcpy(parent_path, ".");
    }
}



void free_cluster(int cluster) {
    fat_table1[cluster] = FAT_UNUSED;
    if (fat_table2) {
        fat_table2[cluster] = FAT_UNUSED;
    }
    memset(&fs_data[cluster * fs_description.cluster_size], 0, fs_description.cluster_size);
}

void free_directory(DirectoryItem *dir) {
    // Free any resources associated with the directory itself if needed.
    free(dir);
}



void update_directory_size(DirectoryItem *dir) {
    if (dir == NULL) {
        printf("Error: Directory is NULL.\n");
        return;
    }

    if (!dir->isFile) {
        int total_size = 0;

        // Iterate over child items
        for (int i = 0; i < dir->child_count; i++) {
            if (i >= MAX_CHILDREN || dir->children[i] == NULL) {
                printf("Error: Invalid child pointer at index %d\n", i);
                continue;
            }
            DirectoryItem *child = dir->children[i];
            if (child && child->isFile) {
                dir->size += child->size;
            }
        }


        // Update directory size
        dir->size = total_size;

        // Check if the directory exceeds the cluster size
        if (dir->size > fs_description.cluster_size) {
            int clusters_needed = (dir->size / fs_description.cluster_size) +
                                  ((dir->size % fs_description.cluster_size) > 0 ? 1 : 0);
            // Allocate clusters for the directory
            allocate_clusters_for_directory(dir, clusters_needed);
        }
    }
}





// This is a helper function that would handle allocating clusters for the directory
void allocate_clusters_for_directory(DirectoryItem *dir, int clusters_needed) {
    int current_cluster = dir->start_cluster;
    int allocated_clusters = 0;

    // Find the number of clusters already allocated
    while (current_cluster != FAT_FILE_END) {
        allocated_clusters++;
        current_cluster = fat_table1[current_cluster];
    }

    // Allocate additional clusters if needed
    while (allocated_clusters < clusters_needed) {
        int32_t new_cluster = allocate_cluster();
        if (new_cluster < 0) {
            printf("Error: Unable to allocate additional cluster for directory '%s'.\n", dir->item_name);
            return;
        }
        
        // Link the new cluster to the previous one
        current_cluster = dir->start_cluster;
        while (fat_table1[current_cluster] != FAT_FILE_END) {
            current_cluster = fat_table1[current_cluster];
        }
        fat_table1[current_cluster] = new_cluster;
        current_cluster = new_cluster;

        allocated_clusters++;
    }

    // Mark the end of the chain with FAT_FILE_END
    fat_table1[current_cluster] = FAT_FILE_END;
}


void* read_cluster_data(int32_t cluster, size_t size) {
    // Validate the cluster index and size to ensure safe access
    if (cluster < 0 || cluster >= fs_description.cluster_count) {
        fprintf(stderr, "Error: Invalid cluster index (%d).\n", cluster);
        return NULL;
    }

    if ((size_t)size > (size_t)fs_description.cluster_size) {
        fprintf(stderr, "Error: Data size exceeds cluster size.\n");
        return NULL;
    }

    void *buffer = malloc(size); // Allocate memory for the data
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return NULL;
    }

    size_t offset = (size_t)cluster * fs_description.cluster_size;
    memcpy(buffer, fs_data + offset, size);  // Copy data from the cluster

    return buffer;
}

void write_cluster_data(int32_t cluster, const void *data, size_t size) {
    if (cluster < 0 || cluster >= fs_description.cluster_count) {
        fprintf(stderr, "Error: Invalid cluster index (%d).\n", cluster);
        return;
    }

    if ((size_t)size > (size_t)fs_description.cluster_size) {
        fprintf(stderr, "Error: Data size exceeds cluster size.\n");
        return;
    }

    if (!data) {
        fprintf(stderr, "Error: Data pointer is NULL.\n");
        return;
    }

    size_t offset = (size_t)cluster * fs_description.cluster_size;
    memcpy(fs_data + offset, data, size);  // Write data into the cluster
}



DirectoryItem* find_directory_item(const char *name) {
    if (!current_directory) {
        printf("Error: Current file and his children are no initialized.\n");
        return NULL;
    }

    for (int i = 0; i < current_directory->child_count; i++) {
        DirectoryItem *child = current_directory->children[i];
        if (child) {
            if (strcmp(child->item_name, name) == 0) {
                return child;
            }
        }
    }

    return NULL; // Item not found
}




int add_directory_item(DirectoryItem *parent, const char *name, bool isFile, int size, int start_cluster) {
    // Add a new item (file or directory) to the specified parent directory
    if (parent->child_count >= MAX_CHILDREN) {
        return -1; // Parent directory is full
    }

    DirectoryItem *new_item = (DirectoryItem *)malloc(sizeof(DirectoryItem));
    if (!new_item) {
        return -1; // Memory allocation failed
    }

    strncpy(new_item->item_name, name, sizeof(new_item->item_name) - 1);
    new_item->item_name[sizeof(new_item->item_name) - 1] = '\0';
    new_item->isFile = isFile;
    new_item->size = size;
    new_item->start_cluster = start_cluster;
    new_item->parent = parent;

    parent->children[parent->child_count++] = new_item; // Add to the parent's child list

    // Update the size of the parent directory
    parent->size += size;

    // If the new item is a directory, initialize its size and allocate clusters
    if (!isFile) {
        parent->size += new_item->size;
    }

    return 0;
}

bool split_path(const char *path, char parts[MAX_CHILDREN][MAX_ITEM_NAME_SIZE], int *part_count) {
    if (!path || !parts || !part_count) {
        return false;
    }

    *part_count = 0;
    const char *start = path;
    while (*start) {
        while (*start == '/') start++; // Přeskoč '/' na začátku
        if (!*start) break;

        const char *end = strchr(start, '/');
        if (!end) end = start + strlen(start);

        size_t len = end - start;
        if (len >= MAX_ITEM_NAME_SIZE) {
            printf("ERROR: Path part too long.\n");
            return false;
        }

        strncpy(parts[*part_count], start, len);
        parts[*part_count][len] = '\0';
        (*part_count)++;

        start = end;
    }

    return true;
}

DirectoryItem* find_item_by_path(const char *path, DirectoryItem *start_directory) {
    if (!path || !start_directory) {
        printf("ERROR: Invalid path or start directory.\n");
        return NULL;
    }

    if (strcmp(path, "/") == 0) {
        return &root_directory;
    }

    char parts[MAX_CHILDREN][MAX_ITEM_NAME_SIZE];
    int part_count = 0;

    if (!split_path(path, parts, &part_count)) {
        printf("ERROR: Failed to split path '%s'.\n", path);
        return NULL;
    }

    DirectoryItem *current = (path[0] == '/') ? &root_directory : start_directory;

    for (int i = 0; i < part_count; i++) {
        if (strcmp(parts[i], ".") == 0) {
            continue;
        }
        if (strcmp(parts[i], "..") == 0) {
            if (current->parent) {
                current = current->parent;
            } else {
                printf("ERROR: No parent directory available.\n");
                return NULL;
            }
            continue;
        }

        bool found = false;
        for (int j = 0; j < current->child_count; j++) {
            if (strcmp(current->children[j]->item_name, parts[i]) == 0) {
                current = current->children[j];
                found = true;
                break;
            }
        }

        if (!found) {
            printf("ERROR: Part '%s' not found in directory '%s'.\n", parts[i], current->item_name);
            return NULL;
        }
    }

    return current;
}




// Allocate a cluster
int32_t allocate_cluster() {
    for (int32_t i = 0; i < fs_description.fat_count; i++) {
        if (fat_table1[i] == FAT_UNUSED) {
            fat_table1[i] = FAT_FILE_END; // Mark the cluster as the end of the file
            fat_table2[i] = FAT_FILE_END; // If a second FAT table is used
            return i; // Return the cluster index
        }
    }
    return FAT_UNUSED; // No free clusters available
}

// Helper function for rm (remove)
void rm_recursive(DirectoryItem *target) {
    if (!target) return;

    if (!target->isFile) {
        for (int i = 0; i < target->child_count; i++) {
            rm_recursive(target->children[i]);
        }
    }

    int32_t cluster = target->start_cluster;
    while (cluster != FAT_FILE_END) {
        decrement_cluster_reference(cluster);

        if (get_cluster_reference_count(cluster) == 0) {
            // Uvolníme cluster, pokud žádná reference nezůstává
            int32_t next_cluster = fat_table1[cluster];
            fat_table1[cluster] = FAT_UNUSED;
            fat_table2[cluster] = FAT_UNUSED;
            cluster = next_cluster;
        } else {
            cluster = fat_table1[cluster];
        }
    }

    free(target);
}


// Copy a file
void copy_file(int32_t src_cluster, int32_t *dest_cluster, DirectoryItem *new_item) {
    int32_t current_src = src_cluster;
    int32_t prev_dest = FAT_UNUSED;
    *dest_cluster = allocate_cluster();
    size_t copied_size = 0; // Number of bytes copied

    if (*dest_cluster == FAT_UNUSED) {
        fprintf(stderr, "Error: No free clusters available for copying the file.\n");
        return;
    }

    while (current_src != FAT_FILE_END) {
        // Copy data from the source cluster to the destination cluster
        void *data = read_cluster_data(current_src, fs_description.cluster_size);
        write_cluster_data(*dest_cluster, data, fs_description.cluster_size);
        free(data);

        copied_size += fs_description.cluster_size;

        // Link the cluster to the FAT
        if (prev_dest != FAT_UNUSED) {
            fat_table1[prev_dest] = *dest_cluster;
        }
        prev_dest = *dest_cluster;

        // Move to the next cluster
        current_src = fat_table1[current_src];
        if (current_src != FAT_FILE_END) {
            *dest_cluster = allocate_cluster();
            if (*dest_cluster == FAT_UNUSED) {
                fprintf(stderr, "Error: No free clusters available during copying.\n");
                fat_table1[prev_dest] = FAT_FILE_END; // Properly terminate the FAT
                break;
            }
        }
    }

    // Terminate the FAT for the new file
    fat_table1[prev_dest] = FAT_FILE_END;

    // Update the size of the destination file
    if (new_item != NULL) {
        new_item->size = copied_size;
    }
}

// Recursive directory copying
void copy_directory(DirectoryItem *src, DirectoryItem *dest) {
    for (int i = 0; i < src->child_count; i++) {
        if (dest->child_count >= MAX_CHILDREN) {
            fprintf(stderr, "Error: Destination directory has reached the maximum number of items.\n");
            return;
        }

        DirectoryItem *src_child = src->children[i];
        DirectoryItem *new_item = (DirectoryItem *)malloc(sizeof(DirectoryItem));
        if (!new_item) {
            fprintf(stderr, "Error: Memory allocation failed for new directory item.\n");
            continue;
        }

        strncpy(new_item->item_name, src_child->item_name, MAX_ITEM_NAME_SIZE);
        new_item->isFile = src_child->isFile;
        new_item->size = src_child->size;
        new_item->parent = dest;
        new_item->child_count = 0;

        if (src_child->isFile) {
            // Copy the file
            copy_file(src_child->start_cluster, &new_item->start_cluster, new_item);
        } else {
            // Allocate a cluster for the new directory
            new_item->start_cluster = allocate_cluster();
            if (new_item->start_cluster == FAT_UNUSED) {
                fprintf(stderr, "Error: No free clusters available for directory '%s'.\n", src_child->item_name);
                free(new_item);
                continue;
            }
            // Recursively copy the contents of the directory
            copy_directory(src_child, new_item);
        }

        // Add the new item to the destination directory
        dest->children[dest->child_count++] = new_item;
    }
}


//////////////////////////////////////////////////////////////////////////////////////////////////


/* PŘÍKAZY */

// Hlavní funkce cp


void cp(const char *src_path, const char *dest_path) {
    DirectoryItem *src = find_item_by_path(src_path, current_directory);
    if (!src || !src->isFile) {
        fprintf(stderr, "Error: Source '%s' not found or is not a file.\n", src_path);
        return;
    }

    // Rozdělení dest_path na adresář a název
    char dest_dir_path[MAX_ITEM_NAME_SIZE];
    char new_name[MAX_ITEM_NAME_SIZE] = "";

    const char *last_slash = strrchr(dest_path, '/');
    if (last_slash) {
        strncpy(dest_dir_path, dest_path, last_slash - dest_path);
        dest_dir_path[last_slash - dest_path] = '\0';
        strncpy(new_name, last_slash + 1, MAX_ITEM_NAME_SIZE - 1);
    } else {
        strncpy(dest_dir_path, "", MAX_ITEM_NAME_SIZE - 1);
        strncpy(new_name, dest_path, MAX_ITEM_NAME_SIZE - 1);
    }

    DirectoryItem *parent_dir = find_item_by_path(dest_dir_path, current_directory);
    if (!parent_dir || parent_dir->isFile) {
        fprintf(stderr, "Error: Destination directory '%s' not found or is not a directory.\n", dest_dir_path);
        return;
    }

    if (parent_dir->child_count >= MAX_CHILDREN) {
        fprintf(stderr, "Error: Destination directory '%s' is full.\n", dest_dir_path);
        return;
    }

    DirectoryItem *new_item = (DirectoryItem *)malloc(sizeof(DirectoryItem));
    if (!new_item) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    strncpy(new_item->item_name, new_name[0] ? new_name : src->item_name, MAX_ITEM_NAME_SIZE - 1);
    new_item->item_name[MAX_ITEM_NAME_SIZE - 1] = '\0';
    new_item->isFile = src->isFile;
    new_item->size = src->size;
    new_item->start_cluster = src->start_cluster;
    new_item->parent = parent_dir;

    // Zvýšení referencí clusterů
    int32_t current_cluster = src->start_cluster;
    while (current_cluster != FAT_FILE_END) {
        increment_cluster_reference(current_cluster);
        current_cluster = fat_table1[current_cluster];
    }

    parent_dir->children[parent_dir->child_count++] = new_item;

    

    printf("Successfully created a copy of '%s' at '%s'.\n", src_path, dest_path);
}

/*
TODO
FORMAT COMPLETE
> mkdir dir1
Directory 'dir1' created successfully.
> mkdir dir1/subdir
Directory 'dir1/subdir' created successfully.
> ls dir1
Contents of directory 'dir1':
Name          Type  Start Cluster
---------------------------------------------------
subdir        Dir   2         
> incp commands.c /dir1/subdir/c.c
File 'c.c' was successfully copied to '/dir1/subdir'.
> ls dir1/subdir
Error: Directory 'dir1/subdir' not found.
> info dir1/subdir/c.c
Size: 7143B
c.c 3,4
> outcp dir1/subdir/c.c c.txt
OK
> mkdir dir2
Directory 'dir2' created successfully.
> ls
Contents of directory 'root':
Name          Type  Start Cluster
---------------------------------------------------
dir1          Dir   1         
dir2          Dir   5         
> mv dir1/subidir/c.c dir2/c_c.c
ERROR: Part 'subidir' not found in directory 'dir1'.
Error: Source item 'dir1/subidir/c.c' not found.
> mv dir1/subdir/c.c dir2/c_c.c
Segmentation fault (core dumped)


*/

void mv(const char *src_path, const char *dest_path) {
    // Find the source item
    DirectoryItem *src = find_item_by_path(src_path, current_directory);
    if (!src) {
        fprintf(stderr, "Error: Source item '%s' not found.\n", src_path);
        return;
    }

    // Find the destination directory
    DirectoryItem *dest = find_item_by_path(dest_path, current_directory);

    if (dest) {
        // Destination exists: check if it's a directory
        if (dest->isFile) {
            fprintf(stderr, "Error: Destination '%s' is a file, not a directory.\n", dest_path);
            return;
        }

        // Check if the destination directory can accommodate more items
        if (dest->child_count >= MAX_CHILDREN) {
            fprintf(stderr, "Error: Destination directory '%s' has too many items.\n", dest_path);
            return;
        }

        // Prevent moving into its own subtree
        DirectoryItem *current = dest;
        while (current) {
            if (current == src) {
                fprintf(stderr, "Error: Cannot move '%s' into its own subtree.\n", src_path);
                return;
            }
            current = current->parent;
        }

        // Remove the source from its parent's children list
        if (src->parent) {
            DirectoryItem *src_parent = src->parent;
            for (int i = 0; i < src_parent->child_count; i++) {
                if (src_parent->children[i] == src) {
                    // Shift remaining children
                    for (int j = i; j < src_parent->child_count - 1; j++) {
                        src_parent->children[j] = src_parent->children[j + 1];
                    }
                    src_parent->children[--src_parent->child_count] = NULL;
                    src_parent->size -= sizeof(DirectoryItem);
                    break;
                }
            }
        }

        // Add the source to the destination directory
        src->parent = dest;
        dest->children[dest->child_count++] = src;

        printf("Successfully moved '%s' to '%s'.\n", src_path, dest_path);

    } else {
        // Destination does not exist: rename the source
        const char *new_name = strrchr(dest_path, '/');
        if (new_name) {
            new_name++; // Move past the '/'
        } else {
            new_name = dest_path;
        }

        // Ensure the new name does not conflict in the same directory
        DirectoryItem *src_parent = src->parent;
        for (int i = 0; i < src_parent->child_count; i++) {
            if (strcmp(src_parent->children[i]->item_name, new_name) == 0) {
                fprintf(stderr, "Error: A file or directory with the name '%s' already exists in the target location.\n", new_name);
                return;
            }
        }

        // Rename the item
        strncpy(src->item_name, new_name, MAX_ITEM_NAME_SIZE - 1);
        src->item_name[MAX_ITEM_NAME_SIZE - 1] = '\0';

        printf("Successfully renamed '%s' to '%s'.\n", src_path, dest_path);
    }
}



void info(const char *path) {
    // Předpokládejme, že current_directory ukazuje na aktuální adresář
    DirectoryItem *item = find_item_by_path(path, current_directory);

    if (item == NULL) {
        printf("FILE NOT FOUND\n");
        return;
    }
    printf("Size: %dB\n",item->size);
    printf("%s ", item->item_name);
    int cluster = item->start_cluster;

    // Validace clusteru
    if (cluster < 0 || cluster >= fs_description.cluster_count) {
        printf("INVALID START CLUSTER\n");
        return;
    }

    // Výpis clusterů
    while (cluster != FAT_FILE_END) {
        printf("%d", cluster);
        cluster = fat_table1[cluster];
        if (cluster != FAT_FILE_END) {
            printf(",");
        }
    }

    printf("\n");
}




void rm(const char *path) {
    DirectoryItem *target = find_item_by_path(path, current_directory);
    if (!target || !target->isFile) {
        fprintf(stderr, "Error: File '%s' not found or is not a file.\n", path);
        return;
    }

    int current_cluster = target->start_cluster;
    while (current_cluster != FAT_FILE_END) {
        int next_cluster = fat_table1[current_cluster];
        
        // Decrement reference count for the cluster
        decrement_cluster_reference(current_cluster);

        // If no references remain, free the cluster
        if (get_cluster_reference_count(current_cluster) == 0) {
            fat_table1[current_cluster] = FAT_UNUSED;
            if (fat_table2) fat_table2[current_cluster] = FAT_UNUSED;
            memset(&fs_data[current_cluster * fs_description.cluster_size], 0, fs_description.cluster_size);
        }

        current_cluster = next_cluster;
    }

    // Update the size of the parent directory
    DirectoryItem *parent = target->parent;
    if (parent) {
        for (int i = 0; i < parent->child_count; i++) {
            if (parent->children[i] == target) {
                parent->children[i] = parent->children[parent->child_count - 1];
                parent->children[parent->child_count - 1] = NULL;
                parent->child_count--;
                break;
            }
        }

        // Adjust the size of the parent directory
        parent->size -= target->size;
        if (parent->size < 0) {
            parent->size = 0;  // Ensure size doesn't go negative
        }

        
    }

    free(target);
    printf("File '%s' removed successfully.\n", path);
}



void rmdir(const char *path) {
    if (!path || strlen(path) == 0) {
        printf("INVALID PATH\n");
        return;
    }

    char *path_copy = strdup(path);
    if (!path_copy) {
        printf("MEMORY ALLOCATION ERROR\n");
        return;
    }

    DirectoryItem *target = current_directory;
    char *token = strtok(path_copy, "/");
    while (token) {
        DirectoryItem *next_dir = NULL;

        // Find the child directory
        for (int i = 0; i < target->child_count; i++) {
            if (target->children[i] && !target->children[i]->isFile &&
                strcmp(target->children[i]->item_name, token) == 0) {
                next_dir = target->children[i];
                break;
            }
        }

        if (!next_dir) {
            printf("FILE NOT FOUND\n");
            free(path_copy);
            return;
        }

        target = next_dir;
        token = strtok(NULL, "/");
    }

    // Ensure directory is empty
    if (target->child_count > 0) {
        printf("NOT EMPTY\n");
        free(path_copy);
        return;
    }

    // Free associated clusters
    int cluster = target->start_cluster;
    while (cluster != FAT_FILE_END) {
        int next_cluster = fat_table1[cluster];
        free_cluster(cluster);
        cluster = next_cluster;
    }

    // Remove directory from parent's child list
    DirectoryItem *parent = target->parent;
    if (parent) {
        for (int i = 0; i < parent->child_count; i++) {
            if (parent->children[i] == target) {
                parent->children[i] = parent->children[parent->child_count - 1];
                parent->children[parent->child_count - 1] = NULL;
                parent->child_count--;
                break;
            }
        }
    }

    free(target);
    free(path_copy);
    printf("OK\n");
}
void mkdir(const char *path) {
    if (!path || strlen(path) == 0 || strlen(path) >= MAX_ITEM_NAME_SIZE * MAX_CHILDREN) {
        printf("INVALID PATH\n");
        return;
    }

    char parts[MAX_CHILDREN][MAX_ITEM_NAME_SIZE];
    int part_count = 0;

    if (!split_path(path, parts, &part_count)) {
        printf("INVALID PATH\n");
        return;
    }

    DirectoryItem *current = (path[0] == '/') ? &root_directory : current_directory;

    for (int i = 0; i < part_count; i++) {
        DirectoryItem *existing_item = find_directory_item(parts[i]);
        if (existing_item) {
            if (i == part_count - 1) {
                printf("DIRECTORY OR FILE WITH NAME '%s' ALREADY EXISTS\n", parts[i]);
                return;
            } else {
                current = existing_item;
                continue;
            }
        }

        DirectoryItem *new_dir = calloc(1, sizeof(DirectoryItem));
        if (!new_dir) {
            printf("MEMORY ALLOCATION ERROR\n");
            return;
        }

        int cluster = allocate_cluster();
        if (cluster < 0) {
            printf("Error: Unable to allocate cluster for '%s'.\n", parts[i]);
            free(new_dir);
            return;
        }

        // Initialize new directory
        strcpy(new_dir->item_name, parts[i]);
        new_dir->isFile = false;
        new_dir->start_cluster = cluster;
        new_dir->size = 0;
        new_dir->child_count = 0;
        new_dir->parent = current;

        // Add to parent
        if (current->child_count >= MAX_CHILDREN) {
            printf("Error: Maximum number of children reached in '%s'.\n", current->item_name);
            free(new_dir);
            return;
        }
        current->children[current->child_count++] = new_dir;

        current = new_dir;
    }

    printf("Directory '%s' created successfully.\n", path);
}






void ls(const char *name) {
    if (current_directory == NULL) {
        printf("Error: Current directory is not initialized.\n");
        return;
    }

    DirectoryItem *target_directory = current_directory;

    // If a directory name is provided, find the target directory
    if (name != NULL && strlen(name) > 0) {
        bool found = false;
        for (int i = 0; i < current_directory->child_count; i++) {
            DirectoryItem *child = current_directory->children[i];
            if (child && !child->isFile && strcmp(child->item_name, name) == 0) {
                target_directory = child;
                found = true;
                break;
            }
        }

        if (!found) {
            printf("Error: Directory '%s' not found.\n", name);
            return;
        }
    }

    // Print the contents of the target directory
    printf("Contents of directory '%s':\n", target_directory->item_name);
    printf("%-13s %-5s %-10s\n", "Name", "Type", "Start Cluster");
    printf("---------------------------------------------------\n");

    if (target_directory->child_count == 0) {
        printf("Directory is empty.\n");
        return;
    }

    for (int i = 0; i < target_directory->child_count; i++) {
        DirectoryItem *child = target_directory->children[i];
        if (child) { // Validate the pointer before accessing
            printf("%-13s %-5s %-10d\n",
                   child->item_name,
                   child->isFile ? "File" : "Dir",
                   child->start_cluster);
        } else {
            printf("Error: Invalid directory entry at index %d.\n", i);
        }
    }
}




void cd(const char *path) {
    if (path == NULL || strlen(path) == 0) {
        printf("INVALID PATH\n");
        return;
    }

    DirectoryItem *target = (path[0] == '/') ? &root_directory : current_directory;

    char temp_path[MAX_ITEM_NAME_SIZE];
    strncpy(temp_path, path, MAX_ITEM_NAME_SIZE - 1);
    temp_path[MAX_ITEM_NAME_SIZE - 1] = '\0';

    char *token = strtok(temp_path, "/");
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
        } else if (strcmp(token, "..") == 0) {
            if (target->parent != NULL) {
                target = target->parent;
            } else {
                printf("NO PARENT DIRECTORY\n");
                return;
            }
        } else {
            DirectoryItem *next_dir = NULL;
            for (int i = 0; i < target->child_count; i++) {
                if (!target->children[i]->isFile && strcmp(target->children[i]->item_name, token) == 0) {
                    next_dir = target->children[i];
                    break;
                }
            }
            if (next_dir == NULL) {
                printf("DIRECTORY '%s' NOT FOUND\n", token);
                return;
            }
            target = next_dir;
        }
        token = strtok(NULL, "/");
    }

    current_directory = target;
    printf("OK\n");
}


void pwd() {
    if (current_directory == NULL) {
        printf("Current directory is NULL.\n");
        return;
    }

    char path[1024] = "";
    DirectoryItem *dir = current_directory;

    while (dir != NULL) {
        if (dir->item_name[0] == '\0') {  // Check for invalid directory names
            fprintf(stderr, "Invalid directory name detected.\n");
            break;
        }

        char temp[256];
        if (snprintf(temp, sizeof(temp), "/%s", dir->item_name) >= (int)sizeof(temp)) {
            fprintf(stderr, "Path segment too long: %s\n", dir->item_name);
            break;
        }

        char new_path[1024];
        if (snprintf(new_path, sizeof(new_path), "%s%s", temp, path) >= (int)sizeof(new_path)) {
            fprintf(stderr, "Path too long.\n");
            break;
        }

        strncpy(path, new_path, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';

        dir = dir->parent;  // Move to the parent directory
    }

    printf("%s\n", path[0] ? path : "/");
}

void check() {

    // File and directory integrity check
    for (int i = 0; i < current_directory->child_count; i++) {
        DirectoryItem *item = current_directory->children[i];

        if (item->isFile) {
            int cluster = item->start_cluster;
            int cluster_count = 0;
            bool corrupted = false;

            while (cluster != FAT_FILE_END) {
                if (cluster < 0 || cluster >= fs_description.cluster_count) {
                    printf("Error: File '%s' has an invalid cluster (%d).\n", item->item_name, cluster);
                    corrupted = true;
                    break;
                }
                cluster = fat_table1[cluster];
                cluster_count++;
            }

            int expected_size = cluster_count * fs_description.cluster_size;
            if (!corrupted && expected_size < item->size) {
                printf("Error: File '%s' has an incorrect size. Actual: %d, Expected: %d.\n", 
                       item->item_name, item->size, expected_size);
                corrupted = true;
            }

            if (!corrupted) {
                printf("File '%s' is intact.\n", item->item_name);
            }

        } else { // Directory check
            if (item->start_cluster < 0 || item->start_cluster >= fs_description.cluster_count) {
                printf("Error: Directory '%s' has an invalid start cluster (%d).\n", item->item_name, item->start_cluster);
            } else {
                printf("Directory '%s' is intact.\n", item->item_name);
            }
        }
    }

    printf("Filesystem check completed.\n");
}

void bug(const char *name) {
    printf("Corrupting filesystem...\n");

    for (int i = 0; i < current_directory->child_count; i++) {
        DirectoryItem *item = current_directory->children[i];
        if (strcmp(item->item_name, name) == 0) {
            if (item->isFile) {
                int original_cluster = fat_table1[item->start_cluster];
                fat_table1[item->start_cluster] = -999; // Corrupt FAT entry

                item->start_cluster = -999; // Corrupt start_cluster

                printf("File '%s' has been corrupted. Original value: %d.\n", name, original_cluster);
            } else {
                int original_cluster = item->start_cluster;
                item->start_cluster = -999;
                item->child_count = -1; // Corrupt child count

                printf("Directory '%s' has been corrupted. Original cluster value: %d.\n", name, original_cluster);
            }
            return;
        }
    }

    printf("Error: File or directory '%s' not found.\n", name);
}

void incp(const char *source, const char *destination) {
    FILE *source_file = fopen(source, "rb");
    if (!source_file) {
        printf("FILE NOT FOUND: '%s' cannot be opened or does not exist.\n", source);
        return;
    }

    fseek(source_file, 0, SEEK_END);
    size_t source_size = ftell(source_file);
    fseek(source_file, 0, SEEK_SET);

    char path[256], file_name[MAX_ITEM_NAME_SIZE];
    const char *last_slash = strrchr(destination, '/');
    if (last_slash) {
        strncpy(path, destination, last_slash - destination);
        path[last_slash - destination] = '\0';
        strncpy(file_name, last_slash + 1, sizeof(file_name) - 1);
        file_name[sizeof(file_name) - 1] = '\0';
    } else {
        strcpy(path, ".");
        strncpy(file_name, destination, sizeof(file_name) - 1);
        file_name[sizeof(file_name) - 1] = '\0';
    }

    DirectoryItem *dest_dir = find_item_by_path(path, &root_directory);
    
    if (!dest_dir || dest_dir->isFile) {
        printf("PATH NOT FOUND: '%s' is not a directory or does not exist.\n", path);
        fclose(source_file);
        return;
    }

    if (dest_dir->child_count >= MAX_CHILDREN) {
        printf("ERROR: Directory '%s' has too many items.\n", path);
        fclose(source_file);
        return;
    }
    

    DirectoryItem *existing_item = NULL;
    for (int i = 0; i < dest_dir->child_count; i++) {
        if (strcmp(dest_dir->children[i]->item_name, file_name) == 0) {
            existing_item = dest_dir->children[i];
            break;
        }
    }

    if (existing_item) {
        printf("ERROR: File '%s' already exists in '%s'.\n", file_name, path);
        fclose(source_file);
        return;
    }

    int32_t start_cluster = FAT_UNUSED;
    int32_t previous_cluster = FAT_UNUSED;
    size_t size_remaining = source_size;
    char buffer[fs_description.cluster_size];

    while (size_remaining > 0) {
        int32_t free_cluster = allocate_cluster();
        if (free_cluster == FAT_UNUSED) {
            printf("Error: Not enough disk space.\n");
            fclose(source_file);
            return;
        }

        if (previous_cluster != FAT_UNUSED) {
            fat_table1[previous_cluster] = free_cluster;
        } else {
            start_cluster = free_cluster;
        }
        previous_cluster = free_cluster;
        fat_table1[free_cluster] = FAT_FILE_END;

        size_t bytes_to_copy = size_remaining < (size_t)fs_description.cluster_size ? size_remaining : (size_t)fs_description.cluster_size;
        size_t bytes_read = fread(buffer, 1, bytes_to_copy, source_file);
        if (bytes_read != bytes_to_copy) {
            if (feof(source_file)) {
                printf("End of file reached.\n");
            } else {
                fclose(source_file);
                return;
            }
        }

        write_cluster_data(free_cluster, buffer, bytes_read);
        increment_cluster_reference(free_cluster); // Zvýšení reference na cluster
        size_remaining -= bytes_read;
    }

    fclose(source_file);

    DirectoryItem *new_item = malloc(sizeof(DirectoryItem));
    if (!new_item) {
        printf("Error: Failed to allocate memory for the new file.\n");
        return;
    }

    strncpy(new_item->item_name, file_name, MAX_ITEM_NAME_SIZE);
    new_item->isFile = true;
    new_item->size = source_size;
    new_item->start_cluster = start_cluster;
    new_item->parent = dest_dir;
    new_item->child_count = 0;

    dest_dir->children[dest_dir->child_count++] = new_item;
    printf("File '%s' was successfully copied to '%s'.\n", file_name, path);
}

void outcp(const char *source_path, const char *destination_path) {
    DirectoryItem *source_item = find_item_by_path(source_path, current_directory);

    if (!source_item) {
        printf("FILE NOT FOUND\n");
        return;
    }

    if (!source_item->isFile) {
        printf("SOURCE IS NOT A FILE\n");
        return;
    }

    FILE *dest_file = fopen(destination_path, "wb");
    if (!dest_file) {
        perror("PATH NOT FOUND");
        return;
    }

    int32_t cluster = source_item->start_cluster;
    int32_t bytes_remaining = source_item->size;

    while (cluster != FAT_FILE_END && bytes_remaining > 0) {
        int32_t bytes_to_read = (bytes_remaining < fs_description.cluster_size) 
                                ? bytes_remaining 
                                : fs_description.cluster_size;

        void *buffer = read_cluster_data(cluster, bytes_to_read);
        if (!buffer) {
            fprintf(stderr, "Error reading cluster %d.\n", cluster);
            fclose(dest_file);
            return;
        }

        size_t bytes_written = fwrite(buffer, 1, bytes_to_read, dest_file);
        if (bytes_written != (size_t)bytes_to_read) {
            fprintf(stderr, "Error writing to destination file.\n");
            free(buffer);
            fclose(dest_file);
            return;
        }

        free(buffer);
        cluster = fat_table1[cluster];
        bytes_remaining -= bytes_to_read;
    }

    fclose(dest_file);
    printf("OK\n");
}



void cat(const char *source_path) {
    DirectoryItem *source_item = find_item_by_path(source_path, current_directory);

    if (!source_item) {
        printf("FILE NOT FOUND\n");
        return;
    }

    if (source_item->isFile == false) {
        printf("SOURCE IS NOT A FILE\n");
        return;
    }

    int32_t cluster = source_item->start_cluster;
    int32_t bytes_remaining = source_item->size;

    while (cluster != FAT_FILE_END && bytes_remaining > 0) {
        int32_t bytes_to_read = (bytes_remaining < fs_description.cluster_size) ? bytes_remaining : fs_description.cluster_size;

        void *buffer = read_cluster_data(cluster, bytes_to_read);
        if (!buffer) {
            fprintf(stderr, "Error reading cluster %d.\n", cluster);
            return;
        }

        fwrite(buffer, 1, bytes_to_read, stdout);

        free(buffer);

        cluster = fat_table1[cluster];
        bytes_remaining -= bytes_to_read;
    }

    printf("\n");
}



void load(const char *filename, const char *source_path) {
    if (!filename || !source_path) {
        printf("Error: Invalid arguments provided to 'load'.\n");
        return;
    }

    FILE *file = fopen(source_path, "r");
    if (!file) {
        printf("FILE NOT FOUND\n");
        return;
    }

    char command_buffer[512];
    int line_number = 0;
    int error_count = 0;

    while (fgets(command_buffer, sizeof(command_buffer), file)) {
        // Remove trailing newline character
        size_t len = strlen(command_buffer);
        if (len > 0 && command_buffer[len - 1] == '\n') {
            command_buffer[len - 1] = '\0';
        }

        line_number++;
        process_error = false; // Reset error flag before each command
        process_command(filename, command_buffer);

        // Check for errors
        if (process_error) {
            printf("Error processing command on line %d: %s\n", line_number, command_buffer);
            error_count++;
        }
    }

    fclose(file);

    if (error_count > 0) {
        printf("Load completed with %d errors.\n", error_count);
    } else {
        printf("OK\n");
    }
}















