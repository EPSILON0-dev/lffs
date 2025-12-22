#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fs.h"

void print_help_and_exit(const char* prog_name)
{
    printf("Usage: %s <command> [options]\n", prog_name);
    printf("Commands:\n");
    printf(
        "  create_volume <path> <block_size> <bytes>  Create a new volume\n");
    printf("  delete_volume <path>                       Delete a volume\n");
    printf(
        "  ls <volume>                                List files in "
        "volume\n");
    printf(
        "  put <volume> <src> <dst>                   Put a file into the "
        "volume\n");
    printf(
        "  get <volume> <src> <dst>                   Get a file from the "
        "volume\n");
    printf(
        "  rm <volume> <filename>                     Remove a file from the "
        "volume\n");
    exit(1);
}

void create_volume(int argc, char** argv)
{
    if (argc < 5)
    {
        fprintf(stderr,
            "Usage: %s create <path> <data_block_size> <drive_size_bytes>\n",
            argv[0]);
        return;
    }

    const char* path = argv[2];
    uint32_t data_block_size = (uint32_t)atoi(argv[3]);
    uint32_t drive_size_bytes = (uint32_t)atoi(argv[4]);

    int result = fs_volume_create(path, data_block_size, drive_size_bytes);
    if (result != FS_RESULT_OK)
        fprintf(stderr, "Failed to create volume. Error code: %s\n",
            fs_strerror(result));
    else
        printf("Volume created successfully at %s\n", path);

    FS_Volume volume;
    result = fs_volume_mount(&volume, path);
    if (result != FS_RESULT_OK)
    {
        fprintf(stderr, "Failed to mount volume. Error code: %s\n", fs_strerror(result));
        exit(1);
    }

    uint32_t vol_size = drive_size_bytes / 1024;
    uint32_t block_size = volume.super_block.data_block_size;
    uint32_t block_count = volume.super_block.data_block_count;
    uint32_t flt_size = block_count / 1024 * sizeof(FS_FLTEntry);

    printf("Created volume:\n");
    printf(" Volume Size: %u KB\n", vol_size);
    printf(" Data Block Size: %u bytes\n", block_size);
    printf(" Data Block Count: %u\n", block_count);
    printf(" FLT Size: %u KB\n", flt_size);

    fs_volume_unmount(&volume);
}

void put_file(int argc, char** argv)
{
    const char* volume_path = argv[2];
    const char* source_file_path = argv[3];
    const char* dest_file_name = argv[4];

    // Mount the volume
    FS_Volume volume;
    int result = fs_volume_mount(&volume, volume_path);
    if (result != FS_RESULT_OK)
    {
        fprintf(stderr, "Failed to mount volume: %s\n", fs_strerror(result));
        return;
    }

    // Open the source file
    FILE* source_file = fopen(source_file_path, "rb");
    if (!source_file)
    {
        fprintf(stderr, "Failed to open source file: %s\n", source_file_path);
        fs_volume_unmount(&volume);
        return;
    }

    // Get the file size and read the data
    fseek(source_file, 0, SEEK_END);
    uint32_t file_size = ftell(source_file);
    fseek(source_file, 0, SEEK_SET);
    void* buffer = malloc(file_size);
    fread(buffer, 1, file_size, source_file);

    // Write the file to the volume
    uint32_t bytes_written;
    result = fs_file_write(
        &volume, dest_file_name, buffer, file_size, &bytes_written);
    if (result != FS_RESULT_OK)
    {
        fprintf(stderr, "Failed to write file to volume: %s\n",
            fs_strerror(result));
    }
    else
    {
        printf("Wrote %u bytes to %s in volume %s\n", bytes_written,
            dest_file_name, volume_path);
    }

    // Cleanup
    free(buffer);
    fs_volume_unmount(&volume);
    fclose(source_file);
}

void get_file(int argc, char** argv)
{
    const char* volume_path = argv[2];
    const char* source_file_path = argv[3];
    const char* dest_file_name = argv[4];

    // Mount the volume
    FS_Volume volume;
    int result = fs_volume_mount(&volume, volume_path);
    if (result != FS_RESULT_OK)
    {
        fprintf(stderr, "Failed to mount volume: %s\n", fs_strerror(result));
        return;
    }

    // Read the file from the volume
    uint32_t file_size;
    result = fs_file_size(&volume, source_file_path, &file_size);
    if (result != FS_RESULT_OK)
    {
        fprintf(stderr, "Failed to get file size: %s\n", fs_strerror(result));
        fs_volume_unmount(&volume);
        return;
    }
    void* buffer = malloc(file_size);
    uint32_t bytes_read;
    result = fs_file_read(
        &volume, source_file_path, buffer, file_size, &bytes_read);
    if (result != FS_RESULT_OK)
    {
        fprintf(stderr, "Failed to read file from volume: %s\n",
            fs_strerror(result));
        free(buffer);
        fs_volume_unmount(&volume);
        return;
    }

    // Write the data to the destination file
    FILE* dest_file = fopen(dest_file_name, "wb");
    if (!dest_file)
    {
        fprintf(
            stderr, "Failed to open destination file: %s\n", dest_file_name);
        free(buffer);
        fs_volume_unmount(&volume);
        return;
    }
    fwrite(buffer, 1, bytes_read, dest_file);
    printf("Read %u bytes from %s in volume %s\n", bytes_read,
        source_file_path, volume_path);

    // Cleanup
    free(buffer);
    fs_volume_unmount(&volume);
    fclose(dest_file);
}

void rm_file(int argc, char** argv)
{
    const char* volume_path = argv[2];
    const char* file_name = argv[3];

    // Mount the volume
    FS_Volume volume;
    int result = fs_volume_mount(&volume, volume_path);
    if (result != FS_RESULT_OK)
    {
        fprintf(stderr, "Failed to mount volume: %s\n", fs_strerror(result));
        return;
    }

    // Delete the file from the volume
    result = fs_file_delete(&volume, file_name);
    if (result != FS_RESULT_OK)
    {
        fprintf(stderr, "Failed to delete file from volume: %s\n",
            fs_strerror(result));
    }
    else
    {
        printf("Deleted %s from volume %s\n", file_name, volume_path);
    }

    // Cleanup
    fs_volume_unmount(&volume);
}

void ls_files(int argc, char** argv)
{
    const char* volume_path = argv[2];

    // Mount the volume
    FS_Volume volume;
    int result = fs_volume_mount(&volume, volume_path);
    if (result != FS_RESULT_OK)
    {
        fprintf(stderr, "Failed to mount volume: %s\n", fs_strerror(result));
        return;
    }

    // List files in the volume
    uint32_t file_count;
    result = fs_list_len(&volume, &file_count);
    if (result != FS_RESULT_OK)
    {
        fprintf(stderr, "Failed to get file list length: %s\n",
            fs_strerror(result));
        fs_volume_unmount(&volume);
        return;
    }

    printf("Files in volume %s:\n", volume_path);
    for (uint32_t i = 0; i < file_count; i++)
    {
        FS_FileInfo info;
        result = fs_list_get(&volume, &info);
        if (result != FS_RESULT_OK)
        {
            fprintf(
                stderr, "Failed to get file info: %s\n", fs_strerror(result));
            break;
        }
        printf(" - %s (%u bytes)\n", info.name, info.size_bytes);
    }

    // Cleanup
    fs_volume_unmount(&volume);
}

int main(int argc, char** argv)
{
    if (argc < 2) print_help_and_exit(argv[0]);
    if (strcmp(argv[1], "create_volume") == 0)
        create_volume(argc, argv);
    else if (strcmp(argv[1], "delete_volume") == 0)
        unlink(argv[2]);
    else if (strcmp(argv[1], "ls") == 0)
        ls_files(argc, argv);
    else if (strcmp(argv[1], "put") == 0)
        put_file(argc, argv);
    else if (strcmp(argv[1], "get") == 0)
        get_file(argc, argv);
    else if (strcmp(argv[1], "rm") == 0)
        rm_file(argc, argv);
    else
        print_help_and_exit(argv[0]);
}
