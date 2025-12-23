#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

void print_size(uint32_t size)
{
    if (size < 1024)
        printf("%u B", size);
    else if (size < 1024 * 1024)
        printf("%.2f KB", size / 1024.0f);
    else if (size < 1024 * 1024 * 1024)
        printf("%.2f MB", size / (1024.0f * 1024.0f));
    else
        printf("%.2f GB", size / (1024.0f * 1024.0f * 1024.0f));
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
        fprintf(stderr, "Failed to mount volume. Error code: %s\n",
            fs_strerror(result));
        exit(1);
    }

    uint32_t vol_size = drive_size_bytes / 1024;
    uint32_t block_size = volume.super_block.data_block_size;
    uint32_t block_count = volume.super_block.data_block_count;
    uint32_t flt_size = block_count * sizeof(FS_FLTEntry);

    printf("Created volume:\n Volume Size: ");
    print_size(vol_size * 1024);
    printf("\n Data Block Size: ");
    print_size(block_size);
    printf("\n Data Block Count: %u\n", block_count);
    printf(" FLT Size: ");
    print_size(flt_size);
    printf("\n");

    fs_volume_unmount(&volume);
}

void delete_volume(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s delete_volume <path>\n", argv[0]);
        return;
    }

    const char* path = argv[2];
    if (unlink(path) != 0)
    {
        fprintf(stderr, "Failed to delete volume at %s\n", path);
    }
    else
    {
        printf("Volume at %s deleted successfully\n", path);
    }
}

void put_file(int argc, char** argv)
{
    (void)argc;
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
    (void)argc;
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
    (void)argc;
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
    (void)argc;
    const char* volume_path = argv[2];

    // Mount the volume
    FS_Volume volume;
    int result = fs_volume_mount(&volume, volume_path);
    if (result != FS_RESULT_OK)
    {
        fprintf(stderr, "Failed to mount volume: %s\n", fs_strerror(result));
        return;
    }

    for (;;)
    {
        FS_FileInfo file_info;
        result = fs_list_get(&volume, &file_info);
        if (result != FS_RESULT_OK && result != FS_RESULT_NOT_FOUND)
        {
            fprintf(
                stderr, "Failed to list files: %s\n", fs_strerror(result));
            break;
        }
        printf(" * %s (", file_info.name);
        print_size(file_info.size_bytes);
        printf(")\n");
        if (result == FS_RESULT_NOT_FOUND) break;
    }

    // Cleanup
    fs_volume_unmount(&volume);
}

uint32_t map_random_color(void)
{
    float hue = rand() % 10 / 9.0f;
    float saturation = 0.5f + (rand() % 3) * 0.25f;
    float brightness = 1.0f;
    // Convert HSV to RGB
    float c = brightness * saturation;
    float x = c * (1 - fabsf(fmodf(hue * 6, 2) - 1));
    float m = brightness - c;
    float r, g, b;
    if (hue < 1.0f / 6)
    {
        r = c;
        g = x;
        b = 0;
    }
    else if (hue < 2.0f / 6)
    {
        r = x;
        g = c;
        b = 0;
    }
    else if (hue < 3.0f / 6)
    {
        r = 0;
        g = c;
        b = x;
    }
    else if (hue < 4.0f / 6)
    {
        r = 0;
        g = x;
        b = c;
    }
    else if (hue < 5.0f / 6)
    {
        r = x;
        g = 0;
        b = c;
    }
    else
    {
        r = c;
        g = 0;
        b = x;
    }
    uint8_t R = (uint8_t)((r + m) * 255);
    uint8_t G = (uint8_t)((g + m) * 255);
    uint8_t B = (uint8_t)((b + m) * 255);
    return (R << 16) | (G << 8) | B;
}

void dump_map(int argc, char** argv)
{
    (void)argc;
    const char* volume_path = argv[2];

    // Mount the volume
    FS_Volume volume;
    int result = fs_volume_mount(&volume, volume_path);
    if (result != FS_RESULT_OK)
    {
        fprintf(stderr, "Failed to mount volume: %s\n", fs_strerror(result));
        return;
    }

    // Create the FLT dump
    FS_FLTDump dump;
    result = fs_flt_dump_create(&volume, &dump);
    if (result != FS_RESULT_OK)
    {
        fprintf(
            stderr, "Failed to create FLT dump: %s\n", fs_strerror(result));
        fs_volume_unmount(&volume);
        return;
    }

    // Give each entry a color
    uint32_t* colors = malloc(dump.file_count * sizeof(uint32_t));
    for (int i = 0; i < dump.file_count; i++) colors[i] = map_random_color();

    // Print the map
    for (uint32_t i = 0; i < volume.super_block.flt_entry_count; i++)
    {
        uint32_t entry = dump.entries[i];
        if (entry == 0)
            printf(".");  // Free block
        else
            printf("\x1b[38;2;%u;%u;%um#\x1b[0m",
                (colors[entry - 1] >> 16) & 0xFF,
                (colors[entry - 1] >> 8) & 0xFF, colors[entry - 1] & 0xFF);
        if ((i + 1) % 64 == 0) printf("\n");
    }
    printf("\n");

    // Print the file legend
    printf("File Legend:\n");
    for (int i = 0; i < dump.file_count; i++)
    {
        printf(" * \x1b[38;2;%u;%u;%um%s\x1b[0m (",
            (colors[i] >> 16) & 0xFF, (colors[i] >> 8) & 0xFF,
            colors[i] & 0xFF, dump.file_names[i]);
        print_size(dump.file_sizes[i]);
        printf(")\n");
    }

    // Cleanup
    free(colors);
    fs_flt_dump_free(&dump);
    fs_volume_unmount(&volume);
}

int main(int argc, char** argv)
{
    srand((unsigned int)time(NULL));

    if (argc < 2) print_help_and_exit(argv[0]);
    if (strcmp(argv[1], "create_volume") == 0)
        create_volume(argc, argv);
    else if (strcmp(argv[1], "delete_volume") == 0)
        delete_volume(argc, argv);
    else if (strcmp(argv[1], "ls") == 0)
        ls_files(argc, argv);
    else if (strcmp(argv[1], "put") == 0)
        put_file(argc, argv);
    else if (strcmp(argv[1], "get") == 0)
        get_file(argc, argv);
    else if (strcmp(argv[1], "rm") == 0)
        rm_file(argc, argv);
    else if (strcmp(argv[1], "dump_map") == 0)
        dump_map(argc, argv);
    else if (strcmp(argv[1], "help") == 0)
        print_help_and_exit(argv[0]);
    else
        print_help_and_exit(argv[0]);
}
