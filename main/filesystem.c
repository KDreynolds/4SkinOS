#include "include/filesystem.h"
#include <string.h>
#include <stdio.h>
#include "esp_system.h"
#include <inttypes.h>

static File files[MAX_FILES];
static int num_files = 0;
static int current_dir = 0;
static char current_path[MAX_PATH_LENGTH] = "/";

void fs_init(void) {
    memset(files, 0, sizeof(files));
    num_files = 0;
    // Create root directory
    strcpy(files[0].name, "/");
    files[0].is_dir = true;
    files[0].parent_dir = -1;
    num_files = 1;  // Set to 1 to account for the root directory
    current_dir = 0;  // Set current directory to root
    strcpy(current_path, "/");
    printf("Filesystem initialized. Root directory created.\n");
}

static int find_file(const char* path) {
    char temp_path[MAX_PATH_LENGTH];
    strcpy(temp_path, path);
    char* token = strtok(temp_path, "/");
    int current = current_dir;

    while (token != NULL) {
        bool found = false;
        for (int i = 0; i < num_files; i++) {
            if (files[i].parent_dir == current && strcmp(files[i].name, token) == 0) {
                current = i;
                found = true;
                break;
            }
        }
        if (!found) return -1;
        token = strtok(NULL, "/");
    }
    return current;
}

bool fs_create_file(const char* path) {
    if (num_files >= MAX_FILES) {
        printf("Maximum number of files reached.\n");
        return false;
    }

    char full_path[MAX_PATH_LENGTH];
    if (path[0] == '/') {
        strncpy(full_path, path, MAX_PATH_LENGTH - 1);
        full_path[MAX_PATH_LENGTH - 1] = '\0';
    } else {
        size_t current_path_len = strlen(current_path);
        size_t path_len = strlen(path);
        if (current_path_len + path_len + 2 > MAX_PATH_LENGTH) {
            printf("Path too long\n");
            return false;
        }
        strcpy(full_path, current_path);
        if (full_path[current_path_len - 1] != '/') {
            strcat(full_path, "/");
        }
        strcat(full_path, path);
    }

    char* last_slash = strrchr(full_path, '/');
    if (!last_slash) return false;

    *last_slash = '\0';
    char* file_name = last_slash + 1;

    int parent_dir = (full_path[0] == '\0') ? 0 : find_file(full_path);
    if (parent_dir == -1) return false;

    // Check if file already exists
    for (int i = 0; i < num_files; i++) {
        if (files[i].parent_dir == parent_dir && strcmp(files[i].name, file_name) == 0 && !files[i].is_dir) {
            return false;  // File already exists
        }
    }

    strcpy(files[num_files].name, file_name);
    files[num_files].is_dir = false;
    files[num_files].parent_dir = parent_dir;
    files[num_files].size = 0;
    num_files++;
    printf("File created: %s\n", path);
    return true;
}

bool fs_write_file(const char* filename, const uint8_t* content, uint32_t size) {
    char full_path[MAX_PATH_LENGTH];
    
    if (filename[0] != '/') {
        size_t current_path_len = strlen(current_path);
        size_t filename_len = strlen(filename);
        size_t separator_len = (current_path[current_path_len - 1] == '/') ? 0 : 1;
        
        if (current_path_len + separator_len + filename_len >= MAX_PATH_LENGTH) {
            printf("Path too long\n");
            return false;
        }
        
        strcpy(full_path, current_path);
        if (separator_len) {
            strcat(full_path, "/");
        }
        strcat(full_path, filename);
    } else {
        if (strlen(filename) >= MAX_PATH_LENGTH) {
            printf("Path too long\n");
            return false;
        }
        strcpy(full_path, filename);
    }

    int file_index = find_file(full_path);
    if (file_index == -1) {
        // File doesn't exist, create it
        if (!fs_create_file(full_path)) {
            printf("Failed to create file: %s\n", full_path);
            return false;
        }
        file_index = find_file(full_path);
    }

    if (file_index == -1 || files[file_index].is_dir) {
        printf("Invalid file: %s\n", full_path);
        return false;
    }

    if (size > MAX_FILE_SIZE) {
        printf("Content too large for file: %s\n", full_path);
        return false;
    }

    memcpy(files[file_index].data, content, size);
    files[file_index].size = size;
    printf("Content written to file: %s (%" PRIu32 " bytes)\n", full_path, size);
    return true;
}

bool fs_read_file(const char* path, uint8_t* data, uint32_t* size) {
    int file_index = find_file(path);
    if (file_index == -1 || files[file_index].is_dir) return false;

    memcpy(data, files[file_index].data, files[file_index].size);
    *size = files[file_index].size;
    return true;
}

bool fs_delete_file(const char* path) {
    int file_index = find_file(path);
    if (file_index == -1) {
        printf("File or directory not found: %s\n", path);
        return false;
    }

    uint32_t deleted_size = files[file_index].size;  // Store the size before deletion

    if (files[file_index].is_dir) {
        // Check if directory is empty
        for (int i = 0; i < num_files; i++) {
            if (files[i].parent_dir == file_index) {
                printf("Cannot delete non-empty directory: %s\n", path);
                return false;
            }
        }
    }

    // Shift all files after the deleted one
    for (int i = file_index; i < num_files - 1; i++) {
        files[i] = files[i + 1];
    }
    num_files--;

    return true;
}

void fs_list_files(const char* path) {
    int dir_index = (strcmp(path, ".") == 0 || strlen(path) == 0) ? current_dir : find_file(path);

    if (dir_index == -1 || !files[dir_index].is_dir) {
        printf("Invalid directory: %s\n", path);
        return;
    }

    bool empty = true;
    for (int i = 0; i < num_files; i++) {
        if (files[i].parent_dir == dir_index) {
            empty = false;
            printf("%s%s", files[i].name, files[i].is_dir ? "/" : "");
            if (!files[i].is_dir) {
                printf(" (%" PRIu32 " bytes)", files[i].size);
            }
        }
    }
    if (empty) {
        printf("(empty)");
    }
}

bool fs_change_dir(const char* path) {
    if (strcmp(path, "/") == 0) {
        current_dir = 0;
        strcpy(current_path, "/");
        return true;
    }

    if (strcmp(path, "..") == 0) {
        if (current_dir != 0) {
            current_dir = files[current_dir].parent_dir;
            char* last_slash = strrchr(current_path, '/');
            if (last_slash != current_path) {
                *last_slash = '\0';
            } else {
                *(last_slash + 1) = '\0';
            }
        }
        return true;
    }

    int dir_index = find_file(path);
    if (dir_index == -1 || !files[dir_index].is_dir) {
        printf("Invalid directory: %s\n", path);
        return false;
    }

    current_dir = dir_index;

    // Update current_path.
    if (path[0] == '/') {
        strncpy(current_path, path, MAX_PATH_LENGTH - 1);
    } else {
        if (strcmp(current_path, "/") != 0) {
            strncat(current_path, "/", MAX_PATH_LENGTH - strlen(current_path) - 1);
        }
        strncat(current_path, path, MAX_PATH_LENGTH - strlen(current_path) - 1);
    }

    return true;
}

void fs_print_working_dir(char* buffer) {
    strcpy(buffer, current_path);
}

bool fs_make_dir(const char* path) {
    if (num_files >= MAX_FILES) {
        printf("Maximum number of files reached.\n");
        return false;
    }

    char full_path[MAX_PATH_LENGTH];
    if (path[0] == '/') {
        strncpy(full_path, path, MAX_PATH_LENGTH - 1);
        full_path[MAX_PATH_LENGTH - 1] = '\0';
    } else {
        size_t current_path_len = strlen(current_path);
        size_t path_len = strlen(path);
        if (current_path_len + path_len + 2 > MAX_PATH_LENGTH) {
            printf("Path too long\n");
            return false;
        }
        strcpy(full_path, current_path);
        if (full_path[current_path_len - 1] != '/') {
            strcat(full_path, "/");
        }
        strcat(full_path, path);
    }

    char* last_slash = strrchr(full_path, '/');
    if (!last_slash) return false;

    *last_slash = '\0';
    char* dir_name = last_slash + 1;

    int parent_dir = (full_path[0] == '\0') ? 0 : find_file(full_path);
    if (parent_dir == -1) return false;

    // Check if directory already exists
    for (int i = 0; i < num_files; i++) {
        if (files[i].parent_dir == parent_dir && strcmp(files[i].name, dir_name) == 0 && files[i].is_dir) {
            return false;  // Directory already exists
        }
    }

    strcpy(files[num_files].name, dir_name);
    files[num_files].is_dir = true;
    files[num_files].parent_dir = parent_dir;
    files[num_files].size = 0;
    num_files++;
    return true;
}