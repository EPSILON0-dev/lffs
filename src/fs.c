#include "fs.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

FS_FUN_INLINE void __DBGTRAP(void)
{
    (void)0;
}

#define FWDERR(x)                  \
    {                              \
        int __res = (x);           \
        if (__res != FS_RESULT_OK) \
        {                          \
            __DBGTRAP();           \
            return __res;          \
        }                          \
    }

#define FWDIOERR(x)                    \
    {                                  \
        int __res = (x);               \
        if (__res != FS_RESULT_OK)     \
        {                              \
            __DBGTRAP();               \
            return FS_RESULT_IO_ERROR; \
        }                              \
    }

FS_FUN_INLINE int fs_generic_read(
    FILE* f, uint32_t offset, void* buff, uint32_t len)
{
    FWDIOERR(fseek(f, offset, SEEK_SET));
    FWDIOERR(fread(buff, 1, len, f) != len);
    return FS_RESULT_OK;
}

FS_FUN_INLINE int fs_generic_write(
    FILE* f, uint32_t offset, const void* buff, uint32_t len)
{
    FWDIOERR(fseek(f, offset, SEEK_SET));
    FWDIOERR(fwrite(buff, 1, len, f) != len);
    return FS_RESULT_OK;
}

FS_FUN_INLINE int fs_read_superblock(FS_Volume* vol, FS_SuperBlock* sb)
{
    FWDIOERR(fs_generic_read(
        vol->file_handle, 0, (uint8_t*)sb, sizeof(FS_SuperBlock)));
    return FS_RESULT_OK;
}

FS_FUN_INLINE int fs_read_flt(
    FS_Volume* vol, uint32_t index, FS_FLTEntry* value)
{
    const uint32_t offset =
        vol->super_block.flt_offset + index * sizeof(FS_FLTEntry);
    FWDIOERR(fs_generic_read(
        vol->file_handle, offset, value, sizeof(FS_FLTEntry)));
    return FS_RESULT_OK;
}

FS_FUN_INLINE int fs_write_flt(
    FS_Volume* vol, uint32_t index, FS_FLTEntry value)
{
    const uint32_t offset =
        vol->super_block.flt_offset + index * sizeof(FS_FLTEntry);
    FWDIOERR(fs_generic_write(
        vol->file_handle, offset, &value, sizeof(FS_FLTEntry)));
    return FS_RESULT_OK;
}

FS_FUN_INLINE int fs_read_block(
    FS_Volume* vol, uint32_t index, void* buff, uint32_t bytes)
{
    const uint32_t offset = vol->super_block.data_block_offset +
                            index * vol->super_block.data_block_size;
    FWDIOERR(fs_generic_read(vol->file_handle, offset, buff, bytes));
    return FS_RESULT_OK;
}

FS_FUN_INLINE int fs_write_block(
    FS_Volume* vol, uint32_t index, const void* buff, uint32_t bytes)
{
    const uint32_t offset = vol->super_block.data_block_offset +
                            index * vol->super_block.data_block_size;
    FWDIOERR(fs_generic_write(vol->file_handle, offset, buff, bytes));
    return FS_RESULT_OK;
}

FS_FUN_INLINE int fs_clear_block(FS_Volume* vol, uint32_t index)
{
    const uint32_t erase_data = 0xffffffff;
    const uint32_t offset = vol->super_block.data_block_offset +
                            index * vol->super_block.data_block_size;
    for (uint32_t i = 0; i < vol->super_block.data_block_size; i += 4)
        FWDIOERR(fs_generic_write(vol->file_handle, offset, &erase_data, 4));
    return FS_RESULT_OK;
}

FS_FUN_INLINE int fs_read_fileentry(
    FS_Volume* vol, uint32_t block, uint32_t index, FS_FileEntry* entry)
{
    uint32_t offset = vol->super_block.data_block_offset +
                      block * vol->super_block.data_block_size +
                      index * sizeof(FS_FileEntry);
    FWDIOERR(fs_generic_read(
        vol->file_handle, offset, entry, sizeof(FS_FileEntry)));
    return FS_RESULT_OK;
}

FS_FUN_INLINE int fs_write_fileentry(
    FS_Volume* vol, uint32_t block, uint32_t index, FS_FileEntry* entry)
{
    uint32_t offset = vol->super_block.data_block_offset +
                      block * vol->super_block.data_block_size +
                      index * sizeof(FS_FileEntry);
    FWDIOERR(fs_generic_write(
        vol->file_handle, offset, entry, sizeof(FS_FileEntry)));
    return FS_RESULT_OK;
}

FS_FUN_INTERNAL int fs_count_used_blocks(
    FS_Volume* vol, uint32_t* used_blocks)
{
    uint32_t total_blocks = vol->super_block.flt_entry_count;
    uint32_t count = 0;

    for (uint32_t i = 0; i < total_blocks; i++)
    {
        uint32_t flt_entry;
        FWDIOERR(fs_read_flt(vol, i, &flt_entry));
        if (flt_entry != FS_FLT_FREE && flt_entry != FS_FLT_DELETED) count++;
    }

    *used_blocks = count;
    return FS_RESULT_OK;
}

FS_FUN_INTERNAL int fs_find_free_block(FS_Volume* vol, uint32_t* block)
{
    uint32_t total_blocks = vol->super_block.flt_entry_count;
    for (uint32_t i = 0; i < total_blocks; i++)
    {
        uint32_t flt_entry;
        FWDIOERR(fs_read_flt(vol, i, &flt_entry));
        if (flt_entry == FS_FLT_FREE || flt_entry == FS_FLT_DELETED)
        {
            *block = i;
            return FS_RESULT_OK;
        }
    }
    return FS_RESULT_NO_SPACE;
}

FS_FUN_INTERNAL int fs_find_fileentry(
    FS_Volume* vol, uint32_t* block, uint32_t* index, const char* name)
{
    const uint32_t entries_per_block =
        vol->super_block.data_block_size / sizeof(FS_FileEntry);
    uint32_t current_block = vol->super_block.root_block_index;

    for (;;)
    {
        // Check all entries in the current block
        for (uint32_t i = 0; i < entries_per_block; i++)
        {
            FS_FileEntry entry;
            FWDIOERR(fs_read_fileentry(vol, current_block, i, &entry));

            if (entry.magic != FS_FILEMAGIC_FILE) continue;

            if (memcmp(entry.name, name, strlen(name)) == 0)
            {
                *block = current_block;
                *index = i;
                return FS_RESULT_OK;
            }
        }

        // Get the next block in the root directory chain
        uint32_t next_block;
        FWDIOERR(fs_read_flt(vol, current_block, &next_block));

        // Check for broken chain or end of chain
        if (next_block == FS_FLT_END_OF_CHAIN) break;
        if (next_block == FS_FLT_FREE || next_block == FS_FLT_DELETED)
            return FS_RESULT_BROKEN_FLT_CHAIN;

        current_block = next_block;
    }

    return FS_RESULT_NOT_FOUND;
}

FS_FUN_INTERNAL int fs_create_fileentry(
    FS_Volume* vol, uint32_t* block, uint32_t* index)
{
    const uint32_t entries_per_block =
        vol->super_block.data_block_size / sizeof(FS_FileEntry);

    uint32_t current_block = vol->super_block.root_block_index;

    for (;;)
    {
        // Check all entries in the current block
        for (uint32_t i = 0; i < entries_per_block; i++)
        {
            FS_FileEntry entry;
            FWDIOERR(fs_read_fileentry(vol, current_block, i, &entry));

            // If magic doesn't mark a file, we have an empty entry
            if (entry.magic != FS_FILEMAGIC_FILE)
            {
                *block = current_block;
                *index = i;
                return FS_RESULT_OK;
            }
        }

        // Get the next block in the root directory chain
        uint32_t next_block;
        FWDIOERR(fs_read_flt(vol, current_block, &next_block));

        // Check for broken chain
        if (next_block == FS_FLT_FREE || next_block == FS_FLT_DELETED)
            return FS_RESULT_BROKEN_FLT_CHAIN;

        // Allocate a new block if needed
        if (next_block == FS_FLT_END_OF_CHAIN)
        {
            FWDERR(fs_find_free_block(vol, &next_block));
            FWDIOERR(fs_clear_block(vol, next_block));
            FWDIOERR(fs_write_flt(vol, current_block, next_block));
            FWDIOERR(fs_write_flt(vol, next_block, FS_FLT_END_OF_CHAIN));
        }

        current_block = next_block;
    }

    return FS_RESULT_OK;
}

FS_FUN_INTERNAL int fs_clear_flt_chain(FS_Volume* vol, uint32_t block)
{
    for (;;)
    {
        // Read and clear the current FLT entry
        uint32_t flt_entry;
        FWDIOERR(fs_read_flt(vol, block, &flt_entry));
        FWDIOERR(fs_write_flt(vol, block, FS_FLT_FREE));

        vol->used_data_blocks--;

        // Check if it was the last entry in the chain
        if (flt_entry == FS_FLT_END_OF_CHAIN) break;

        // Check for broken chain
        if (flt_entry == FS_FLT_FREE || flt_entry == FS_FLT_DELETED)
            return FS_RESULT_BROKEN_FLT_CHAIN;

        // Move to the next block in the chain
        block = flt_entry;
    }

    return FS_RESULT_OK;
}

FS_FUN_API int fs_volume_mount(FS_Volume* volume, const char* filepath)
{
    if (!volume || !filepath) return FS_RESULT_INVALID_PARAMETER;
    FILE* f = fopen(filepath, "r+b");
    if (!f) return FS_RESULT_MOUNT_ERROR;
    volume->file_handle = f;

    // Read superblock
    int res = fs_read_superblock(volume, &volume->super_block);
    if (res != FS_RESULT_OK)
    {
        fclose(f);
        return res;
    }

    // Initialize other volume parameters
    FWDERR(fs_count_used_blocks(volume, &volume->used_data_blocks));
    FWDERR(fs_list_len(volume, &volume->file_entry_count));
    FWDERR(fs_list_reset(volume));

    return FS_RESULT_OK;
}

FS_FUN_API int fs_volume_unmount(FS_Volume* volume)
{
    if (!volume || !volume->file_handle) return FS_RESULT_INVALID_PARAMETER;
    fclose(volume->file_handle);
    volume->file_handle = NULL;
    return FS_RESULT_OK;
}

FS_FUN_API int fs_volume_create(
    const char* path, uint32_t block_size, uint32_t volume_bytes)
{
    const char* magic = FS_SUPERBLOCK_MAGIC;

    // Check parameters
    if (!path || block_size < sizeof(FS_FileEntry) ||
        volume_bytes < sizeof(FS_SuperBlock) + block_size * 2)
        return FS_RESULT_INVALID_PARAMETER;

    // Check if the block size is a power of two
    if ((block_size & (block_size - 1)) != 0)
        return FS_RESULT_INVALID_PARAMETER;

    // Create and open the file
    FILE* f = fopen(path, "w+b");
    if (!f) return FS_RESULT_MOUNT_ERROR;

    // Clear the file
    const uint32_t zero = 0xffffffff;
    for (uint32_t i = 0; i < volume_bytes / sizeof(uint32_t); i++)
    {
        if (fwrite(&zero, sizeof(uint32_t), 1, f) != 1)
        {
            fclose(f);
            return FS_RESULT_IO_ERROR;
        }
    }

    // Calculate volume parameters
    const uint32_t flt_entry_count_initial =
        (volume_bytes - sizeof(FS_SuperBlock)) /
        (block_size + sizeof(FS_FLTEntry));
    const uint32_t flt_offset_initial = sizeof(FS_SuperBlock);
    const uint32_t flt_end_initial =
        flt_offset_initial + flt_entry_count_initial * sizeof(FS_FLTEntry);
    const uint32_t data_block_offset =
        flt_end_initial / block_size * block_size +
        ((flt_end_initial % block_size) ? block_size : 0);
    const uint32_t data_block_count =
        (volume_bytes - data_block_offset) / block_size;
    const uint32_t flt_entry_count = data_block_count;
    const uint32_t flt_offset = flt_offset_initial;

    // Create and write the superblock
    FS_SuperBlock sb;
    memcpy(sb.magic, magic, 8);
    sb.data_block_size = block_size;
    sb.flt_entry_count = flt_entry_count;
    sb.data_block_count = data_block_count;
    sb.flt_offset = flt_offset;
    sb.data_block_offset = data_block_offset;
    sb.root_block_index = 0;
    if (fs_generic_write(f, 0, &sb, sizeof(FS_SuperBlock)) != FS_RESULT_OK)
    {
        fclose(f);
        return FS_RESULT_IO_ERROR;
    }

    // Mark the first data block (root directory) as end of chain in FLT
    const uint32_t flt_entry = FS_FLT_END_OF_CHAIN;
    if (fs_generic_write(f, flt_offset, &flt_entry, sizeof(uint32_t)) != 0)
    {
        fclose(f);
        return FS_RESULT_IO_ERROR;
    }

    // Close the file and return
    fclose(f);
    return FS_RESULT_OK;
}

FS_FUN_API int fs_file_size(
    FS_Volume* volume, const char* filename, uint32_t* size_bytes)
{
    // Check parametters
    if (!volume || !filename || !size_bytes)
        return FS_RESULT_INVALID_PARAMETER;
    uint32_t block, index;

    // Find and read the file entry
    FS_FileEntry entry;
    FWDERR(fs_find_fileentry(volume, &block, &index, filename));
    FWDIOERR(fs_read_fileentry(volume, block, index, &entry));

    // Return the file size
    *size_bytes = entry.data_byte_count;
    return FS_RESULT_OK;
}

FS_FUN_API int fs_file_read(FS_Volume* volume, const char* filename,
    void* buffer, uint32_t buffer_size, uint32_t* bytes_read)
{
    // Check parameters
    if (!volume || !filename || !buffer || !bytes_read)
        return FS_RESULT_INVALID_PARAMETER;

    // Find and read the file entry
    FS_FileEntry entry;
    uint32_t block, index;
    FWDERR(fs_find_fileentry(volume, &block, &index, filename));
    FWDIOERR(fs_read_fileentry(volume, block, index, &entry));

    // Read the file data
    uint32_t bytes_remaining = (buffer_size < entry.data_byte_count)
                                   ? buffer_size
                                   : entry.data_byte_count;
    uint32_t current_block = entry.data_block_index;
    *bytes_read = 0;

    while (bytes_remaining > 0)
    {
        const uint32_t block_size = volume->super_block.data_block_size;
        uint32_t chunk_size =
            (bytes_remaining < block_size) ? bytes_remaining : block_size;

        // Read a chunk and update counters
        FWDIOERR(fs_read_block(volume, current_block,
            ((uint8_t*)buffer + *bytes_read), chunk_size));
        *bytes_read += chunk_size;
        bytes_remaining -= chunk_size;

        // Get the next block in the FLT chain
        uint32_t next_block;
        FWDIOERR(fs_read_flt(volume, current_block, &next_block));

        if (next_block == FS_FLT_END_OF_CHAIN && bytes_remaining > 0)
            return FS_RESULT_BROKEN_FLT_CHAIN;
        if (next_block == FS_FLT_FREE || next_block == FS_FLT_DELETED)
            return FS_RESULT_BROKEN_FLT_CHAIN;

        current_block = next_block;
    }

    return FS_RESULT_OK;
}

FS_FUN_API int fs_file_write(FS_Volume* volume, const char* filename,
    const void* data, uint32_t data_size, uint32_t* bytes_written)
{
    // Check parameters
    if (!volume || !filename || !data || !bytes_written)
        return FS_RESULT_INVALID_PARAMETER;

    // Check if the file exists
    FS_FileEntry entry;
    uint32_t block, index;
    if (fs_find_fileentry(volume, &block, &index, filename) !=
        FS_RESULT_NOT_FOUND)
        return FS_RESULT_FILE_EXISTS;

    // Check if there's enough space for the file
    const uint32_t block_size = volume->super_block.data_block_size;
    uint32_t required_blocks = data_size / block_size;
    if (data_size % block_size != 0) required_blocks++;
    required_blocks++;  // For safety, if we need to create a new entry
    if (volume->used_data_blocks + required_blocks >
        volume->super_block.data_block_count)
        return FS_RESULT_NO_SPACE;

    // Get the first empty block
    uint32_t current_block;
    fs_find_free_block(volume, &current_block);
    // TODO Find better solution
    // Mark the end of chain to avoid the search assuming it's empty
    FWDIOERR(fs_write_flt(volume, current_block, FS_FLT_END_OF_CHAIN));

    // Create the entry
    FWDERR(fs_create_fileentry(volume, &block, &index));
    memset(&entry, 0, sizeof(FS_FileEntry));
    entry.magic = FS_FILEMAGIC_FILE;
    strncpy(entry.name, filename, sizeof(entry.name));
    entry.data_block_index = current_block;
    entry.data_byte_count = data_size;
    FWDIOERR(fs_write_fileentry(volume, block, index, &entry));

    // Store the file
    uint32_t bytes_remaining = (data_size < entry.data_byte_count)
                                   ? data_size
                                   : entry.data_byte_count;
    *bytes_written = 0;
    while (bytes_remaining > 0)
    {
        const uint32_t block_size = volume->super_block.data_block_size;
        uint32_t chunk_size =
            (bytes_remaining < block_size) ? bytes_remaining : block_size;

        // Mark the block as used
        volume->used_data_blocks++;

        // Write a chunk and update counters
        FWDIOERR(fs_write_block(volume, current_block,
            ((uint8_t*)data + *bytes_written), chunk_size));
        *bytes_written += chunk_size;
        bytes_remaining -= chunk_size;

        // If we stored the whole file, mark the end in FLT
        if (!bytes_remaining)
        {
            FWDIOERR(
                fs_write_flt(volume, current_block, FS_FLT_END_OF_CHAIN));
            break;
        }

        // Find the next block and store it to FLT
        uint32_t next_block;
        // Mark the end of chain to avoid the search assuming it's empty
        // TODO Find better solution
        FWDIOERR(fs_write_flt(volume, current_block, FS_FLT_END_OF_CHAIN));
        FWDERR(fs_find_free_block(volume, &next_block));
        FWDIOERR(fs_write_flt(volume, current_block, next_block));
        current_block = next_block;
    }

    volume->file_entry_count++;
    return FS_RESULT_OK;
}

FS_FUN_API int fs_file_delete(FS_Volume* volume, const char* filename)
{
    // Check parameters
    if (!volume || !filename) return FS_RESULT_INVALID_PARAMETER;
    uint32_t block, index;

    // Find and read the file entry
    FS_FileEntry entry;
    FWDERR(fs_find_fileentry(volume, &block, &index, filename));
    FWDIOERR(fs_read_fileentry(volume, block, index, &entry));

    // Clear the FLT chain (also updates the used block count)
    FWDERR(fs_clear_flt_chain(volume, entry.data_block_index));

    // Clear the file entry (we don't do any wear leveling)
    memset(&entry, 0xff, sizeof(FS_FileEntry));
    FWDIOERR(fs_write_fileentry(volume, block, index, &entry));
    volume->file_entry_count--;

    return FS_RESULT_OK;
}

FS_FUN_API int fs_list_len(FS_Volume* volume, uint32_t* file_count)
{
    const uint32_t entries_per_block =
        volume->super_block.data_block_size / sizeof(FS_FileEntry);
    uint32_t current_block = volume->super_block.root_block_index;

    *file_count = 0;
    for (;;)
    {
        // Check all entries in the current block
        for (uint32_t i = 0; i < entries_per_block; i++)
        {
            FS_FileEntry entry;
            FWDIOERR(fs_read_fileentry(volume, current_block, i, &entry));

            if (entry.magic == FS_FILEMAGIC_FILE) (*file_count)++;
        }

        // Get the next block in the root directory chain
        uint32_t next_block;
        FWDIOERR(fs_read_flt(volume, current_block, &next_block));

        // Check for broken chain or end of chain
        if (next_block == FS_FLT_END_OF_CHAIN) break;
        if (next_block == FS_FLT_FREE || next_block == FS_FLT_DELETED)
            return FS_RESULT_BROKEN_FLT_CHAIN;

        current_block = next_block;
    }

    return FS_RESULT_OK;
}

FS_FUN_API int fs_list_get(FS_Volume* volume, FS_FileInfo* file_info)
{
    // Read the entry and generate the info
    FS_FileEntry entry;
    FWDIOERR(fs_read_fileentry(
        volume, volume->file_list_block, volume->file_list_index, &entry));
    strncpy((char*)&file_info->name, entry.name, sizeof(file_info->name) - 1);
    file_info->name[sizeof(file_info->name) - 1] = '\0';
    file_info->size_bytes = entry.data_byte_count;

    // Get the next entry if there is one
    const uint32_t entries_per_block =
        volume->super_block.data_block_size / sizeof(FS_FileEntry);
    uint32_t current_block = volume->file_list_block;

    for (;;)
    {
        // Check all entries in the current block
        for (uint32_t i = volume->file_list_index + 1; i < entries_per_block;
            i++)
        {
            FS_FileEntry entry;
            FWDIOERR(fs_read_fileentry(volume, current_block, i, &entry));

            if (entry.magic == FS_FILEMAGIC_FILE)
            {
                volume->file_list_block = current_block;
                volume->file_list_index = i;
                return FS_RESULT_OK;
            }
        }

        // Get the next block in the root directory chain
        uint32_t next_block;
        FWDIOERR(fs_read_flt(volume, current_block, &next_block));

        // Check for broken chain or end of chain
        if (next_block == FS_FLT_END_OF_CHAIN) return FS_RESULT_NOT_FOUND;
        if (next_block == FS_FLT_FREE || next_block == FS_FLT_DELETED)
            return FS_RESULT_BROKEN_FLT_CHAIN;

        current_block = next_block;
    }

    return FS_RESULT_OK;
}

FS_FUN_API int fs_list_reset(FS_Volume* volume)
{
    if (!volume) return FS_RESULT_INVALID_PARAMETER;

    volume->file_list_block = volume->super_block.root_block_index;
    volume->file_list_index = 0;

    return FS_RESULT_OK;
}

FS_FUN_API const char* fs_strerror(int code)
{
    switch (code)
    {
        case FS_RESULT_OK:
            return "No error";
        case FS_RESULT_ERROR:
            return "Generic error";
        case FS_RESULT_NOT_FOUND:
            return "Item not found";
        case FS_RESULT_FILE_EXISTS:
            return "File already exists";
        case FS_RESULT_NO_SPACE:
            return "No space left on device";
        case FS_RESULT_INVALID_PARAMETER:
            return "Invalid parameter";
        case FS_RESULT_MOUNT_ERROR:
            return "Failed to mount volume";
        case FS_RESULT_BROKEN_FLT_CHAIN:
            return "Broken FLT chain detected";
        case FS_RESULT_IO_ERROR:
            return "I/O error occurred";
        default:
            return "Unknown error code";
    }
}

#if FS_FLT_DUMP_ENABLE != 0
FS_FUN_API int fs_flt_dump_create(FS_Volume* vol, FS_FLTDump* dump)
{
    if (!vol || !dump) return FS_RESULT_INVALID_PARAMETER;

    // Allocate memory for dump structures
    dump->file_count = vol->file_entry_count + 1;  // +1 for root chain
    dump->file_names = malloc(dump->file_count * sizeof(const char*));
    dump->file_sizes = malloc(dump->file_count * sizeof(uint32_t));
    dump->entries =
        malloc(vol->super_block.flt_entry_count * sizeof(FS_FLTEntry));

    // Clear the usage and entries
    memset(dump->entries, 0,
        vol->super_block.flt_entry_count * sizeof(FS_FLTEntry));

    // Scan the files and build the dump
    const uint32_t block_size = vol->super_block.data_block_size;
    const uint32_t entries_per_block = block_size / sizeof(FS_FileEntry);
    uint32_t curr_block = vol->super_block.root_block_index;
    uint32_t curr_file = 1;
    for (;;)
    {
        for (uint32_t i = 0; i < entries_per_block; i++)
        {
            FS_FileEntry entry;
            FWDIOERR(fs_read_fileentry(vol, curr_block, i, &entry));
            if (entry.magic != FS_FILEMAGIC_FILE) continue;

            // Store the file name and size
            dump->file_names[curr_file - 1] = malloc(sizeof(entry.name) + 1);
            strncpy((char*)dump->file_names[curr_file - 1], entry.name,
                strlen(entry.name));
            dump->file_names[curr_file - 1][strlen(entry.name)] = '\0';
            dump->file_sizes[curr_file - 1] = entry.data_byte_count;

            // Follow the FLT chain and mark usage
            uint32_t data_block = entry.data_block_index;
            uint32_t bytes_left = entry.data_byte_count;
            for (;;)
            {
                uint32_t block_bytes =
                    bytes_left < block_size ? bytes_left : block_size;
                dump->entries[data_block] = curr_file;

                // Get the next block in the chain
                uint32_t next_block;
                FWDIOERR(fs_read_flt(vol, data_block, &next_block));
                data_block = next_block;

                // Check for broken chain or end of chain
                if (data_block == FS_FLT_FREE || data_block == FS_FLT_DELETED)
                    return FS_RESULT_BROKEN_FLT_CHAIN;
                if (data_block == FS_FLT_END_OF_CHAIN) break;

                bytes_left -= block_bytes;
            }

            curr_file++;
        }

        // Get the next block in the root directory chain
        uint32_t next_block;
        FWDIOERR(fs_read_flt(vol, curr_block, &next_block));
        if (next_block == FS_FLT_END_OF_CHAIN) break;
        if (next_block == FS_FLT_FREE || next_block == FS_FLT_DELETED)
            return FS_RESULT_BROKEN_FLT_CHAIN;
        curr_block = next_block;
    }

    // Follow the root chain
    uint32_t root_chain = dump->file_count - 1;
    uint32_t current_block = vol->super_block.root_block_index;
    dump->file_sizes[root_chain] =
        (dump->file_count - 1) * sizeof(FS_FileEntry);
    dump->file_names[root_chain] = strdup("<root_directory>");
    for (;;)
    {
        dump->entries[current_block] = root_chain + 1;
        uint32_t next_block;
        FWDIOERR(fs_read_flt(vol, current_block, &next_block));
        if (next_block == FS_FLT_END_OF_CHAIN) break;
        if (next_block == FS_FLT_FREE || next_block == FS_FLT_DELETED)
            return FS_RESULT_BROKEN_FLT_CHAIN;
        current_block = next_block;
    }

    return FS_RESULT_OK;
}

FS_FUN_API int fs_flt_dump_free(FS_FLTDump* dump)
{
    if (!dump) return FS_RESULT_INVALID_PARAMETER;

    // Free allocated memory
    for (int i = 0; i < dump->file_count; i++)
        free((void*)dump->file_names[i]);
    free(dump->file_names);
    free(dump->file_sizes);
    free(dump->entries);

    return FS_RESULT_OK;
}
#endif
